/****************************************************************************
 *
 * rg/pkg/build/config_host.c
 * 
 * Copyright (C) Jungo LTD 2004
 * 
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General 
 * Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02111-1307, USA.
 *
 * Developed by Jungo LTD.
 * Residential Gateway Software Division
 * www.jungo.com
 * info@jungo.com
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "str.h"

#include "config_opt.h"
#include "create_config.h"
#include <rg_version.h>

/* List of CONFIGs:
 * TODO Move all CONFIGs to use a global char *
 */
static char *JMKE_BUILDDIR, *JMK_ROOT, *CCACHE, *TOOLS_PREFIX, *TOOLCHAIN_ROOT, 
    *PATH;

/* Primitive version of sys_exec (to avoid compiling pkg/util/sys.c) */
static int sys_exec(char *cmd)
{
    int ret = -1;
    char *s = sys_get(&ret, cmd);

    str_free(&s);
    return ret;
}


static void install_linux_package(char *pkg, char *dscr)
{
    fprintf(stderr, "Package %s: %s\n", pkg, dscr);
    fprintf(stderr, "To install %s, open a command prompt as root and:\n", pkg);
    fprintf(stderr, "Debian/Knoppix:\n"
	   "   Type:\n"
	   "   apt-get install %s\n", pkg);
    fprintf(stderr, "RedHat/Mandrake/Fedora/Suse:\n"
	   "   Download/locate %s-x.y.z-arch.rpm package.\n"
	   "   To find this package you can google '%s rpm'.\n"
	   "   After downloading, type:\n"
	   "   rpm -Uvh %s-x.y.z-arch.rpm\n", pkg, pkg, pkg);
}

static void conf_ostype(void)
{
    char *ostype = NULL;
    int ret;

    /* Host OS: Set CONFIG_RG_HOST_xxx */
    ostype = sys_get(&ret, "echo $OSTYPE");
    if (ret)
	conf_err("$OSTYPE not defined\n");

    /* the gcc has built-in */
    if (!strcmp(ostype, "cygwin"))
    {
	token_set_y("CONFIG_RG_HOST_CYGWIN");
	token_set_y("CONFIG_RG_WINDOWS_HOST"); /* backwards compatibility */
    }
    else if (!strncmp(ostype, "linux", 5))
	token_set_y("CONFIG_RG_HOST_LINUX");
    else
	conf_err("OS '%s' not supported ($OSTYPE)\n", ostype);
    str_free(&ostype);
}

static void conf_os_path(void)
{
    /* OS-dependent directory name translation */
    if (token_get("CONFIG_WINDOWS_HOST"))
	token_set("OS_PATH", "\"$(shell cygpath -w $(1))\"");
    else
	token_set("OS_PATH", "$(1)");
}

static void conf_endian_flags(void)
{
    char *t;

    if (!(t = token_get_str("TARGET_ENDIANESS")))
	conf_err("Can't find TARGET_ENDIANESS");
    
    if (!strcmp(t,"BIG"))
    {
	token_set("ENDIAN_LDFLAGS", "-EB");
	if (token_get("CONFIG_CPU_MIPS32") ||
	    token_get("CONFIG_CPU_MIPS32_R2") ||
	    token_get("CONFIG_CAVIUM_OCTEON"))
	{
	    token_set("ENDIAN_CFLAGS", "-meb");
	}
	else
	    token_set("ENDIAN_CFLAGS", "-mbig-endian");
    }
    else if (!strcmp(t,"LITTLE"))
    {
	if (token_get("CONFIG_CPU_MIPS32"))
	    token_set("ENDIAN_CFLAGS", "-mel");
    }
    else
	conf_err("Illegal TARGET_ENDIANESS: %s", t);
}

static void conf_toolchain(void)
{
    /* Toolchain */
    str_catprintf(&TOOLCHAIN_ROOT, "/usr/local/openrg");
    token_set("TOOLCHAIN_ROOT", "%s", TOOLCHAIN_ROOT);
    token_set("I386_TARGET_MACHINE", "i686-jungo-linux-gnu");
    token_set("I386_TOOLS_PREFIX", "%s/%s", TOOLCHAIN_ROOT,
	token_get_str("I386_TARGET_MACHINE"));
}

static void conf_target_machine(void)
{
    if (!token_get_str("ARCH"))
    {
	printf("conf_target_machine: Error: ARCH not defined.\n");
	exit(1);
    }

    if (!strcmp(token_get_str("ARCH"), "i386"))
	token_set("TARGET_MACHINE", token_get_str("I386_TARGET_MACHINE"));
    if (!strcmp(token_get_str("ARCH"), "x86"))
	token_set("TARGET_MACHINE", "i686-jungo-linux-gnu");
    if (!strcmp(token_get_str("ARCH"), "um"))
	token_set("TARGET_MACHINE", "i686-jungo-linux-gnu");
    if (token_get("CONFIG_BCM963XX") || token_get("CONFIG_BCM63XX"))
	token_set("TARGET_MACHINE", "mipseb-broadcom-linux-uclibc");
    if (token_get("CONFIG_BROADCOM_9636X") ||
	token_get("CONFIG_BROADCOM_9636X_3X"))
    {
	token_set("TARGET_MACHINE", "mips-linux-uclibc");
    }
    if (token_get("CONFIG_BCM947XX") || token_get("CONFIG_P400_REF"))
	token_set("TARGET_MACHINE", "mipsel-jungo-linux-gnu");
    if (token_get("CONFIG_ARMNOMMU"))
	token_set("TARGET_MACHINE", "arm-jungo-linux-gnu");
    if (token_get("CONFIG_COMCERTO"))
    {
	if (token_get("CONFIG_COMCERTO_COMMON_821XX"))
	    token_set("TARGET_MACHINE", "armv6j-jungo-linux-gnu");

	token_set("TARGET_PREFIX", "armv5l-jungo-linux-gnu");
    }

    if (token_get("CONFIG_LANTIQ_XWAY"))
	token_set("TARGET_MACHINE", "mips-linux-uclibc");

    if (token_get("CONFIG_CAVIUM_OCTEON"))
	token_set("TARGET_MACHINE", "mips64-octeon-linux-gnu");

    if (token_get("CONFIG_ARCH_AVALANCHE"))
	token_set("TARGET_MACHINE", "arm_v6_be_uclibc");

    if (!token_get_str("TARGET_PREFIX"))
	token_set("TARGET_PREFIX", token_get_str("TARGET_MACHINE"));
}

static void check_toolchain_exists(char *tc_path)
{
    struct stat st;
    if (stat(tc_path, &st))
    {
	char *path = NULL;
	char *msg = NULL;

	str_printf(&path, "%s/pkg/jpkg/install/install.sh", JMK_ROOT);
	str_printf(&msg, "Cannot find toolchain %s\n", tc_path);
	if (!stat(path, &st))
	{   
	    str_catprintf(&msg, "Run the following to install the toolchain:\n"
		"./pkg/jpkg/install/install.sh --location %s\n", tc_path);
	}

	conf_err(msg);
    }	
}

static void check_toolchain_version(char *tc_path, int needed_tc_version)
{
    /* check Jungo toolchain version */
    char *tc_ver_cmd = NULL;
    int tc_ver = atoi(sys_get(NULL, *str_printf(&tc_ver_cmd,
	"cat %s/version", tc_path)));
    char *msg = NULL;

    if (tc_ver < needed_tc_version)
    {
	str_printf(&msg, "Installed toolchain version is %d, needed %d\n",
	    tc_ver, needed_tc_version);
	str_catprintf(&msg, "You Need to reinstall the toolchain.\n");
	str_catprintf(&msg, "Run the following to install the toolchain:\n"
	    "./pkg/jpkg/install/install.sh --location %s\n", tc_path);
	conf_err(msg);
    }
    str_free(&tc_ver_cmd);
}

static void conf_tools_prefix(void)
{
    char *toolchain_name = strdup("");
    char *external_tc;

    /* TOOLS_PREFIX might have been already set in the command line */
    if (!(TOOLS_PREFIX = token_get_str("TOOLS_PREFIX")))
    {

	if (token_get("CONFIG_COMCERTO"))
	    str_printf(&toolchain_name, "%s-2", token_get_str("TARGET_PREFIX"));

	if (token_get("CONFIG_ARCH_AVALANCHE"))
	    str_printf(&toolchain_name, "armv6b-montavista-linux-uclibceabi");
	
	if (token_get("CONFIG_LANTIQ_XWAY"))
	    str_printf(&toolchain_name, "mips-xway-5_1_1-linux-uclibc");

	if (token_get("CONFIG_BROADCOM_9636X") ||
	    token_get("CONFIG_BROADCOM_9636X_3X"))
	{
	    str_printf(&toolchain_name, "mips-brcm-linux-uclibc");
	}
	
	if (str_isempty(toolchain_name))
	    toolchain_name = strdup(token_get_str("TARGET_PREFIX"));

	str_printf(&TOOLS_PREFIX, "%s/%s", TOOLCHAIN_ROOT, toolchain_name);

	token_set("TOOLS_PREFIX", TOOLS_PREFIX);
    }

    check_toolchain_exists(TOOLS_PREFIX);
    check_toolchain_exists(token_get_str("I386_TOOLS_PREFIX"));
    
    if (!strcmp(toolchain_name, "armv5b-jungo-linux-gnu"))
	check_toolchain_version(TOOLS_PREFIX, 20090303);
    else if (!strcmp(toolchain_name, "powerpc-jungo-linux-gnu"))
	check_toolchain_version(TOOLS_PREFIX, 20060801);
    else if (!strcmp(toolchain_name, "armv5l-jungo-linux-gnu-2"))
	check_toolchain_version(TOOLS_PREFIX, 20061019);
    else if (!strcmp(toolchain_name, "mipseb-jungo-linux-gnu"))
	check_toolchain_version(TOOLS_PREFIX, 20061024);
    else if (!strcmp(toolchain_name, "mips64-octeon-linux-gnu"))
	check_toolchain_version(TOOLS_PREFIX, 20090323);

    if ((external_tc = token_get_str("CONFIG_RG_EXTERNAL_TOOLS_PATH")))
	check_toolchain_exists(external_tc);

    token_set("CROSS_COMPILE", "%s/bin/%s-", TOOLS_PREFIX,
	token_get_str("TARGET_PREFIX"));

    str_free(&toolchain_name);
}

