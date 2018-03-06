#include "includes.h"
#include "write_all.h"
#include "include/rpc_client.h"
#ifndef REGISTER
#define REGISTER 0
#endif

extern BOOL AllowDebugChange;
extern BOOL override_logfile;
extern BOOL in_client;
static pstring calling_name;

static int io_bufsize = 64512;

static BOOL override = False;

typedef struct smb_client_t {
    pstring server;
    pstring share;
    pstring username;
    pstring password;
    pstring file_name;
    struct cli_state *cli;
    BOOL got_pass;

    SMB_BIG_UINT size;
    uint16 mode;
    /* these times are normally kept in GMT */
    time_t change_time;    
    time_t access_time;
    time_t write_time;    
} smb_client_t;

static smb_client_t dst_client, src_client;

/* Get info from an SMB server on a file. Use a qpathinfo call first
 * and if that fails, use cli_getattrE, as Win95 sometimes refuses qpathinfo
 */
static BOOL smb_client_getatr(struct cli_state *cli, char *path, uint16 *mode,
    SMB_OFF_T *size, time_t *access_time, time_t *write_time,
    time_t *change_time)
{
    int fd, ret;

    if (cli_qpathinfo(cli, path, change_time, access_time, write_time,
	size, mode))
    {
	return True;
    }

    /* if this is Windows NT don't bother with the getatr */
    if (cli->capabilities & CAP_NT_SMBS)
	return False;

    /* Open the file */
    if ((fd = cli_open(cli, path, O_RDONLY, DENY_NONE)) < 0)
	return False;

    ret = cli_getattrE(cli, fd, mode, size, change_time, access_time,
	write_time);

    cli_close(cli, fd);

    return ret;
}

/* Set file info on an SMB server.  Use setpathinfo call first.  If that
 * fails, use setattrE..
 *
 * Access and modification time parameters are always used and must be
 * provided.  Create time, if zero, will be determined from the actual create
 * time of the file.  If non-zero, the create time will be set as well.
 *
 * "mode" (attributes) parameter may be set to -1 if it is not to be set.
 */
static BOOL smb_client_setatr(struct cli_state *cli, char *path,
    time_t access_time, time_t write_time, time_t change_time, uint16 mode)
{
    int fd, ret;

    /* First, try setpathinfo (if qpathinfo succeeded), for it is the
     * modern function for "new code" to be using, and it works given a
     * filename rather than requiring that the file be opened to have its
     * attributes manipulated.
     */
    if (!cli_setpathinfo(cli, path, 0, access_time, write_time, change_time,
	mode))
    {
	/*
	 * setpathinfo is not supported; go to plan B. 
	 *
	 * cli_setatr() does not work on win98, and it also doesn't
	 * support setting the access time (only the modification
	 * time), so in all cases, we open the specified file and use
	 * cli_setattrE() which should work on all OS versions, and
	 * supports both times.
	 */

	/* Open the file */
	if ((fd = cli_open(cli, path, O_RDWR, DENY_NONE)) < 0)
	    return -1;

	/* Set the new attributes */
	ret = cli_setattrE(cli, fd, change_time, access_time, write_time);

	/* Close the file */
	cli_close(cli, fd);

	/*
	 * Unfortunately, setattrE() doesn't have a provision for
	 * setting the access mode (attributes).  We'll have to try
	 * cli_setatr() for that, and with only this parameter, it
	 * seems to work on win98.
	 */
	if (ret && mode != (uint16) -1)
	{
	    d_printf("do_cp: setattrE failed \n");
	    ret = cli_setatr(cli, path, mode, 0);
	}

	if (!ret)
	    return False;
    }

    return True;
}

static int copy_data(int src_fd, int dst_fd)
{
    int rc = 1, read_size = io_bufsize;
    off_t nread = 0;
    char *data;

    if (!(data = (char *)SMB_MALLOC(read_size)))
    { 
	d_printf("malloc fail for size %d\n", read_size);
	goto Exit;
    }

    /* Copy the data */
    while (1)
    {
	int n, ret;

	n = cli_read(src_client.cli, src_fd, data, nread, read_size);

	if (n <= 0)
	    break;

	ret = cli_write(dst_client.cli, dst_fd, 0, data, nread, n);

	if (n != ret)
	{
	    d_printf("Error writing file: %s\n", cli_errstr(dst_client.cli));
	    goto Exit;
	} 

	nread += n;
    }

    if (nread < src_client.size)
    {
	d_printf("Short read when getting file %s. Only got %ld bytes.\n",
	    dst_client.file_name, (long)nread);
	goto Exit;
    }

    rc = 0;

Exit:
    SAFE_FREE(data);
    return rc;
}

