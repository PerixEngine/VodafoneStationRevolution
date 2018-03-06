#include "includes.h"
#include "write_all.h"
#include "include/rpc_client.h"
#ifndef REGISTER
#define REGISTER 0
#endif

#include <rg_smb_err.h>

int smbcp_main(int argc,char *argv[]);
int smbclient_main(int argc,char *argv[]);

extern NTSTATUS g_rg_nt_err;

static int nt_stat_to_rg_err(NTSTATUS nt_err)
{
    if (NT_STATUS_V(nt_err) == NT_STATUS_V(NT_STATUS_DIRECTORY_NOT_EMPTY))
	return SMB_ERR_DIR_NOT_EMPTY;
    else if (NT_STATUS_V(nt_err) == NT_STATUS_V(NT_STATUS_NOT_A_DIRECTORY))
	return SMB_ERR_NOT_DIR;
    else if (NT_STATUS_V(nt_err) == NT_STATUS_V(NT_STATUS_NO_SUCH_FILE))
	return SMB_ERR_NO_SUCH_FILE;
    else if (NT_STATUS_V(nt_err) == NT_STATUS_V(NT_STATUS_ACCESS_DENIED))
	return SMB_ERR_ACCESS_DENIED;
    else if (NT_STATUS_V(nt_err) == NT_STATUS_V(NT_STATUS_LOGON_FAILURE))
	return SMB_ERR_ACCESS_DENIED;
    else if (NT_STATUS_V(nt_err) == NT_STATUS_V(NT_STATUS_WRONG_PASSWORD))
	return SMB_ERR_ACCESS_DENIED;
    else if (NT_STATUS_V(nt_err) == 
	NT_STATUS_V(NT_STATUS_NETWORK_ACCESS_DENIED))
    {
	return SMB_ERR_NETWORK_ACCESS_DENIED;
    }
    else if (NT_STATUS_V(nt_err) == 
	NT_STATUS_V(NT_STATUS_OBJECT_PATH_NOT_FOUND))
    {
	return SMB_ERR_OBJECT_NOT_FOUND;
    }
    else if (NT_STATUS_V(nt_err) == 
	NT_STATUS_V(NT_STATUS_OBJECT_NAME_NOT_FOUND))
    {
	return SMB_ERR_OBJECT_NOT_FOUND;
    }
    else if (NT_STATUS_V(nt_err) == NT_STATUS_V(NT_STATUS_BAD_NETWORK_NAME))
	return SMB_ERR_OBJECT_NOT_FOUND;
    else if (NT_STATUS_V(nt_err) == NT_STATUS_V(NT_STATUS_CANNOT_DELETE))
	return SMB_ERR_CANNOT_DELETE;
    else if (NT_STATUS_V(nt_err) == 
	NT_STATUS_V(NT_STATUS_OBJECT_NAME_COLLISION))
    {
	return SMB_ERR_OBJECT_COLLISION;
    }
    else if (NT_STATUS_V(nt_err) == NT_STATUS_V(NT_STATUS_DISK_FULL))
	return SMB_ERR_DISK_FULL;
    else if (NT_STATUS_V(nt_err) == NT_STATUS_V(NT_STATUS_HOST_UNREACHABLE))
	return SMB_ERR_HOST_UNREACHABLE;

    return SMB_ERR_UNKNOWN;
}

int main(int argc,char *argv[])
{
    int ret;

    if (strstr(argv[0], "smbclient"))
	ret = smbclient_main(argc, argv);
    else
	ret = smbcp_main(argc, argv);

    return ret ? nt_stat_to_rg_err(g_rg_nt_err) : 0;
}