static char *libc_detect(char **libc_path)
{
    struct libc_scan {
	char *prefix;
	int major_from, major_to;
	int minor_from, minor_to;
	int sub_from, sub_to;
	int extra_from, extra_to;
	char *token;
    } libcs[] = {
	{"libc-", 2, 2, 1, 9, 0, 9, 0, 0, "CONFIG_GLIBC"},
	{"libuClibc-", 0, 0, 9, 9, 20, 40, 0, 5, "CONFIG_ULIBC"},
    };
    int i, major, minor, sub, extra;
    
    for (i = 0; i < sizeof(libcs) / sizeof (struct libc_scan); i++)
    {
	struct libc_scan *libc = &libcs[i];
	for (major = libc->major_from; major <= libc->major_to; major++)
	{
	    for (minor = libc->minor_from; minor <= libc->minor_to; minor++)
	    {
		for (sub = libc->sub_from; sub <= libc->sub_to; sub++)
		{
		    for (extra = libc->extra_from; extra <= libc->extra_to; 
			extra++)
		    {
			int is_found;
			char *libc_name = NULL, *sub_extra_str = NULL;
			struct stat st;

			/* sub and extra can be missing */
			if (sub > 0)
			{
			    if (extra > 0)
				str_printf(&sub_extra_str, ".%d.%d", sub, extra);
			    else
				str_printf(&sub_extra_str, ".%d", sub);
			}
			str_printf(&libc_name, "%s%d.%d%s.so", libc->prefix,
			    major, minor, str_nonull(sub_extra_str));
			*libc_path = sys_get(NULL, "%s -print-file-name=%s",
			    token_get_str("TARGET_CC"), libc_name);
			str_trim(libc_path);

			is_found = !lstat(*libc_path, &st);

			str_free(&libc_name);
			str_free(&sub_extra_str);

			if (is_found)
			    return libc->token;
		    }
		}
	    }
	}
    }

    str_free(libc_path);
    return NULL;
}

static void libc_path_set(char *libcso_path)
{
    char *libc_dir, *libcso_dir;
    
    libcso_dir = sys_get(NULL, "dirname %s", libcso_path);
    token_set("LIBC_IN_TOOLCHAIN_PATH_SO", strtrim(libcso_dir));
    free(libcso_dir);

    /* crt1.o, crti.o and crtn.o are needed for rg_gcc and are sometimes
     * located in a different directory 
     */
    libc_dir = sys_get(NULL, "dirname `%s -print-file-name=crt1.o`",
	token_get_str("TARGET_CC"));
    token_set("LIBC_IN_TOOLCHAIN_PATH_CRT", strtrim(libc_dir));
    token_set("LIBC_IN_TOOLCHAIN_PATH_INC", "%s/../include", strtrim(libc_dir));
    free(libc_dir);

    printf("Detected LIBC libc.so at: '%s'\n",
	token_getz("LIBC_IN_TOOLCHAIN_PATH_SO"));
    printf("Detected LIBC crt1.o at: '%s'\n",
	token_getz("LIBC_IN_TOOLCHAIN_PATH_CRT"));
    printf("Detected LIBC include at: '%s'\n",
	token_getz("LIBC_IN_TOOLCHAIN_PATH_INC"));
}

static void conf_libc(void)
{
    char *libc_detected = NULL;
    char *libc_config = NULL;
    char *libc_path = NULL;
    char *libc_default = NULL;

    if (!token_get("CONFIG_RG_NOT_UNIX"))
	libc_default = "CONFIG_ULIBC";
    else
	return;

    if (token_get("CONFIG_ULIBC") && token_get("CONFIG_GLIBC"))
    {
	if (!token_get("CONFIG_RG_JPKG_BIN"))
	{
	    conf_err("Both 'CONFIG_ULIBC' and 'CONFIG_GLIBC', "
		"please remove one to define LIBC\n");
	}
	/* binary JPKG - assume everything is well defined in this case */
	goto Path_set;
    }
    else if (token_get("CONFIG_ULIBC") || token_get("CONFIG_GLIBC"))
    {
	libc_config =
	    token_get("CONFIG_ULIBC") ? "CONFIG_ULIBC" : "CONFIG_GLIBC";
    }

    if (!strcmp(token_getz("LIBC_IN_TOOLCHAIN"), "n"))
    {
	/* Default LIBC is ulibc, when not specified */
	if (!libc_config)
	{
	    token_set_y(libc_default);
	    printf("Forcing LIBC to default: '%s' "
		"(remove LIBC_IN_TOOLCHAIN=n to autodetect LIBC version)\n",
		libc_default);
	}
	else
	{
	    printf("Forcing use of specified LIBC: '%s' "
		"(remove LIBC_IN_TOOLCHAIN=n to autodetect LIBC version)\n",
		libc_config);
	}
	return;
    }

    /* LIBC_IN_TOOLCHAIN == y or NULL*/

    libc_detected = libc_detect(&libc_path);

    if (libc_config)
    {
	if (!libc_detected)
	{
	    printf("Using specified LIBC: '%s' "
		"(failed to detect precompiled LIBC version)\n", libc_config);
	}
	else if (strcmp(libc_config, libc_detected))
	{
	    if (!strcmp(token_getz("LIBC_IN_TOOLCHAIN"), "y"))
	    {
		conf_err("Autodetected precompiled LIBC version is '%s', "
		    "but specified version is '%s'. "
		    "Please remove '%s'=y to use autodetected LIBC version\n",
		    libc_detected, libc_config, libc_config);
	    }
	    else
	    {
		printf("Using specified LIBC: '%s' "
		    "(instead of detected precompiled LIBC version:%s (%s))\n",
		    libc_config, libc_detected, libc_path);
	    }
	}
	else
	{
	    token_set_y("LIBC_IN_TOOLCHAIN");
	    printf("Using specified and autodetected precompiled LIBC: '%s' "
		"(%s)\n", libc_config, libc_path);

	    if (!strcmp(libc_config, "CONFIG_GLIBC"))
		token_set_y("GLIBC_IN_TOOLCHAIN");
	    else
		token_set_y("ULIBC_IN_TOOLCHAIN");
	}
    }
    else
    {
	if (!libc_detected)
	{
	    /* Forced to use precompiled LIBC from toolchain, but
	     * but we couldn't detect it and we dont have explicit version */
	    if (!strcmp(token_getz("LIBC_IN_TOOLCHAIN"), "y"))
	    {
		conf_err("Failed to detect precompiled LIBC version "
		    "in toolchain. Please install correct toolchain or "
		    "remove LIBC_IN_TOOLCHAIN=y to autodetect LIBC version\n");
	    }
	    else
	    {
		token_set_y(libc_default);
		printf("Using default LIBC: '%s' "
		    "(failed to detect precompiled LIBC version)\n",
		    libc_default);
	    }
	}
	else
	{
	    token_set_y(libc_detected);
	    token_set_y("LIBC_IN_TOOLCHAIN");
	    printf("Using autodetected precompiled LIBC: '%s' (%s)\n",
		libc_detected, libc_path);

	    if (!strcmp(libc_detected, "CONFIG_GLIBC"))
		token_set_y("GLIBC_IN_TOOLCHAIN");
	    else
		token_set_y("ULIBC_IN_TOOLCHAIN");
	}
    }

Path_set:
    if ((token_get("CONFIG_GLIBC") && token_get("GLIBC_IN_TOOLCHAIN")) || 
	(token_get("CONFIG_ULIBC") && token_get("ULIBC_IN_TOOLCHAIN")))
    {
	if (!libc_path && !libc_detect(&libc_path))
	    conf_err("Fail to find path of libc in the toolchain\n");

	libc_path_set(libc_path);
    }

    str_free(&libc_path);
}

static void conf_ccache(void)
{
    char *ccache = NULL;
    char *ccache_ver = NULL;
    char *s = NULL;
    int ret;

    str_alloc(&CCACHE);
    str_cpy(&ccache, token_getz("CONFIG_RG_CCACHE"));
    printf("Detecting ccache: ");
    if (!strcmp(ccache, "n"))
    {
	printf("disabled\n");
	goto Exit;
    }
    if (!*ccache || !strcmp(ccache, "y"))
    {
	/* autodetect ccache */
        str_trim(str_use(&ccache, sys_get(&ret, "which ccache")));
	if (ret || str_isempty(ccache))
	{
	    printf("not found\n");
	    goto Exit;
	}
    }
    /* check ccache exists */
    str_cpy(&ccache_ver, ccache);
    str_cat(&ccache_ver, " --version");
    str_use(&s, sys_get(NULL, ccache_ver));
    if (!strstr(s, "ccache"))
	conf_err("failed executing ccache (%s)", ccache);
    
    str_printf(&CCACHE, "%s ", ccache);
    printf("found %s (use CONFIG_RG_CCACHE=n to disable)\n", CCACHE);

Exit:
    str_free(&s);
    str_free(&ccache);
    str_free(&ccache_ver);
}

static void conf_cc(void)
{
    /* CC */
    token_set("CC", "%s/pkg/build/rg_gcc", JMKE_BUILDDIR);

    /* CXX */
    token_set("CPP", "%s -E", token_get_str("CC"));
    if (token_get("CONFIG_UCLIBCXX"))
	token_set("CXX", "%s/pkg/uclibc++/bin/g++-uc", JMKE_BUILDDIR);
    else
    {
	token_set("CXX", "%s%s/bin/%s-g++", CCACHE, TOOLS_PREFIX, 
	    token_get_str("TARGET_PREFIX"));
    }
}

static int file_exist(char *file)
{
    struct stat st;
    return !lstat(file, &st);
}

/* Return:
 * 0 - Do not do version check
 * 1 - Do version check.
 */
static int debian_check(void)
{
    if (!token_get("CONFIG_DEBIAN_DPKG_VER_CHECK"))
    {
	printf("Skipping by user request\n");

	return 0;
    }

    if (!file_exist("/etc/debian_version"))
    {
	printf("Debian not detected on this machine, skipping " \
	    "package check\n");

	return rg_error(LEXIT, "In order to override please unset "
	    "CONFIG_DEBIAN_DPKG_VER_CHECK");
    }

    return 1;
}

/* Verify debian package is installed. */
#define DPKG_COMPARE "dpkg --compare-versions %s %s %s"
static int dpkg_verify(char *dpkg, char *lower_version, char *upper_version)
{
    char *buf = NULL;
    char *ver;
    int ret = 0;

    printf("Detecting debian package %s... ", dpkg);
    if (!debian_check())
	return 0;

    ver = sys_get(NULL, *str_printf(&buf, "dpkg -s %s | "
	"grep Version | awk -F: '{print $NF}'", dpkg));
    str_trim(&ver);
    if (str_isempty(ver) ||
	sys_exec(*str_printf(&buf, DPKG_COMPARE, ver, "ge", lower_version)) ||
	sys_exec(*str_printf(&buf, DPKG_COMPARE, ver, "lt", upper_version)))
    {
	printf("\n");
	rg_error(LEXIT,
	    "Required package %s is not installed or not in the correct "
	    "version range (between %s and %s).\n"
	    "Please install %s using the following apt-get command:\n"
	    "# apt-get install %s\n", dpkg, lower_version, upper_version, dpkg,
	    dpkg);
	ret = -1;
    }
    else
	printf("found\n");

    str_free(&buf);
    str_free(&ver);
    return ret;
}