static int is_same_file(smb_client_t *f1, smb_client_t *f2)
{
    return !strcmp(f1->server, f2->server) &&
	!strcmp(f1->share, f2->share) &&
	!strcmp(f1->file_name, f2->file_name);
}

static int do_cp(void)
{
    int dst_fd = -1, src_fd = -1, rc = 1;

    /* Check if dst file already exist */
    if (!cli_qpathinfo2(dst_client.cli, dst_client.file_name, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL))
    {
	if (!NT_STATUS_EQUAL(cli_nt_error(dst_client.cli),
	    NT_STATUS_OBJECT_NAME_NOT_FOUND))
	{
	    d_printf("Failed to retrieve info for %s in %s (Error:%s)\n",
		dst_client.file_name, dst_client.server,
		cli_errstr(dst_client.cli));
	    goto Exit;
	}
    }
    else if (!override)
    {
	/* File already exist */
	d_printf("File %s in %s already exist\n", dst_client.file_name,
	    dst_client.server);
	goto Exit;
    }

    /* Get src file info */
    if (!smb_client_getatr(src_client.cli, src_client.file_name,
	&src_client.mode, (SMB_OFF_T *)&src_client.size,
	&src_client.access_time, &src_client.write_time,
	&src_client.change_time))
    {
	d_printf("Failed to retrieve info for %s in %s (Error:%s)\n",
	    src_client.file_name, src_client.server,
	    cli_errstr(src_client.cli));
	goto Exit;
    }

    DEBUG(4, ("%s in %s info: mode: %d, size: %.0f, access_time: %ld,"
	"change_time: %ld, write_time: %ld\n", src_client.file_name,
	src_client.server, src_client.mode, (double)src_client.size,
	src_client.access_time, src_client.change_time, src_client.write_time));

    if (src_client.mode & aDIR)
    {
	d_printf("Can not copy directory '%s'. smbcp utility can copy files "
	    "only\n", src_client.file_name);
	goto Exit;
    }

    /* Open the new file */
    if ((dst_fd = cli_open(dst_client.cli, dst_client.file_name,
	O_RDWR|O_CREAT|O_TRUNC, DENY_NONE)) < 0)
    {
	d_printf("Failed to open destination file %s on server %s (Error:%s)\n",
	    dst_client.file_name, dst_client.server,
	    cli_errstr(dst_client.cli));
	goto Exit;
    }

    /* Copy attribs from src file to dst file */
    if (!smb_client_setatr(dst_client.cli, dst_client.file_name,
	 src_client.access_time, src_client.write_time, src_client.change_time,
	 src_client.mode))
    {
	d_printf("Failed to set attribs for %s in %s (Error:%s)\n",
	    dst_client.file_name, dst_client.server,
	    cli_errstr(dst_client.cli));
	goto Exit;
    }

    if ((src_fd = cli_open(src_client.cli, src_client.file_name, O_RDONLY,
	DENY_NONE)) < 0)
    {
	d_printf("Failed to open source file %s on server %s (Error:%s)\n",
	    src_client.file_name, src_client.server,
	    cli_errstr(src_client.cli));
	goto Exit;
    }

    rc = copy_data(src_fd, dst_fd);

Exit:

    if (dst_fd != -1 && !cli_close(dst_client.cli, dst_fd))
    {
	d_printf("Failed to close file %s in %s (Error:%s)\n",
	    dst_client.file_name, dst_client.server,
	    cli_errstr(dst_client.cli));
	rc = 1;
    }

    if (src_fd != -1 && !cli_close(src_client.cli, src_fd))
    {
	d_printf("Failed to close file %s in %s (Error:%s)\n",
	    src_client.file_name, src_client.server,
	    cli_errstr(src_client.cli));

	rc = 1;
    }

    return rc;
}

static int smb_client_open_connection(smb_client_t *smb_client)
{
    int rc = 0;

    /* XXX: The smbclient utility opens a connection to only one host while
     * the smbcp utility opens 2 connections: one to the src host and one to
     * the dst host. Therefore, we need to update the global 'username' and
     * 'password' before opening the connection */

    cmdline_auth_info.use_kerberos = False;
    cmdline_auth_info.got_pass = smb_client->got_pass;
    cmdline_auth_info.signing_state = Undefined;
    pstrcpy(cmdline_auth_info.username, smb_client->username);
    if (*smb_client->password)
	pstrcpy(cmdline_auth_info.password, smb_client->password);
    cli_cm_set_credentials(&cmdline_auth_info);

    if (!(smb_client->cli = cli_cm_open(smb_client->server, smb_client->share,
	True)))
    {
	d_printf("Failed to open connection to '%s' and share '%s'\n",
	    smb_client->server, smb_client->share);
	rc = 1;
	goto Exit;
    }

Exit:
    return rc;
}

static char CLI_DIRSEP_STR[] = { '\\', '\0' };

static void smb_client_build_full_file_path(smb_client_t *smb_client,
    pstring dir, pstring file_name)
{
    if (!*dir)
	pstrcpy(dir, "/");

    string_sub(dir, "/", "\\", 0);
    pstrcat(dir,CLI_DIRSEP_STR);
    pstrcat(dir, file_name);

    string_sub(dir, "\\\\", "\\", 0);

    pstrcpy(smb_client->file_name, dir);
}

static int parse_params(int argc, char *argv[])
{
    int opt, rc = -1;
    poptContext pc;
    pstring src_dir, dst_dir, src_file, dst_file;    
    struct poptOption long_options[] = {
	POPT_AUTOHELP

	{ "src-srv", 'I', POPT_ARG_STRING, NULL, 'I', "source server IP",
	    "SERVER" },
	{ "dst-srv", 'i', POPT_ARG_STRING, NULL, 'i', "destination server IP",
	    "SERVER" },
	{ "src-usr", 'U', POPT_ARG_STRING, NULL, 'U', "source host usrname",
	    "USERNAME" },
	{ "dst-usr", 'u', POPT_ARG_STRING, NULL, 'u',
	    "destination host usrname", "USERNAME" },
	{ "src-password", 'P', POPT_ARG_STRING, NULL, 'P',
	    "source host password", "PASSWORD" },
	{ "dst-password", 'p', POPT_ARG_STRING, NULL, 'p',
	    "destination host password", "PASSWORD" },
	{ "src-file", 'F', POPT_ARG_STRING, NULL, 'F', "source file", "FILE" },
	{ "dst-file", 'f', POPT_ARG_STRING, NULL, 'f', "destination file",
	    "FILE" },
	{ "src-dir", 'T', POPT_ARG_STRING, NULL, 'T', "source directory",
	    "DIR" },
	{ "dst-dir", 't', POPT_ARG_STRING, NULL, 't', "destination directory",
	    "DIR" },
	{ "src-share", 'R', POPT_ARG_STRING, NULL, 'R', "source share",
	    "SHARE" },
	{ "dst-share", 'r', POPT_ARG_STRING, NULL, 'r', "destination share",
	    "SHARE" },
	{ "src-no-pass", 'N', POPT_ARG_NONE, &src_client.got_pass, 0,
	    "Don't ask for a password for source host" },
	{ "dst-no-pass", 'n', POPT_ARG_NONE, &dst_client.got_pass, 0,
	    "dst Don't ask for a password for destination host" },
	{ "override", 'O', POPT_ARG_NONE, &override, 0,
	    "Override if file exist" },
	POPT_COMMON_SAMBA
	POPT_TABLEEND
    };

    src_dir[0] = 0;
    dst_dir[0] = 0;
    src_file[0] = 0;
    dst_file[0] = 0;

    /* skip argv(0) */
    pc = poptGetContext("smbcp", argc, (const char **) argv, long_options, 0);

    while ((opt = poptGetNextOpt(pc)) != -1) {

	switch (opt) {
	case 'U':
	    pstrcpy(src_client.username, poptGetOptArg(pc));
	    break;
	case 'u':
	    pstrcpy(dst_client.username, poptGetOptArg(pc));
	    break;
	case 'P':
	    pstrcpy(src_client.password, poptGetOptArg(pc));
	    src_client.got_pass = True;
	    break;
	case 'p':
	    pstrcpy(dst_client.password, poptGetOptArg(pc));
	    dst_client.got_pass = True;	    
	    break;
	case 'F':
	    pstrcpy(src_file, poptGetOptArg(pc));
	    break;
	case 'f':
	    pstrcpy(dst_file, poptGetOptArg(pc));
	    break;
	case 'T':
	    pstrcpy(src_dir, poptGetOptArg(pc));
	    break;
	case 't':
	    pstrcpy(dst_dir, poptGetOptArg(pc));
	    break;
	case 'I':
	    pstrcpy(src_client.server, poptGetOptArg(pc));
	    break;
	case 'i':
	    pstrcpy(dst_client.server, poptGetOptArg(pc));
	    break;
	case 'R':
	    pstrcpy(src_client.share, poptGetOptArg(pc));
	    break;
	case 'r':
	    pstrcpy(dst_client.share, poptGetOptArg(pc));
	    break;
	}
    }

    if (!*src_client.username)
	pstrcpy(src_client.username, "GUEST");		
    if (!*dst_client.username)
	pstrcpy(dst_client.username, "GUEST");
    if (!*dst_client.share || !*src_client.share || !*src_file ||
	!*src_client.server || !*dst_client.server)
    {
	d_printf("One of the fields is missing.\n");
	poptPrintUsage(pc, stderr, 0);
	goto Exit;
    }
    if (!*dst_file)
	pstrcpy(dst_file, src_file);

    smb_client_build_full_file_path(&src_client, src_dir, src_file);
    smb_client_build_full_file_path(&dst_client, dst_dir, dst_file);

    if (is_same_file(&src_client, &dst_client))
    {
	d_printf("%s and %s are the same file\n", src_client.file_name,
	    dst_client.file_name);
	goto Exit;
    }

    rc = 0;

Exit:
    poptFreeContext(pc);
    return rc;
}