static void conf_cc_for_build(void)
{
    char *local_gcc = NULL;
    char *local_cxx = NULL;
    if (token_get("CONFIG_RG_USE_LOCAL_TOOLCHAIN"))
    {
	local_gcc = sys_get(NULL, "which cc");
	dpkg_verify("g++", "4.3.1", "4.5");

	if (str_isempty(local_gcc))
	{
	    local_gcc = sys_get(NULL, "which gcc");
	    if (str_isempty(local_gcc))
		rg_error(LEXIT, "Internal error - failed locating cc or gcc");
	}
	str_trim(&local_gcc);

	local_cxx = sys_get(NULL, "which g++");
	if (str_isempty(local_cxx))
	    rg_error(LEXIT, "Internal error - failed locating g++");

	str_trim(&local_cxx);
    }
    else
    {
	str_printf(&local_gcc, "%s/bin/%s-gcc", 
	    token_get_str("I386_TOOLS_PREFIX"),
	    token_get_str("I386_TARGET_MACHINE"));
	str_printf(&local_cxx, "%s/bin/%s-g++", 
	    token_get_str("I386_TOOLS_PREFIX"),
	    token_get_str("I386_TARGET_MACHINE"));
    }

    token_set("CC_FOR_BUILD", "%s%s -DCC_FOR_BUILD "
	"-I%s/pkg/include -DCONFIG_HAS_MMU",
	CCACHE, local_gcc, JMKE_BUILDDIR);

    token_set("CXX_FOR_BUILD", "%s%s -DCC_FOR_BUILD "
	"-I%s/pkg/include -DCONFIG_HAS_MMU",
	CCACHE, local_cxx, JMKE_BUILDDIR);

    str_free(&local_gcc);
    str_free(&local_cxx);
}

static void conf_target_cc(void)
{
    char *t = NULL;
    
    token_set("TARGET_CC", "%s%s/bin/%s-gcc", CCACHE, TOOLS_PREFIX,
	token_get_str("TARGET_PREFIX"));

    token_set("TARGET_CPP", "%s%s/bin/%s-gcc -E", CCACHE, TOOLS_PREFIX,
	token_get_str("TARGET_PREFIX"));

    token_set("TARGET_CXX", "%s%s/bin/%s-g++", CCACHE, TOOLS_PREFIX,
	token_get_str("TARGET_PREFIX"));

    token_set("REAL_GCC", "%s", token_get_str("TARGET_CC"));

    if (sys_exec(*str_printf(&t, "%s --version", token_get_str("TARGET_CC"))))
    {
	conf_err("Cannot find cross-compiler for target (%s)",
	    token_get_str("TARGET_CC"));
    }
    str_free(&t);
}

static void conf_gcc_version(void)
{
    char *bn;
    char *gcc = token_get_str("TARGET_CC");
    char *ver = sys_get(NULL, "%s -dumpversion", gcc);
    char *libgcc = sys_get(NULL, "%s -print-libgcc-file-name", gcc);
    char *incl_fixed = sys_get(NULL, "%s -print-file-name=include-fixed", gcc);
    int major, minor;

    if (!ver || !libgcc)
	conf_err("Cannot configure GCC_VERSION and/or LIB_GCC_DIR_PREFIX");

    /* find out GCC version */
    if ((bn = strchr(ver, '\n')))
	*bn = '\0';
    token_set("GCC_VERSION", ver);

    sscanf(ver, "%d.%d", &major, &minor);
    token_set("GCC_VERSION_NUM", itoa(major * 1000 + minor));

    /* find out LIB-GCC prefix
     * .../lx4189-uclibc/bin/../lib/gcc-lib/lx4189-uclibc/3.3.4/libgcc.a -> 
     * .../lx4189-uclibc/bin/../lib/gcc-lib/lx4189-uclibc/3.3.4
     */
    if ((bn = strchr(libgcc, '\n')))
	*bn = '\0';
    if ((bn = strrchr(libgcc, '/')) && !strcmp(bn, "/libgcc.a"))
	*bn = '\0';
    else
	conf_err("Cannot configure LIB_GCC_DIR_PREFIX");
    token_set("LIB_GCC_DIR_PREFIX", libgcc);

    printf("Detecting target GCC: found %s (version: %s)\n", gcc, ver);
    printf("Detecting target libgcc: found at %s\n", libgcc);

    str_trim(&incl_fixed);
    if (file_exist(incl_fixed))
	token_set_y("GCC_HAS_INCLUDE_FIXED");

    str_free(&ver);
    str_free(&libgcc);
    str_free(&incl_fixed);
}

static void conf_host_tools(void)
{
    char postfix[256] = "", prefix[256] = "";

    if (token_get("CONFIG_WINDOWS_HOST"))
    {
	sprintf(postfix, "i386");
	sprintf(prefix, "%s/bin/", TOOLS_PREFIX);
    }
    else if (!token_get("CONFIG_RG_USE_LOCAL_TOOLCHAIN"))
    {
	sprintf(prefix, "%s/bin/%s-",
	    token_get_str("I386_TOOLS_PREFIX"),
	    token_get_str("I386_TARGET_MACHINE"));
    }

    token_set("HOST_AR", "%sar%s", prefix, postfix);
    token_set("HOST_LD", "%sld%s", prefix, postfix);
    token_set("HOST_AS", "%sas%s", prefix, postfix);
    token_set("HOST_NM", "%snm%s", prefix, postfix);
    token_set("HOST_RANLIB", "%sranlib%s", prefix, postfix);
    token_set("HOST_STRIP", "%sstrip%s", prefix, postfix);
    token_set("HOST_SIZE", "%ssize%s", prefix, postfix);
    token_set("HOST_OBJCOPY", "%sobjcopy%s", prefix, postfix);
    token_set("HOST_OBJDUMP", "%sobjdump%s", prefix, postfix);
}

static void conf_tools(void)
{
    char postfix[256] = "", prefix[256];

    if (token_get("CONFIG_WINDOWS_HOST"))
    {
	sprintf(postfix, "%s", token_get_str("ARCH"));
	sprintf(prefix, "%s/bin/", TOOLS_PREFIX);
    }
    else
    {
	sprintf(prefix, "%s/bin/%s-", TOOLS_PREFIX,
	    token_get_str("TARGET_PREFIX"));
    }

    token_set("AR", "%sar%s", prefix, postfix);
    token_set("LD", "%sld%s", prefix, postfix);
    token_set("AS", "%sas%s", prefix, postfix);
    token_set("NM", "%snm%s", prefix, postfix);
    token_set("RANLIB", "%sranlib%s", prefix, postfix);
    token_set("STRIP", "%sstrip%s", prefix, postfix);
    token_set("SIZE", "%ssize%s", prefix, postfix);
    token_set("OBJCOPY", "%sobjcopy%s", prefix, postfix);
    token_set("OBJDUMP", "%sobjdump%s", prefix, postfix);
}

static void conf_os_target(void)
{
    token_set("PROD_LINUX_TARGET", "bzImage.initrd");
    token_set("OS_TARGET", "%s", token_get_str("PROD_LINUX_TARGET"));
}

static void conf_mklibs(void)
{
    /* mklibs is python utility which use to reduce shared lib size.
     * we need to check if python installed and mklibs installed
     */
    int r;
    char *missing_apps = NULL;
    int have_python=1;
    int have_mklibs=1;

    r = sys_exec("python -V 2>/dev/null");

    printf("Detecting python: %sfound\n", r ? "Not " : "");

    if (r)
    {
	have_python=0;
	missing_apps = "'python' and 'mklibs'";
	goto Exit;
    }

    r = sys_exec("mklibs --version 2>/dev/null");

    printf("Detecting mklibs: %sfound\n", r ? "Not " : "");

    if (r )
    {
	have_mklibs=0;
	missing_apps = "mklibs";
    }

Exit:
    if (!missing_apps || !token_get("CONFIG_RG_MKLIBS"))
	return;
    fprintf(stderr,
	"OpenRG build uses 'python' and 'mklibs' for footprint reduction.\n"
	"Please install %s. Alternatively, you may add\n"
	"CONFIG_RG_MKLIBS=n to your 'make config' command line to avoid \n"
	"using mklibs. ", missing_apps); 
    if (!have_python)
    {
	install_linux_package("python",
	    "advaned pattern scanning and processing language");
    }
    if (!have_mklibs)
    {
	install_linux_package("mklibs",
	    "utility to reduce shared libs");
    }
    rg_error(LEXIT, "Exiting due to above error/s.\n");
}

static void conf_gawk(void)
{
    /* gawk is used in makefiles of linux kernel 2.6, but it is not always
     * installed on linux hosts.
     */

    int r = sys_exec("gawk --version 2>/dev/null");

    printf("Detecting gwak: %sfound\n", r ? "Not " : "");

    if (r && token_get("CONFIG_RG_OS_LINUX_26"))
    {
	fprintf(stderr, 
	    "Linux 2.6 compilation requires installation of 'gawk'\n");
	install_linux_package("gawk",
	    "pattern scanning and processing language");
	rg_error(LEXIT, "Please install gawk.\n");
    }
}

static void conf_dc(void)
{
    /* dc is used in cramfs checksum calculation
     * (in pkg/kernel/linux/boot/Makefile), and it is not always
     * installed on linux hosts.
     */

    int r = sys_exec("dc --version 2>/dev/null");

    printf("Detecting dc: %sfound\n", r ? "Not " : "");

    if (r)
    {
	fprintf(stderr, "OpenRG compilation requires installation of 'dc'\n");
	install_linux_package("dc",
	    "The GNU dc arbitrary precision reverse-polish calculator");
	rg_error(LEXIT, "Please install dc.\n");
    }
}

static void conf_mksquashfs(void)
{
    char *ver;
    float vernum;
    int found;

    if (strcmp(token_getz("CONFIG_RG_MAINFS_TYPE"), "squashfs"))
	return;

    ver = sys_get(NULL, "PATH=\"$PATH:/usr/sbin\"; mksquashfs -version 2>&1");

    /* The output (to stderr) should contain: 'mksquashfs version x.y',
     * ver 4.x and up is required */
    found = sscanf(ver, "mksquashfs version %f", &vernum) == 1 && vernum >= 4;

    printf("Detecting mksquashfs: %sfound\n", found ? "" : "Not ");

    if (!found)
    {
	install_linux_package("squashfs-tools",
	    "squashfs filesystems creation tool");
	rg_error(LEXIT, "Please install squashfs-tools.\n");
    }
    str_free(&ver);
}

static void conf_xz(void)
{
    const char *xz_ver_output = "xz (XZ Utils)";
    char *ver;
    int found;

    if (!token_get("CONFIG_RG_XZ_COMPRESSED_KERNEL"))
	return;

    ver = sys_get(NULL, "PATH=\"$PATH:/usr/sbin\"; xz --version 2>&1");

    /* The output (to stderr) should contain: 'xz (XZ Utils)' */
    found = !strncmp(ver, xz_ver_output, strlen(xz_ver_output));

    printf("Detecting xz: %sfound\n", found ? "" : "Not ");

    if (!found)
    {
	install_linux_package("xz-utils",
	    "XZ-format compression utilities");
	rg_error(LEXIT, "Please install xz-utils.\n");
    }
    str_free(&ver);
}

static void conf_lex_and_yacc(void)
{
    char *bison_ver = sys_get(NULL,
	"bison --version 2>/dev/null | head -n 1");
    char *lex_ver = sys_get(NULL,
	"flex --version 2>/dev/null | head -n 1");
    int flex_installed = 1;
    
    printf("Detecting bison: ");
    if (!bison_ver || !*bison_ver)
    {
	printf("Not found.\n");

	if (
	    token_get("CONFIG_ATM") ||
	    token_get("CONFIG_RG_BLUETOOTH") ||
	    token_get("CONFIG_RG_E2FSPROGS") ||
	    token_get("CONFIG_RG_FLEX") ||
	    token_get("CONFIG_RG_FTP_SERVER") ||
	    token_get("CONFIG_RG_GDBSERVER") ||
	    token_get("CONFIG_RG_KERBEROS") ||
	    token_get("CONFIG_RG_IPROUTE2") ||
	    token_get("CONFIG_RG_MAIL_SERVER") ||
	    token_get("CONFIG_RG_NETKIT") ||
	    token_get("CONFIG_PCMCIA") ||
	    token_get("CONFIG_RG_FILESERVER") ||
	    token_get("CONFIG_RG_STAR") ||
	    token_get("CONFIG_RG_USAGI") 
	   )
	{
	    install_linux_package("bison",
		"a general-purpose parser generator");
	    rg_error(LEXIT, "Please install bison.\n");
	}
    }
    str_chomp(&bison_ver);
    printf("found %s.\n", bison_ver);
    token_set("YACC", "bison -y");

    printf("Detecting flex: ");
    str_replace(&lex_ver, "flex", "");
    str_replace(&lex_ver, "version", "");
    str_trim(&lex_ver);
    if (strchr(lex_ver,'.') == strrchr(lex_ver,'.')) 
    {
	printf("Not installed / Unknown version\n");
	flex_installed = 0;
	if (token_get("CONFIG_RG_USAGI"))
	{
	    install_linux_package("flex", "a tool for generating scanners");
	    rg_error(LEXIT, "Please install flex, or exclude IPv6 by adding"
		"\"MODULE_RG_IPV6=n\" to your make config command");
	}    
    }
    
    if (flex_installed)
    {
	char *flex_exe = NULL;
	printf("found %s\n", lex_ver);
	flex_exe = sys_get(NULL, "which flex");
	if (str_isempty(flex_exe))
	    rg_error(LEXIT, "Internal error - failed 'which flex'");
	str_trim(&flex_exe);
	token_set("LEX", flex_exe);
	str_free(&flex_exe);
    }

    str_free(&bison_ver);
    str_free(&lex_ver);
}

/* General configuration */
static void conf_host_general(void)
{
    token_set("RG_BIN", "%s/pkg/build/bin/", JMKE_BUILDDIR);
    token_set("RG_BUILD", "%s/pkg/build/", JMKE_BUILDDIR);
    token_set("RG_LIB", "%s/pkg/build/lib/", JMKE_BUILDDIR);
    token_set("RG_INCLUDE_DIR", "%s/pkg/include", JMKE_BUILDDIR);
    token_set("RG_INCLUDE_RG_LINUX_DIR", "%s/pkg/include/rg_linux",
	JMKE_BUILDDIR);
    
    token_set("SPCC", "echo");
    token_set("HOSTCC", "gcc -DCONFIG_HAS_MMU");
    token_set("HOSTCPP", "%s -E", token_get_str("HOSTCC"));
    token_set("CPP_FOR_BUILD", "%s -E", token_get_str("CC_FOR_BUILD"));

    token_set("GENROMFS", "%s/pkg/genromfs/genromfs", JMKE_BUILDDIR);

    if (token_get("CONFIG_RG_OS_LINUX_3X"))
	token_set("AUTOCONF_H", "include/generated/autoconf.h");
    else
	token_set("AUTOCONF_H", "include/linux/autoconf.h");
    token_set("AUTO_DOT_CONF", "include/config/auto.conf");

    token_set("KBUILD_VERBOSE", "1");

    /* net-tools generic Makefile */
    token_set("NET_TOOLS_MAKE_FILE", "%s/pkg/include/net-tools-config.make",
	JMK_ROOT);

    token_set("DYNAMIC_LINKER", "/lib/ld-linux.so.1");
    token_set("LZMAMK", "%s/pkg/lzma/SRC/lzma.mak", JMK_ROOT);

    /* local unit-test spawner */
    token_set("UNITTEST_SPAWNER", "%s/pkg/util/jspawn -s 20", JMKE_BUILDDIR);
    
    /* valgrind  command*/
    if (token_get("CONFIG_RG_VALGRIND_LOCAL_TARGET"))
    {
	int i;
	struct stat tmp;
	char *valgrind_cmd = NULL;
	char *valgrind_path[] = {
	    "/usr/local/openrg/i386-jungo-linux-gnu/bin/valgrind",
	    "/usr/local/openrg/bin/valgrind", NULL};
	
	/* Check if valgrind exists in path */
	for (i = 0; valgrind_path[i] && stat(valgrind_path[i], &tmp); i++);

	if (valgrind_path[i])
	{
	    rg_error(LNOTICE, "Using valgrind: %s", valgrind_path[i]);
	    str_printf(&valgrind_cmd, "%s --num-callers=10 --leak-check=yes "
		"--show-reachable=yes --demangle=no", valgrind_path[i]);
	}
	else
	{
	    rg_error(LNOTICE, "Cannot find valgrind executable. "
		"Unittests will run without valgrind");
	}

	token_set("VALGRIND_CMD", valgrind_path[i] ?  valgrind_cmd : "");

	str_free(&valgrind_cmd);
    }
    
    /* For str_iconv() */
    token_set("CONFIG_ICONV_CONST_CAST", "%s", "");
}

static void conf_target_general(void)
{
    char *external_tc;

    /* This subdir is used to debug modules */
    token_set("STATIC_MOD_DIR", "%s/os/kernel/modules_static/", JMKE_BUILDDIR);

    token_set("RG_LIBPTHREAD_VERSION", "0.8");
    token_set("NEEDED_SYMBOLS", "%s/os/linux/needed_symbols.lst",
	JMKE_BUILDDIR);
    
    if (token_get("CONFIG_ARMNOMMU"))
    {
	token_set("FLTHDR", "%s/bin/%s/-flthdr", TOOLS_PREFIX,
	    token_get_str("TARGET_MACHINE"));
    }

    /* Sometime we need a certain header file from the rg kernel. Therefore,
     * here we define a search path that will guarantee that gcc will first 
     * search the standard Linux libraries, and only if the file isn't found,
     * gcc will try rg kernel.
     */
    token_set("LOCAL_ADD_RG_KERNEL_FOR_TARGET", "-idirafter "
	"%s/os/linux/include", JMKE_BUILDDIR);

    if (token_get("CONFIG_BINFMT_FLAT"))
    {
	token_set("LDFLAT", "%s/bin/ldelf2flt", TOOLS_PREFIX);
	token_set("BINFRMT_FILE", "target_binfmt_flat.mak");
    }
    else
	token_set("BINFRMT_FILE", "target_binfmt_elf.mak");

    token_set("PERM_STORAGE_FILES_DIR", "%s/pkg/flash/", JMK_ROOT);

    /* XXX remove CONFIG_RG_EXTERNAL_TOOLS_PATH after resolving B36422 */
    if ((external_tc = token_get_str("CONFIG_RG_EXTERNAL_TOOLS_PATH")))
	str_catprintf(&PATH, ":%s/bin", external_tc);

    str_catprintf(&PATH, ":%s/bin", TOOLS_PREFIX);
    
    if (token_get("CONFIG_BOOTLDR_UBOOT"))
	str_printf(&PATH, "%s/pkg/boot/u-boot:%s", JMKE_BUILDDIR, PATH);
}

static void conf_openrg_img(void)
{
    char *img, *ldist = strdup(dist);

    /* OPENRG_IMG */
    if (token_get("CONFIG_RG_RGLOADER"))
	img = "rgloader.img";
    else if (token_get("CONFIG_LSP_DIST"))
	img = "lsp.img";
    else
	img = "openrg.img";

    token_set("OPENRG_IMG", "%s/os/%s", JMKE_BUILDDIR, img);

    /* OPENRG_RMT_UPDATE_IMG */
    if (!token_get("CONFIG_LSP_DIST"))
    {
	token_set("OPENRG_RMT_UPDATE_IMG", "%s/openrg.rmt", JMKE_BUILDDIR);
	token_set("OPENRG_RMT_UPDATE_RSA_IMG", "%s/openrg.rms", JMKE_BUILDDIR);
    }
    else
    {
	token_set("OPENRG_RMT_UPDATE_IMG", "%s/%s.rmt", *str_tolower(&ldist),
	    JMKE_BUILDDIR);
	token_set("OPENRG_RMT_UPDATE_RSA_IMG", "%s/%s.rms",
	    *str_tolower(&ldist), JMKE_BUILDDIR);
    }

    if (token_get("CONFIG_RG_DLM"))
    {
	token_set("OPENRG_RMT_UPDATE_DLM", "%s/dlm.rmt", JMKE_BUILDDIR);
	token_set("OPENRG_RMT_UPDATE_RSA_DLM", "%s/dlm.rms", JMKE_BUILDDIR);
    }

    token_set("OPENRG_PROD", "%s/os/openrg.prod", JMKE_BUILDDIR);
    token_set("BOOTLDR_IMG", "%s/pkg/bootldr/arch/boot.img", JMKE_BUILDDIR);
    token_set("FLASH_IMG", "%s/flash.img", "JMKE_BUILDDIR");
}

static void conf_prod_kernel_image(void)
{
    token_set("PROD_KERNEL_IMAGE", "%s/os/linux/vmlinux", JMKE_BUILDDIR);
}

static void conf_flash_img_offset(void)
{
    char *offset = NULL, *real_offset = NULL;

    if (token_get("CONFIG_COMCERTO"))
    {
	offset = "0x10000";
	real_offset = "0x0";
    }

    offset = offset?:"0x0";
    real_offset = real_offset?:offset;
    
    token_set("FLASH_IMG_OFFSET", "%s", offset);
    token_set("FLASH_IMG_REAL_OFFSET", "%s", real_offset);
}