int smbcp_main(int argc,char *argv[])
{
    fstring new_workgroup;
    pstring term_code;
    int rc = 1;

    load_case_tables();

#ifdef KANJI
    pstrcpy(term_code, KANJI);
#else /* KANJI */
    *term_code = 0;
#endif /* KANJI */

    /* initialize the workgroup name so we can determine whether or 
       not it was set by a command line option */

    set_global_myworkgroup("");
    set_global_myname("");

    /* set default debug level to 0 regardless of what smb.conf sets */
    setup_logging("smbcp", True);
    DEBUGLEVEL_CLASS[DBGC_ALL] = 1;
    if ((dbf = x_fdup(x_stderr)))
	x_setbuf(dbf, NULL);

    in_client = True;   /* Make sure that we tell lp_load we are */

    if (parse_params(argc, argv))
	goto Exit;

    /*
     * Don't load debug level from smb.conf. It should be
     * set by cmdline arg or remain default (0)
     */
    AllowDebugChange = False;

    /* save the workgroup...

       FIXME!! do we need to do this for other options as well 
       (or maybe a generic way to keep lp_load() from overwriting 
       everything)?  */

    fstrcpy(new_workgroup, lp_workgroup());
    pstrcpy(calling_name, global_myname());

    if (override_logfile)
	setup_logging(lp_logfile(), False);

    if (!lp_load(dyn_CONFIGFILE, True, False, False, True))
    {
	fprintf(stderr, "%s: Can't load %s - run testparm to debug it\n",
	    argv[0], dyn_CONFIGFILE);
    }

    load_interfaces();

    if (strlen(new_workgroup) != 0)
	set_global_myworkgroup(new_workgroup);

    if (strlen(calling_name) != 0)
	set_global_myname(calling_name);
    else
	pstrcpy(calling_name, global_myname());

    init_names();

    if (smb_client_open_connection(&dst_client))
	goto Exit;
    if (smb_client_open_connection(&src_client))
	goto Exit;

     rc = do_cp();

Exit:
    cli_cm_shutdown();
    return rc;
}