static void conf_have_gc_sections(void)
{
    if (token_get("CONFIG_BCM963XX") ||
	token_get("CONFIG_COMCERTO") ||
        token_get("CONFIG_FUSIV_VX160") || token_get("CONFIG_CAVIUM_OCTEON") ||
        token_get("CONFIG_LANTIQ_XWAY") ||
	token_get("CONFIG_BROADCOM_9636X") ||
	token_get("CONFIG_BROADCOM_9636X_3X") ||
        token_get("CONFIG_RG_UML") || 
	token_get("CONFIG_ARCH_AVALANCHE"))
    {
	token_set_y("HAVE_GC_SECTIONS");
    }

}

static void conf_compressed_disk(void)
{
    token_set("BOOTLDR_COMPRESSED_DISK",
	"%s/os/linux/arch/%s/boot/bootldr_ramdisk.gz", JMKE_BUILDDIR,
	token_get_str("ARCH"));
    
    token_set("COMPRESSED_DISK", "$(DISK_IMAGE_DIR)/mainfs.img");
}

static void conf_gcc_has_size_optimize(void)
{
    token_set_y("GCC_HAS_SIZE_OPTIMIZE");
}

/* C implementation of detect_cc_feature in detect_host.sh */
static void detect_cc_feature(char *cc, char *config, char *code_h,
    char *code_c, char *type)
{
    char *t = NULL;

    str_printf(&t,
	"cat > detect_cc_features_tmp.c << DDDD\n"
	"%s\n" /* code_h */
        "\n"
	"int main(int argc, char *argv[])\n"
	"{\n"
	"    %s\n" /* code_c */
	"    return 0;\n"
        "}\n"
	"DDDD"
	, code_h, code_c);
    
    sys_exec(t);
    
    if (!sys_exec(*str_printf(&t, "%s -Wall -Werror detect_cc_features_tmp.c "
	"-o detect_cc_features_tmp.o >/dev/null 2>&1", cc)))
    {
	token_set_y(config);
    }
    sys_exec("rm -f detect_cc_features_tmp.[co]");
    str_free(&t);
}

static void conf_gcc_attribute_support(void)
{
    detect_cc_feature(token_get_str("TARGET_CC"), "HAVE_ATTRIBUTE_NONNULL",
	"void f(char *) __attribute__((__nonnull__(1)));", "", NULL);
}

static void conf_rg_cflags(void)
{
    char *f = NULL;

#if 0
    /* this is needed since the CFLAGS variable is also used by makefiles 
     * whom we have not converted to ours (e.g. pkg/ulibc), and they need 
     * access to the linux header files.
     */
    str_catprintf(&f, "-I%s/pkg/build/include ", JMKE_BUILDDIR);
#endif

    str_catprintf(&f, "-D_LIBC_REENTRANT -Wall ");

    if (!token_get("CONFIG_RG_JPKG"))
	str_catprintf(&f, "-g ");

    if (token_get("CONFIG_RG_NO_OPT"))
	str_catprintf(&f, "-O0 ");
    else if (token_get("GCC_HAS_SIZE_OPTIMIZE"))
	str_catprintf(&f, "-Os ");
    else
	str_catprintf(&f, "-O2 ");

    if (token_get("CONFIG_ARCH_AVALANCHE") && token_get("CONFIG_ARM_THUMB"))
	str_catprintf(&f, "-mthumb-interwork ");

    if (token_get("CONFIG_RG_DEV_IF_COMPAT"))
	str_catprintf(&f, "-DRG_DEV_IF_COMPAT");

    token_set("RG_CFLAGS", "%s", f);
}

static void conf_arch_cflags(void)
{
    char *f = NULL;
    
    str_printf(&f, "%s ", token_getz("ENDIAN_CFLAGS"));

    if (token_get("CONFIG_RG_UML"))
    {
	str_catprintf(&f, "-Wno-pointer-sign -Wno-format-security "
	    "-Wno-strict-overflow -Wno-enum-compare ");
    }

    if (token_get("CONFIG_RG_686"))
    {
	str_catprintf(&f, "-Wno-pointer-sign -Wno-format-security "
	    "-Wno-strict-overflow -Wno-enum-compare ");
    }

    if (token_get("CONFIG_BCM947XX"))
	str_catprintf(&f, "-mgp32 -mlong32 -march=4kc -mtune=4kc ");

    if (token_get("CONFIG_BCM963XX") || token_get("CONFIG_BCM63XX"))
	str_catprintf(&f, "-march=mips32 -mabi=32 -Wa,-32 -Wa,-march=mips32 ");

    if (token_get("CONFIG_BROADCOM_9636X") ||
	token_get("CONFIG_BROADCOM_9636X_3X"))
    {
	str_catprintf(&f, "-march=mips32 -mabi=32 -Wa,-mips32 "
	    "-Wno-pointer-sign ");
    }
    
    if (token_get("CONFIG_CAVIUM_OCTEON"))
	str_catprintf(&f, "-march=octeon -mabi=64");

    if (token_get("CONFIG_ARMNOMMU"))
    {
	str_catprintf(&f, "-fomit-frame-pointer -fno-builtin ");
	if (token_get("CONFIG_CPU_ARM7"))
	    str_catprintf(&f, "-mcpu=arm7tdmi ");
	if (token_get("CONFIG_CPU_ARM940T"))
	    str_catprintf(&f, "-mcpu=arm9tdmi ");
    }
    
    if (token_get("CONFIG_SOFT_FLOAT"))
	str_catprintf(&f, "-msoft-float ");

    if (token_get("CONFIG_COMCERTO_COMMON_821XX"))
	str_catprintf(&f, "-mapcs-32 -march=armv6j -Wa,-march=armv6j ");

    if (token_get("CONFIG_ARCH_AVALANCHE"))
    {
	str_catprintf(&f, "-mapcs -mno-sched-prolog -mabi=aapcs-linux "
	    "-march=armv6zk -mtune=arm1176jz-s -Wno-pointer-sign ");
	/* BIG NOTE:
	 * using K extensions of V6 (armv6k, armv6zk) makes gcc generate
	 * code which assumes TLS thread pointer in cp15,
	 * hence kernel MUST be compiled with CONFIG_HAS_TLS_REG
	 */
	token_set_y("CONFIG_HAS_TLS_REG");
    }

    if (token_get("CONFIG_AR9") || token_get("CONFIG_VR9"))
    {
	str_catprintf(&f, "-march=mips32r2 -fno-builtin -mabi=32 -Wa,-mips32r2 "
	    "-Wno-pointer-sign -Wno-address -Wno-attributes ");
    }

    token_set("ARCH_CFLAGS", "%s", f);
}

static void conf_rg_linux_include_cflags(void)
{
    char *arch;

    if (!(arch = token_get_str("ARCH")))
    {
	fprintf(stderr, "config_host: ARCH is not set.\n");
	exit(1);
    }

    if (token_get("CONFIG_RG_OS_LINUX_26"))
    {
	char *flags_26 = NULL;

	str_catprintf(&flags_26, "-I%s/os/linux-2.6/include ", JMKE_BUILDDIR);
	str_catprintf(&flags_26, "-I%s/os/linux-2.6/include2 ", JMKE_BUILDDIR);
	str_catprintf(&flags_26, "-I%s/os/linux-2.6/include2 ", JMK_ROOT);
	str_catprintf(&flags_26, "-I%s/os/linux-2.6/include ", JMK_ROOT);
	str_catprintf(&flags_26, "-I%s/os/linux-2.6/arch/%s ", JMKE_BUILDDIR,
	    arch);
	str_catprintf(&flags_26, "-I%s/os/linux-2.6/arch/%s/include ",
	    JMKE_BUILDDIR, arch);
	str_catprintf(&flags_26, "-I%s/os/linux-2.6/arch/%s ", JMK_ROOT, arch);
	
	if (token_get("CONFIG_RG_UML"))
	{
	    str_catprintf(&flags_26,
		"-I%s/os/linux-2.6/arch/um/include " 
		"-I%s/os/linux-2.6/arch/um/include2 "
		"-I%s/os/linux-2.6/arch/um/include "
		"-I%s/os/linux-2.6/arch/um/include/tt "
		"-I%s/os/linux-2.6/arch/um/include/skas ",
		JMKE_BUILDDIR, JMKE_BUILDDIR, JMK_ROOT, JMK_ROOT, JMK_ROOT);
	}

	if (token_get("CONFIG_BCM963XX") ||
	    token_get("CONFIG_BROADCOM_9636X"))
	{
	    str_catprintf(&flags_26,
		"-I%s/os/linux-2.6/include/asm/mach-bcm963xx ",	JMKE_BUILDDIR);
	    str_catprintf(&flags_26,
		"-I%s/os/linux-2.6/include2/asm/mach-generic ",	JMKE_BUILDDIR);
	}

	if (token_get("CONFIG_AR9") || token_get("CONFIG_VR9"))
	{
	    str_catprintf(&flags_26,
		"-I%s/os/linux-2.6/include/asm/ifx "
		"-I%s/os/linux-2.6/include2/asm/mach-generic ",
		JMKE_BUILDDIR, JMKE_BUILDDIR);
	}

	if (token_get("CONFIG_CAVIUM_OCTEON"))
	{
	    str_catprintf(&flags_26,
		"-I%s/os/linux-2.6/include/asm/mach-cavium-octeon "
		"-I%s/os/linux-2.6/include2/asm/mach-generic ",
		JMKE_BUILDDIR, JMKE_BUILDDIR);
	}

	if (token_get("CONFIG_ARCH_AVALANCHE"))
	{
	    str_catprintf(&flags_26,
		"-I%s/os/linux-2.6/include/asm/arch/puma5 "
		"-I%s/os/linux-2.6/include/asm/arch/generic ",
		JMKE_BUILDDIR, JMKE_BUILDDIR);
	}    

	token_set("RG_LINUX_26_CFLAGS", "%s", flags_26);
    }

    if (token_get("CONFIG_RG_OS_LINUX_3X"))
    {
	char *flags_3X = NULL;

	str_catprintf(&flags_3X, "-I%s/os/linux-3.x/include ", JMKE_BUILDDIR);
	str_catprintf(&flags_3X, "-I%s/os/linux-3.x/include ", JMK_ROOT);
	str_catprintf(&flags_3X, "-I%s/os/linux-3.x/arch/%s/include ",
	    JMK_ROOT, arch);
	str_catprintf(&flags_3X, "-I%s/os/linux-3.x/arch/%s/include ",
	    JMKE_BUILDDIR, arch);
	str_catprintf(&flags_3X, "-I%s/os/linux-3.x/arch/%s/include/generated ",
	    JMKE_BUILDDIR, arch);
	
	if (token_get("CONFIG_BCM63XX"))
	{
	    str_catprintf(&flags_3X,
		"-I%s/os/linux-3.x/arch/mips/include/asm/mach-bcm63xx ",
		JMK_ROOT);
	    str_catprintf(&flags_3X,
		"-I%s/os/linux-3.x/arch/mips/include/asm/mach-generic ",
		JMK_ROOT);
	}

	if (token_get("CONFIG_BROADCOM_9636X_3X"))
	{
	    str_catprintf(&flags_3X,
		"-I%s/os/linux-3.x/arch/mips/include/asm/mach-bcm963xx ",
		JMKE_BUILDDIR);
	    str_catprintf(&flags_3X,
		"-I%s/os/linux-3.x/arch/mips/include/asm/mach-generic ",
		JMK_ROOT);
	}

	if (token_get("CONFIG_RG_UML"))
	{
	    str_catprintf(&flags_3X,
		"-I%s/os/linux-3.x/arch/x86/um ", JMK_ROOT);
	    str_catprintf(&flags_3X,
		"-I%s/os/linux-3.x/arch/x86/include ", JMK_ROOT);
	    str_catprintf(&flags_3X,
		"-I%s/os/linux-3.x/arch/x86/include/generated ", JMKE_BUILDDIR);
	    str_catprintf(&flags_3X,
		"-I%s/os/linux-3.x/arch/um/include/shared ", JMK_ROOT);
	    str_catprintf(&flags_3X,
		"-I%s/os/linux-3.x/arch/x86/um/shared ", JMK_ROOT);
	    str_catprintf(&flags_3X,
		"-I%s/os/linux-3.x/arch/um/include/shared/skas ", JMK_ROOT);
	}

	token_set("RG_LINUX_3X_CFLAGS", "%s", flags_3X);
    }
}

static void conf_rg_linux_cflags(void)
{
    char *f = NULL;
    char *isystem_path;
    char *mips_long_calls;

    str_catprintf(&f, "-Wall -Wstrict-prototypes -Wno-trigraphs "
	"-fno-strict-aliasing -fno-common -pipe ");

    if (token_get("CONFIG_RG_OS_LINUX_3X"))
    {
	/* from os/3.x/Makefile */
	str_catprintf(&f, "-Werror-implicit-function-declaration "
	    "-Wno-format-security -fno-delete-null-pointer-checks ");
    }

    if (!token_get("CONFIG_RG_JPKG"))
	str_catprintf(&f, "-g ");
    
    if (!token_get("CONFIG_FRAME_POINTER"))
	str_catprintf(&f, "-fomit-frame-pointer ");

    if (token_get("CONFIG_COMCERTO_COMMON_821XX"))
    {
	str_catprintf(&f, "-Os -Uarm -mapcs-32 -D__LINUX_ARM_ARCH__=6 "
	    "-march=armv6j -Wa, -malignment-traps -msoft-float %s ",
	    token_get_str("ARCH_CFLAGS"));
    }

    if (token_get("CONFIG_RG_686"))
    {
	/* Flags are from linux-3.x/Makefile and linux-3.x/arch/x86/Makefile,
	 * with the following modifications:
	 * Not taken:
	 *   -Wundef (a common flag)
	 *   -Wdeclaration-after-statement
	 *   -Wno-pointer-sign, -fno-strict-overflow (set in ARCH_CFLAGS)
	 *   -pipe (set for all archs, above)
	 * Added:
	 *   -Wno-unused-result
	 */
	str_catprintf(&f, "%s -Wno-unused-result "
	    "-O2 -m32 -msoft-float -mregparm=3 -freg-struct-return "
	    "-mpreferred-stack-boundary=2 -march=i686 -mtune=generic "
	    "-Wa,-mtune=generic32 -ffreestanding -DCONFIG_AS_CFI=1 "
	    "-DCONFIG_AS_CFI_SIGNAL_FRAME=1 -DCONFIG_AS_CFI_SECTIONS=1 "
	    "-Wno-sign-compare -fno-asynchronous-unwind-tables "
	    " -mno-sse -mno-mmx -mno-sse2 -mno-3dnow -mno-avx "
	    " -Wframe-larger-than=2048 "
	    "-fno-stack-protector -fomit-frame-pointer  "
	    "-fconserve-stack -DCC_HAVE_ASM_GOTO ",
	    token_get_str("ARCH_CFLAGS"));
    }

    if (token_get("CONFIG_RG_UML"))
    {
	str_catprintf(&f, "-O2 -mpreferred-stack-boundary=2 -march=i686 "
	    "-ffreestanding -D__arch_um__ -Dvmap=kernel_vmap "
	    "-Din6addr_loopback=kernel_in6addr_loopback -D_LARGEFILE64_SOURCE "
	    "-Derrno=kernel_errno -Dsigprocmask=kernel_sigprocmask "
	    "-Dmktime=kernel_mktime ");

	if (token_get("CONFIG_RG_OS_LINUX_3X"))
	{
	    str_catprintf(&f, "-m32 -Din6addr_any=kernel_in6addr_any "
		" -Dstrrchr=kernel_strrchr ");
	}
	else if (token_get("CONFIG_RG_OS_LINUX_26"))
	    str_catprintf(&f, "-DSUBARCH=\"i386\" ");

	str_catprintf(&f, "%s ", token_getz("ARCH_CFLAGS"));
    }

    if (token_get("CONFIG_BCM947"))
    {
	str_catprintf(&f, "-O2 -mno-split-addresses -G 0 -mno-abicalls "
	    "-fno-pic -mlong-calls -DBCM47XX_CHOPS -DDMA %s -Wa,--trap ", 
	    token_getz("ARCH_CFLAGS"));
    }

    if (token_get("CONFIG_BCM963XX") || token_get("CONFIG_BCM63XX"))
    {
	str_catprintf(&f, "-O2 -mno-split-addresses -G 0 -mno-abicalls "
	    "-fno-pic -mlong-calls %s -Wa,--trap ", token_getz("ARCH_CFLAGS"));
    }

    if (token_get("CONFIG_MIPS_MODULES_IN_KSEG0"))
	mips_long_calls = "";
    else
	mips_long_calls = "-mlong-calls ";

    if (token_get("CONFIG_BROADCOM_9636X") ||
	token_get("CONFIG_BROADCOM_9636X_3X"))
    {
	str_catprintf(&f, "-O2 -G 0 -mno-abicalls -fno-pic %s"
	    "-ffreestanding %s -msoft-float -Wa,--trap ", mips_long_calls,
	    token_getz("ARCH_CFLAGS"));
    }

    if (token_get("CONFIG_AR9") || token_get("CONFIG_VR9"))
    {
	str_catprintf(&f, "-Os -G 0 -mno-abicalls -fno-pic -mlong-calls "
	    "-ffreestanding %s -msoft-float -Wa,--trap ",
	    token_getz("ARCH_CFLAGS"));
    }

    if (token_get("CONFIG_CAVIUM_OCTEON"))
    {
	str_catprintf(&f, "-O2 -G 0 -mlong-calls -msoft-float -mno-abicalls %s "
	    " -Wa,--trap %s -fno-builtin -fno-strict-aliasing -fno-pic "
	    "-fno-common -ffreestanding -fomit-frame-pointer -pipe ", 
	    token_getz("ARCH_CFLAGS"),
	    token_getz("OCTEON_CPPFLAGS_GLOBAL_ADD"));
    }

    str_catprintf(&f, "-D__KERNEL__ -D__TARGET__ ");

    if (token_get("CONFIG_ARCH_AVALANCHE"))
    {
	str_catprintf(&f, "-ffreestanding -O2 -fno-omit-frame-pointer "
	    "-fno-optimize-sibling-calls -mno-thumb-interwork "
	    "-D__LINUX_ARM_ARCH__=6 %s -msoft-float -Uarm ",
	    token_getz("ARCH_CFLAGS"));
    }    

    str_catprintf(&f, " %s ", token_getz("RG_LINUX_26_CFLAGS"));
    str_catprintf(&f, " %s ", token_getz("RG_LINUX_3X_CFLAGS"));

    isystem_path = sys_get(NULL, "%s -print-file-name=include",
	token_get_str("TARGET_CC"));

    token_set("JMK_RG_LINUX_CFLAGS", "%s -nostdinc -isystem %s", f,
	strtrim(isystem_path));    
    str_free(&isystem_path);
}

static void conf_linux_cflags(void)
{
    char *f = NULL;

    str_printf(&f, "%s -include %s", token_getz("ENDIAN_CFLAGS"),
	token_get("CONFIG_RG_OS_LINUX_3X") ? "linux/kconfig.h" :
	"linux/autoconf.h");
    token_set("JMK_LINUX_CFLAGS", "%s", f);
    str_free(&f);
}

static void conf_host_ldflags(void)
{
    char *f = NULL;
    /* JMK_LOCAL_LDFLAGS: 
     * Make sure we are using the ld-so and linked agaist our libs
     */
    str_printf(&f, "-L%s/pkg/lib ", JMKE_BUILDDIR);

    if (!token_get("CONFIG_RG_USE_LOCAL_TOOLCHAIN"))
    {
	str_catprintf(&f, "-Wl,--dynamic-linker,"
	    "%s/%s/lib/ld-2.9.so -Wl,-rpath,"
	    "%s/%s/lib ", 
	    token_get_str("I386_TOOLS_PREFIX"), 
	    token_get_str("I386_TARGET_MACHINE"),
	    token_get_str("I386_TOOLS_PREFIX"),
	    token_get_str("I386_TARGET_MACHINE"));
    }

    /* If using valgrind we dont want the --static flag */
    if (!token_get("CONFIG_RG_VALGRIND_LOCAL_TARGET"))
	str_catprintf(&f, "--static ");

    if (token_get("HAVE_MYSQL"))
	str_catprintf(&f, "-L%s ", token_get_str("CONFIG_RG_MYSQL_LIB_PREFIX"));

    if (token_get("CONFIG_RG_OPENSSL_LOCAL"))
    {
	str_catprintf(&f, "-L%s/lib ",
	    token_get_str("CONFIG_RG_OPENSSL_LOCAL_PREFIX"));
    }

    token_set("JMK_LOCAL_LDFLAGS", "%s", f);
    str_free(&f);
}

/* LDFLAGS are flags for GCC linkage, not flags for LD directly! */
static void conf_ldflags(void)
{
    char *f = NULL;

    str_catprintf(&f, "%s ", token_getz("ENDIAN_CFLAGS"));

    if (token_get("CONFIG_ARMNOMMU"))
	str_catprintf(&f, "-static Wl,-elf2flt ");

    if (token_get("HAVE_GC_SECTIONS") &&
	token_get("CONFIG_RG_FOOTPRINT_REDUCTION"))
    {
	str_catprintf(&f, "-Wl,--gc-sections ");
    }

    if (token_get("CONFIG_ULIBC") && !token_get("CONFIG_DYN_LINK"))
	str_catprintf(&f, "-static ");
    
    str_catprintf(&f, "-L%s/pkg/lib ", JMKE_BUILDDIR);

    token_set("LDFLAGS_ENVIR", "%s", f);

    str_free(&f);
}

static void conf_host_cflags(void)
{
    char *f = NULL;

    if (token_get("CONFIG_RG_JPKG_SRC"))
	str_catprintf(&f, "-I%s/pkg ", JMK_ROOT);

    str_catprintf(&f, "-I%s -D__HOST__ -O0 -Wall "
	"-Wstrict-prototypes -fno-strict-aliasing -Wno-pointer-sign ",
	token_get_str("RG_INCLUDE_DIR"));

    if (!token_get("CONFIG_RG_JPKG"))
	str_catprintf(&f, "-g");
    
    token_set("JMK_LOCAL_CFLAGS", "%s", f);

    str_free(&f);
}

static void conf_cflags(void)
{
    char *f = NULL;

    if (token_get("CONFIG_ARMNOMMU"))
	str_catprintf(&f, "-fpic -msingle-pic-base -D__PIC__ ");

    if (token_get("HAVE_GC_SECTIONS") &&
	token_get("CONFIG_RG_FOOTPRINT_REDUCTION"))
    {
	str_catprintf(&f, "-ffunction-sections -fdata-sections ");
    }

    str_catprintf(&f, "-Wstrict-prototypes -fno-strict-aliasing ");

    str_catprintf(&f, "%s %s -D__TARGET__ ", token_getz("RG_CFLAGS"),
	token_getz("ARCH_CFLAGS"));

    token_set("CFLAGS_ENVIR", "%s", f);
}

/* Note: Until all code will use the new jmk infrastructure, we need to have the
 * libs in both formats
 */
#define ADD_LIB(path, _lib, o_var, n_var, suffix) \
    do { \
	str_catprintf(&o_var, "-l" _lib " "); \
	str_catprintf(&n_var, "%s/%s/lib" _lib ".%s ", JMKE_BUILDDIR, path, \
	    suffix); \
    } while (0)
static void conf_openrg_lflags(void)
{
    char *suffix = token_get("CONFIG_DYN_LINK") ? "so" : "a";
    char *rg_gpl_ldlibs = NULL, *rg_gpl_libs = NULL;
    char *librg = NULL, *libsrg = NULL;

    token_set("OPENRG_LIB", "%s/pkg/util/libopenrg.%s", JMKE_BUILDDIR, suffix);

    token_set("OPENRG_GPL_LIB", "%s/pkg/util/libopenrg_gpl.%s ", JMKE_BUILDDIR,
	suffix);

    /* OPENRG_GPL_LDLIBS */
    ADD_LIB("pkg/util", "openrg_gpl", rg_gpl_ldlibs, rg_gpl_libs, suffix);

    if (token_get("CONFIG_GLIBC"))
    {
	ADD_LIB("pkg/syslog/glibc/lib", "ulibc_syslog", rg_gpl_ldlibs,
	    rg_gpl_libs, "a");
    }
    token_set("OPENRG_GPL_LDLIBS", "%s ", rg_gpl_ldlibs);
    token_set("OPENRG_GPL_LIBS", "%s ", rg_gpl_libs);

    /* LIBS */
    ADD_LIB("pkg/util", "openrg", librg, libsrg, suffix);
    ADD_LIB("pkg/util", "jutil", librg, libsrg, suffix);
    ADD_LIB("pkg/zlib", "z", librg, libsrg, "a");
    
    if (token_get("CONFIG_RG_OPENSSL"))
        ADD_LIB("pkg/openssl/ssl", "ssl", librg, libsrg, suffix);

    ADD_LIB("pkg/openssl/crypto", "crypto", librg, libsrg, suffix);

    if (token_get("CONFIG_DYN_LINK"))
    {
	if (token_get("LIBC_IN_TOOLCHAIN"))
	{
	    str_catprintf(&librg, "-ldl ");
	    str_catprintf(&libsrg, "__local_dl ");
	}
	else
	{
	    char *path = NULL;
	    str_printf(&path, "%s/lib", token_get_str("ULIBC_DIR"));
	    ADD_LIB(path, "dl", librg, libsrg, "so");
	    str_free(&path);
	}
    }

    ADD_LIB("pkg/util", "openrg", librg, libsrg, suffix);
    ADD_LIB("pkg/util", "jutil", librg, libsrg, suffix);
    ADD_LIB("pkg/util", "rg_config", librg, libsrg, suffix);

    token_set("OPENRG_LDLIBS", "%s", librg);
    token_set("OPENRG_LIBS", "%s", libsrg);

    token_set("MGT_LDLIBS", "-lmgt ");
    token_set("MGT_LIBS", "%s/pkg/mgt/lib/libmgt.a ", JMKE_BUILDDIR);

    str_free(&rg_gpl_ldlibs);
    str_free(&rg_gpl_libs);
    str_free(&librg);
    str_free(&libsrg);
}

static void conf_openrg_local_lflags(void)
{
    char *liblocal_rg = NULL, *libslocal_rg = NULL;

    if (token_get("CONFIG_OPENRG") || token_get("CONFIG_RG_RGLOADER"))
    {
	ADD_LIB("pkg/util", "local_openrg", liblocal_rg, libslocal_rg, "a");
	ADD_LIB("pkg/util", "local_jutil", liblocal_rg, libslocal_rg, "a");
	ADD_LIB("pkg/util", "local_rg_config", liblocal_rg, libslocal_rg, "a");
	ADD_LIB("pkg/zlib", "local_z", liblocal_rg, libslocal_rg, "a");
    }
    else
	ADD_LIB("pkg/util", "local_jutil", liblocal_rg, libslocal_rg, "a");

    if (token_get("CONFIG_RG_OPENSSL_COMMON"))
    {
	if (token_get("CONFIG_RG_OPENSSL"))
	{
	    ADD_LIB("pkg/openssl/ssl", "local_ssl", liblocal_rg, libslocal_rg,
		"a");
	}
	if (token_get("CONFIG_RG_CRYPTO"))
	{
	    ADD_LIB("pkg/openssl/crypto", "local_crypto", liblocal_rg,
		libslocal_rg, "a");
	}
	else
	{
	    ADD_LIB("pkg/openssl/crypto", "local_crypto", liblocal_rg,
		libslocal_rg, "a");
	    ADD_LIB("pkg/util", "local_openrg", liblocal_rg, libslocal_rg, "a");
	    ADD_LIB("pkg/util", "local_jutil", liblocal_rg, libslocal_rg, "a");
	    ADD_LIB("pkg/util", "local_rg_config", liblocal_rg, libslocal_rg,
		"a");
	}
    }
    else if (token_get("CONFIG_RG_OPENSSL_LOCAL"))
    {
	dpkg_verify("libssl0.9.8", "0.9.8g", "1.0");
	dpkg_verify("libssl-dev", "0.9.8g", "1.0");
	dpkg_verify("libcurl4-openssl-dev", "7", "8");
	str_catprintf(&liblocal_rg, "-lssl -lcrypto ");
	str_catprintf(&libslocal_rg, "__local_ssl __local_crypto ");
    }

    if (token_get("HAVE_MYSQL"))
    {
	str_catprintf(&liblocal_rg, "-lmysqlclient ");
	str_catprintf(&libslocal_rg, "__local_mysqlclient ");
    }

    token_set("OPENRG_LOCAL_LDLIBS", "%s", liblocal_rg);
    token_set("OPENRG_LOCAL_LIBS", "%s", libslocal_rg);
    token_set("MGT_LOCAL_LDLIBS", "-llocal_mgt ");
    token_set("MGT_LOCAL_LIBS", "%s/pkg/mgt/lib/liblocal_mgt.a ",
	JMKE_BUILDDIR);

    str_free(&liblocal_rg);
    str_free(&libslocal_rg);
}

static int validate_doc_tools(char *tool, char *tool_exe, char *tool_ver)
{
    int enable = 0;
    
    printf("Detecting %s: ", tool);
    if (!tool_exe || !*tool_exe)
    {
	printf("not found.\n");
	return enable;
    }

    if (!tool_ver)
    {
        enable = 1;
	printf("found\n");
	goto Exit;
    }

    str_chomp(&tool_exe);
    if (!strcmp(tool_exe, tool_ver))
    {
	enable = 1;
	printf("found %s\n", tool_ver);
    }
    else
	printf("Unknown %s version (%s)\n", tool, tool_exe);
Exit:
    return enable;
}

void conf_doctools(void)
{
    int rg_doc_enable;
    struct stat st;
    char *doc_makefile = NULL;
    char *xsltproc_exe = sys_get(NULL, "xsltproc -V 2>/dev/null");
    char *fop_exe = sys_get(NULL, 
	"fop -v 2>/dev/null  | head -n 1 | cut -d\" \" -f 3");

    rg_doc_enable = validate_doc_tools("xsltproc", xsltproc_exe, NULL);
    rg_doc_enable &= validate_doc_tools("fop", fop_exe, "0.95");
     
    printf("Detecting msttcorefonts: ");
    if (rg_doc_enable && stat("/usr/share/fonts/truetype/msttcorefonts/", &st))
    {
        rg_doc_enable = 0;
	install_linux_package("msttcorefonts", 
	    "Microsoft's TrueType core fonts");
    }
    else
        printf("found\n");

    str_printf(&doc_makefile, "%s/pkg/doc/Makefile", JMK_ROOT);
    if (rg_doc_enable && !stat(doc_makefile, &st))
	token_set_y("CONFIG_RG_DOC_ENABLED");
    str_free(&xsltproc_exe);
    str_free(&fop_exe);
    str_free(&doc_makefile);
}

static void conf_arch(void)
{
    char *arch = token_getz("ARCH");
    char *jpkg_src = token_get_str("CONFIG_RG_JPKG_SRC");
    
    if (!strcmp(arch, "arm") || jpkg_src)
	token_set_y("CONFIG_ARM");
    if (!strcmp(arch, "armnommu") || jpkg_src)
	token_set_y("CONFIG_ARMNOMMU");
    if (!strcmp(arch, "i386") || jpkg_src)
	token_set_y("CONFIG_I386");
    if (!strcmp(arch, "mips") || jpkg_src)
	token_set_y("CONFIG_MIPS");
    if (!strcmp(arch, "um") || jpkg_src)
	token_set_y("CONFIG_UM");
}

static void conf_lang_compiler(void)
{
    char *str = NULL;
    
    token_set("LANG_COMPILER", "%s/pkg/util/lang_compiler", JMKE_BUILDDIR);
    
    if (token_get("CONFIG_RG_LANG_TEST"))
	str_printf(&str, "--static ");
    
    if (token_get_str("CONFIG_RG_LANGUAGES"))
    {
	str_catprintf(&str, "--select \"%s\"",
	    token_get_str("CONFIG_RG_LANGUAGES"));
    }

    if (str)
	token_set("LANG_COMPILER_OPT", "%s", str);

    str_free(&str);
}

static void conf_text2c(void)
{
    token_set("TEXT2C", "%s/pkg/util/text2c", JMKE_BUILDDIR);
}

static char *locate_file(char *search_locations[], int num_locations,
    const char *filename)
{
    char *t = NULL;
    char *first_loc = NULL;
    int i;

    for (i = 0; i < num_locations; i++)
    {
	if (!sys_exec(*str_printf(&t, "test -f %s/%s", search_locations[i], 
	    filename)))
	{
	    if (first_loc)
	    {
		rg_error(LWARN, "%s found in both %s and in %s. Using %s\n",
		    filename, first_loc, search_locations[i], first_loc);
		break;
	    }
	    else
	    {
	        first_loc = search_locations[i];
	    }
	}
    }

    str_free(&t);
    return first_loc;
}

static void conf_mysql(void)
{
    char *t = NULL;
    const char *mysql_h_file = "include/mysql/mysql.h";
    char *search_locations[] = {"/usr/local", "/usr"};
    char *mysql_location = NULL;

    if (!token_get("HAVE_MYSQL"))
	return;

    dpkg_verify("libmysqlclient15off", "5.0", "5.1");
    dpkg_verify("libmysqlclient15-dev", "5.0", "5.1");

    mysql_location = locate_file(search_locations, 
	sizeof(search_locations) / sizeof(*search_locations), mysql_h_file);

    printf("Detecting mysql: %sfound\n", 
	mysql_location ? "" : "Not ");

    if (!mysql_location )
    {
	fprintf(stderr, 
	    "Compilation for this distribution requires installation of "
	    "'mysql'\n");
	install_linux_package("mysql",
	    "a SQL-based relational database");
	rg_error(LEXIT, "Please install mysql.\n");
    }

    token_set("CONFIG_RG_MYSQL_PREFIX", mysql_location);

    if (!sys_exec(*str_printf(&t, "test -f %s/lib/mysql/libmysqlclient.a",
	mysql_location)))
    {
	token_set("CONFIG_RG_MYSQL_LIB_PREFIX", "%s/lib/mysql", mysql_location);
    } 
    else if (!sys_exec(*str_printf(&t, "test -f %s/lib/libmysqlclient.a", 
	mysql_location)))
    {
	token_set("CONFIG_RG_MYSQL_LIB_PREFIX", "%s/lib/mysql", mysql_location);
    }
    else
    {
	rg_error(LEXIT, "Found %s/include/mysql/mysql.h, "
	    "but failed to find"
	    "  %s/lib/mysql/libmysqlclient.a or "
	    "  %s/lib/libmysqlclient.a", 
	    mysql_location, mysql_location, mysql_location);
    }

    str_free(&t);
}

static void conf_openssl(void)
{
    char *t = NULL;
    const char *openssl_h_file = "include/openssl/ssl.h";
    char *search_locations[] = {"/usr/local", "/usr"};
    char *openssl_location = NULL;

    if (!token_get("CONFIG_RG_OPENSSL") || 
	!token_get("CONFIG_RG_USE_LOCAL_TOOLCHAIN"))
    {
	return;
    }

    openssl_location = locate_file(search_locations, 
	sizeof(search_locations) / sizeof(*search_locations), openssl_h_file);

    printf("Detecting openssl: %sfound\n", 
	openssl_location ? "" : "Not ");

    if (openssl_location)
	token_set_y("CONFIG_RG_OPENSSL_LOCAL");
    else if (token_get("CONFIG_RG_USE_LOCAL_TOOLCHAIN"))
    {
        install_linux_package("libssl-dev",
            "OpenSSL libraries & include files");
        rg_error(LEXIT, "Please install libssl-dev.\n");
    }
    else
    {
	token_set_y("CONFIG_RG_OPENSSL_COMMON");
	return;
    }

    token_set("CONFIG_RG_OPENSSL_LOCAL_PREFIX", openssl_location);

    if (sys_exec(*str_printf(&t, "test -f %s/lib/libssl.so",
	openssl_location)))
    {
	rg_error(LEXIT, "Found %s/%s, "
	    "but failed to find %s/lib/libssl.so",
	    openssl_location, openssl_h_file, openssl_location);
    }

    str_free(&t);
}

static void conf_fcgi(void)
{
    char *t = NULL;
    const char *fcgi_h_file = "include/fcgi_stdio.h";
    char *search_locations[] = {"/usr/local", "/usr"};
    char *fcgi_location = NULL;

    if (!token_get("CONFIG_RG_FAST_CGI"))
	return;

    fcgi_location = locate_file(search_locations, 
	sizeof(search_locations) / sizeof(*search_locations), fcgi_h_file);

    printf("Detecting fcgi: %sfound\n", fcgi_location ? "" : "Not ");

    if (!fcgi_location)
    {
	install_linux_package("libfcgi-dev", "Header files of FastCGI");
	rg_error(LEXIT, "Please install libfcgi-dev.\n");
    }

    if (sys_exec(*str_printf(&t, "test -f %s/lib/libfcgi.a", fcgi_location)))
    {
	rg_error(LEXIT, "Found %s/%s, "
	    "but failed to find %s/lib/libfcgi.a",
	    fcgi_location, fcgi_h_file, fcgi_location);
    }

    str_free(&t);
}

static void conf_sharutils(void)
{
    if (!token_get("CONFIG_RG_JNET_SERVER"))
	return;

    dpkg_verify("sharutils", "4.6.3-1", "5.0");
}

static void conf_jrguml(void)
{
    int ret;
    char *ver;
    char *required_ver = "4.6";

    if (!token_get("CONFIG_RG_UML") || token_get("CONFIG_RG_JPKG") || 
	token_get("CONFIG_RG_GPL"))
    {
	/* In these cases, jrguml is not used: nothing to check */
	return;
    }

    ver = sys_get(&ret, "jrguml --version 2>/dev/null");

    printf("Detecting jrguml: %s%s\n", 
	ret ? "Not found" : "Found ",
	ret ? "" : ver);

    if (ret || rg_version_compare(ver, required_ver) < 0)
    {
	char *lic = getenv("LIC");
	rg_error(LEXIT,
	    "UML requires the utility jrguml from version %s or higher.\n"
    	    "To install the UML tools, make sure your tree version is %s or "
	    "higher and run:\n"
	    "$ make config DIST=HOSTTOOLS%s%s\n"
	    "$ make\n"
	    "Obtain root permission:\n"
	    "$ JUSER=$USER su\n"
	    "# make make_install_uml\n"
	    "IMPORTANT: To use the newly installed tools you must open a new "
	    "shell (to let changes in 'PATH' take effect)",
	    required_ver, required_ver, lic? " LIC=" : "", lic? lic : "");
    }
}

#define JSLINT_FULL_PATH "/usr/local/openrg/bin/jsl"

static void conf_jslint(void)
{
    /* jslint is used for syntax checking of javascript files */
    struct stat st;
    
    if (!token_get("CONFIG_RG_JSLINT"))
	return;
    
    if (stat(JSLINT_FULL_PATH, &st))
    {
	fprintf(stderr, "Compilation requires installation of 'jsl'\n");
	fprintf(stderr, "To install jsl, obtain root permissions and run:\n");
	fprintf(stderr, "jpkg -x jslint_20090702.jpkg -C /\n");
	rg_error(LEXIT, "Please install jsl.\n");
    }
    token_set("JSLINT_CMD", JSLINT_FULL_PATH);
}

#define SASSC_FULL_PATH "/usr/local/openrg/bin/sassc"

static void conf_sassc(void)
{
    struct stat st;

    if (!token_get("CONFIG_RG_SCSS_SUPPORT"))
	return;
    
    if (stat(SASSC_FULL_PATH, &st))
    {
	fprintf(stderr, "Compilation requires installation of 'sassc'\n");
	fprintf(stderr, "To install sassc, obtain root permissions and run:\n");
	fprintf(stderr, "jpkg -x sassc_20150626.jpkg -C /\n");
	rg_error(LEXIT, "Please install sassc.\n");
    }
    token_set("SASSC_CMD", SASSC_FULL_PATH);
}

static void conf_java(void)
{
    /* javac is used to compile java files, jar is used to compress them */
    if (!token_get("CONFIG_RG_OSGI"))
	return;

    if (str_isempty(sys_get(NULL, "which javac")))
	rg_error(LEXIT, "Internal error - failed locating javac");

    if (str_isempty(sys_get(NULL, "which jar")))
	rg_error(LEXIT, "Internal error - failed locating jar");
}

static void conf_parallel_make(void)
{
#ifdef B88475    
    int n, ret;
    char *p;

    /* We want to use -j <num of CPUs+1> */
    if (!(p = sys_get(&ret, "cat /proc/cpuinfo | grep processor | wc -l")) ||
	ret)
    {
	rg_error(LWARN, "Error Calculating number of processors.");
	n = 2;
    }
    else if (!(n = atoi(p)))
	n = 2;

    token_set("CONFIG_RG_PARALLEL_MAKE",  itoa(n+1));

    free(p);
#else
    token_set("CONFIG_RG_PARALLEL_MAKE", "1");
#endif    
}

void config_host(void)
{
    JMKE_BUILDDIR = getenv("JMKE_BUILDDIR");
    JMK_ROOT = getenv("JMK_ROOT");
    PATH = strdup(getenv("PATH")); /* because we later change PATH */
    conf_ostype();
    conf_os_path();
    conf_ccache();
    conf_toolchain();
    conf_cc_for_build();
    conf_host_tools();
    conf_lex_and_yacc();
    conf_host_general();
    conf_host_cflags();
    conf_arch();
    conf_doctools();
    conf_mysql();
    conf_openssl();
    conf_fcgi();
    conf_sharutils();
    conf_openrg_local_lflags();
    conf_host_ldflags();
    if (!token_get("CONFIG_RG_JPKG_SRC") && hw)
    {
	conf_gawk();
	conf_mklibs();
	conf_mksquashfs();
	conf_xz();
	conf_endian_flags();
	conf_target_machine();
	conf_tools_prefix();
	conf_cc();
	conf_target_cc();
	conf_gcc_version();
	conf_libc();
	conf_tools();
	conf_openrg_img();
	conf_prod_kernel_image();
	conf_compressed_disk();
	conf_os_target();
	conf_target_general();
	conf_flash_img_offset();
	conf_have_gc_sections();
	conf_gcc_has_size_optimize();
	conf_gcc_attribute_support();
	conf_rg_cflags();
	conf_arch_cflags();
	conf_rg_linux_include_cflags();
	conf_rg_linux_cflags();
	conf_linux_cflags();
	conf_ldflags();
	conf_cflags();
	conf_openrg_lflags();
	conf_jrguml();
	conf_dc();
    }
    conf_parallel_make();
    conf_lang_compiler();
    conf_text2c();
    conf_jslint();
    conf_sassc();
    conf_java();
    /* PATH: Start with '.' for autogenerated executables */
    token_set("PATH", ".:%s", PATH);
}

