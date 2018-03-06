/****************************************************************************
 *
 * rg/pkg/build/dist_config.c
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

#include <string.h>
#include "config_opt.h"
#include "create_config.h"
#include <sys/stat.h>
#include <unistd.h>

static int stat_lic_file(char *path)
{
    struct stat s;
    int ret = stat(path, &s);

    printf("Searching for license file in %s: %sfound\n", path,
	ret ? "not " : "");
    return ret;
}

static void set_jnet_server_configs(void)
{
    token_set_y("CONFIG_JRMS_DEBUG");
    token_set_y("CONFIG_JRMS_LDAP");    
    token_set_y("CONFIG_RG_HTTPS");
    token_set_y("CONFIG_RG_SSL");
    token_set_y("CONFIG_RG_OPENSSL_MD5");
    token_set_y("CONFIG_RG_JNLP");
    token_set_y("CONFIG_RG_MGT");
    token_set_y("CONFIG_RG_DSLHOME");
    token_set_y("CONFIG_RG_WGET");
    token_set_y("CONFIG_LOCAL_WBM_LIB");
    token_set_y("CONFIG_RG_SESSION_MYSQL");
    token_set_y("HAVE_MYSQL");
    token_set_y("CONFIG_RG_JNET_SERVER");
    token_set_y("CONFIG_RG_AJAX_SERVER");
    token_set("CONFIG_RG_JPKG_DIST", "JPKG_LOCAL_I386");
    token_set("TARGET_MACHINE", "local_i386");
    token_set_y("CONFIG_RG_LANG");
    token_set_y("CONFIG_GLIBC");
    token_set_y("CONFIG_RG_LIBIMAGE_DIM");
    token_set_y("CONFIG_RG_SQL");
    token_set_y("CONFIG_RG_LOCAL_WPA_UTILS");
    token_set_y("CONFIG_RG_JRMS_REDUCE_SUPPORT");    
    token_set_y("CONFIG_RG_JRMS_USERS_SUPPORT");
    token_set_y("CONFIG_RG_CMD");
    token_set_y("CONFIG_RG_REDUCE_SUPPORT");
    token_set_y("CONFIG_RG_FIREWALL");
    token_set_y("CONFIG_RG_WEB"); 
    token_set_y("CONFIG_RG_WEB_UTIL_LIB");
    token_set_y("CONFIG_RG_WEB_GRAPHICS_LIB");
    token_set_y("CONFIG_RG_WEB_NETWORK_LIB");
    token_set_y("CONFIG_RG_WEB_RG_LIB");
    token_set_y("CONFIG_RG_SSL_ROOT_CERTIFICATES");
    token_set_y("CONFIG_RG_JQUERY");
    token_set_y("CONFIG_RG_JRMS_SYSLOG");
    token_set_y("CONFIG_RG_JRMS_MONITORING");
    token_set_y("CONFIG_RG_JRMS_PBX");
    token_set_y("CONFIG_RG_JRMS_DOCSIS");
    token_set_y("CONFIG_RG_JRMS_PPP_AUTHENTICATION");
    token_set_y("CONFIG_RG_JRMS_WEB_AUTH");
    token_set_y("CONFIG_RG_JRMS_HW_DSL_WAN");
    token_set_y("CONFIG_RG_JRMS_IPERF");
    token_set_y("CONFIG_RG_JRMS_LOG_VOIP_RTP_STATS");
    token_set_y("CONFIG_HEADEND_OSAP");
    token_set_y("CONFIG_RG_WEB_SERVICE");    
    token_set_y("CONFIG_RG_CURL");    
    token_set_y("CONFIG_HEADEND_OSAP_OPAQUE_IDS");
    token_set_y("CONFIG_HEADEND_OSAP_STORAGE");
    token_set_y("CONFIG_RG_FAST_CGI");    
    token_set("CONFIG_JRMS_APPS_LOCATION", "/usr/lib/jrms-jacs");
    token_set("CONFIG_JRMS_UI_APPS_LOCATION", "/usr/lib/jrms-ui");
    token_set("CONFIG_JRMS_UI_HTDOCS_LOCATION", "/usr/share/jrms-ui/htdocs");
    token_set("CONFIG_JRMS_UI_STYLES_LOCATION", "%s/styles",
	token_getz("CONFIG_JRMS_UI_HTDOCS_LOCATION"));
    token_set("CONFIG_JRMS_AJAX_HTDOCS_LOCATION", 
	"/usr/share/jrms-ajax/htdocs");
    token_set("CONFIG_JRMS_SCR_HTDOCS_LOCATION", "/usr/share/jrms-scr/htdocs");
    token_set("CONFIG_JRMS_VOIP_VOICEMAIL_PASSWORDS_LOCATION",
	"/var/spool/asterisk/voicemail_passwords");
    token_set("CONFIG_JRMS_VOIP_PHONE_NUMBERS_LOCATION",
	"/var/spool/asterisk/phone_numbers");
}

static void set_vas_configs(void)
{
    token_set_y("CONFIG_RG_VAS_PORTAL");
    token_set_y("CONFIG_RG_JQUERY");

    token_set_y("CONFIG_RG_HTTPS");
    token_set_y("CONFIG_RG_SSL");
    token_set_y("CONFIG_RG_OPENSSL_MD5");
    token_set_y("CONFIG_RG_JNLP");
    token_set_y("CONFIG_RG_MGT");
    token_set_y("CONFIG_RG_WGET");
    token_set_y("CONFIG_RG_SESSION_MYSQL");
    token_set_y("HAVE_MYSQL");
    token_set("CONFIG_RG_JPKG_DIST", "JPKG_LOCAL_I386");
    token_set("TARGET_MACHINE", "local_i386");
    token_set_y("CONFIG_RG_LANG");
    token_set_y("CONFIG_GLIBC");
    token_set_y("CONFIG_RG_LIBIMAGE_DIM");
    token_set_y("CONFIG_RG_SQL");
}

static void set_hosttools_configs(void)
{
    /***************************************************************************
    * this function created due to bug B32553 . until the bug will fix         *
    * we need to seperate the configs used by the HOSTTOOLS dist to 2 sets     *
    * 1 - contain CONFIG_RG_BUILD_LOCAL_TARGETS_ONLY which can turn on by the  *
    * dist         but not by the jpkg                                         * 
    * 2 - contain configs which should also turn on by the JPKG                *
    ***************************************************************************/
    token_set_y("CONFIG_RG_ZLIB");
    token_set_y("CONFIG_RG_TOOLS");
    token_set("CONFIG_RG_DISK_EMULATION_COUNT", "4");
    token_set_y("CONFIG_RG_OPENSSL");
}

char *set_dist_license(void)
{
#define DEFAULT_LIC_DIR "pkg/license/licenses/"
#define DEFAULT_JPKG_LIC_DIR "pkg/jpkg/install/"
#define DEFAULT_LIC_FILE "license.lic"
#define DEFAULT_JPKG_LIC_DIR "pkg/jpkg/install/"

    char *lic = NULL;

    if (IS_DIST("YWZ00B_VODAFONE") || IS_DIST("BCM96362_VODAFONE") || 
	IS_DIST("VOX_1.5_IT") || IS_DIST("RGLOADER_YWZ00B") ||
	IS_DIST("VOX_2.5_IT_1.5HW"))
    {
	lic =  DEFAULT_JPKG_LIC_DIR "jpkg_bcm9636x_vodafone.lic";
    }
    else if (IS_DIST("VOX_1.5_ES") || IS_DIST("RGLOADER_AGC00A"))
	lic =  DEFAULT_JPKG_LIC_DIR "jpkg_bcm9636x_vodafone_es.lic";
    else if (IS_DIST("VOX_1.5_PT"))
	lic =  DEFAULT_JPKG_LIC_DIR "jpkg_bcm9636x_vodafone_pt.lic";
    else if (IS_DIST("VOX_2.0_DE") || IS_DIST("VOX_2.0_INT"))
	lic =  DEFAULT_JPKG_LIC_DIR "jpkg_xway_vodafone_de.lic";
    else if (IS_DIST("VOX_1.5_NZ"))
	lic =  DEFAULT_JPKG_LIC_DIR "jpkg_bcm9636x_vodafone_nz.lic";
    else if (IS_DIST("VOX_2.5_IT"))
	lic =  DEFAULT_JPKG_LIC_DIR "jpkg_bcm9636x_vodafone_vox25_it.lic";
    else if (IS_DIST("VOX_2.5_UK"))
	lic =  DEFAULT_JPKG_LIC_DIR "jpkg_bcm9636x_vodafone_vox25_uk.lic";
    else if (IS_DIST("VOX_2.5_DE_VDSL_26"))
	lic = DEFAULT_JPKG_LIC_DIR "jpkg_bcm9636x_vodafone_vox25_de.lic";
    else if (IS_DIST("VOX_2.5_DE_VDSL") || IS_DIST("VOX_2.5_DE_VDSL_3X"))
	lic = DEFAULT_JPKG_LIC_DIR "jpkg_bcm9636x_3x_vodafone_vox25_de.lic";
    else if (!stat_lic_file(DEFAULT_LIC_DIR DEFAULT_LIC_FILE))
	lic = DEFAULT_LIC_DIR DEFAULT_LIC_FILE;
    else if (!stat_lic_file(DEFAULT_LIC_FILE))
	lic = DEFAULT_LIC_FILE;

    if (lic)
    {
	token_set("LIC", lic);
	is_default_license = !!strstr(lic, DEFAULT_LIC_FILE);
    }
    return lic;
}

static void set_jpkg_dist_configs(char *jpkg_dist)
{
    int is_src = !strcmp(jpkg_dist, "JPKG_SRC");

#if 0
    if (is_src || !strcmp(jpkg_dist, "JPKG_UML"))
    {
	jpkg_dist_add("UML");
	jpkg_dist_add("UML_GLIBC");
	jpkg_dist_add("UML_26");
	jpkg_dist_add("RGLOADER_UML");
	jpkg_dist_add("UML_VALGRIND");

	set_hosttools_configs();
	enable_module("MODULE_RG_FULL_PBX");
    }
    if (is_src || !strcmp(jpkg_dist, "JPKG_ARMV6J"))
    {
	//jpkg_dist_add("PACKET-IAD");
	//jpkg_dist_add("FE-ROUTER");
	jpkg_dist_add("BB-ROUTER");

	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_PRINTSERVER");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_ROUTE_MULTIWAN");
	enable_module("MODULE_RG_BLUETOOTH");
	enable_module("MODULE_RG_SNMP");	
	enable_module("MODULE_RG_IPV6");
	enable_module("MODULE_RG_VOIP_ASTERISK_MGCP_CALL_AGENT");
	enable_module("MODULE_RG_MAIL_SERVER");
	enable_module("MODULE_RG_JVM");
	enable_module("MODULE_RG_FTP_SERVER");
	/* Can't be included because B37659
	enable_module("MODULE_RG_VOIP_ASTERISK_H323");
	*/
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_TR_064");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
        enable_module("CONFIG_HW_80211G_RALINK_RT2560");
        enable_module("CONFIG_HW_80211G_RALINK_RT2561");
	enable_module("CONFIG_HW_80211N_RALINK_RT2860");
	/* Can't be included because B3774
	enable_module("MODULE_RG_VOIP_OSIP");
	*/
	enable_module("MODULE_RG_ATA");
	token_set_y("CONFIG_RG_DSLHOME_VOUCHERS"); /* removed from MALINDI2 */
	enable_module("MODULE_RG_FILESERVER");
	enable_module("MODULE_RG_IPSEC");
        enable_module("MODULE_RG_FULL_PBX");
	enable_module("MODULE_RG_HOME_PBX");
        enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_SSL_VPN");
	enable_module("MODULE_RG_UPNP_AV");
	enable_module("MODULE_RG_RADIUS_SERVER");
	enable_module("MODULE_RG_PSE");
	enable_module("MODULE_RG_VIDEO_SURVEILLANCE");
	enable_module("MODULE_RG_REDUCE_SUPPORT");
	enable_module("MODULE_RG_OSAP_AGENT");

	token_set_m("CONFIG_IPSEC_ALG_JOINT");

	/* these are needed for VLAN */
	token_set_y("CONFIG_RG_IPROUTE2");
	token_set_m("CONFIG_RG_BRIDGE");
	enable_module("MODULE_RG_VLAN");
    }
    if (is_src || !strcmp(jpkg_dist, "JPKG_OCTEON"))
    {
	jpkg_dist_add("CN3XXX");

	enable_module("MODULE_RG_PSE");
	enable_module("MODULE_RG_FULL_PBX");
	enable_module("MODULE_RG_VOIP_ASTERISK_MGCP_CALL_AGENT");
    }
    if (is_src || !strcmp(jpkg_dist, "JPKG_MIPSEB"))
    {
	jpkg_dist_add("DWV_96358");
	jpkg_dist_add("HH1620");
	jpkg_dist_add("CT6382D");
	jpkg_dist_add("DWV_96358_VODAFONE");
	jpkg_dist_add("TW_96358_VODAFONE");
	jpkg_dist_add("BCM_96358_VODAFONE_TWONKY");

	enable_module("MODULE_RG_FILESERVER");
	enable_module("MODULE_RG_UPNP_AV");
	enable_module("MODULE_RG_VOIP_ASTERISK_SIP");
	enable_module("MODULE_RG_WEB_SERVER");
	enable_module("MODULE_RG_ATA");
	enable_module("MODULE_RG_PRINTSERVER");
	enable_module("MODULE_RG_BLUETOOTH");
	enable_module("MODULE_RG_IPSEC");
	/* Can't be included in JPKG_MIPSEB because B37659
	enable_module("MODULE_RG_VOIP_ASTERISK_H323");
	*/
	enable_module("MODULE_RG_VOIP_ASTERISK_MGCP_CALL_AGENT");
	enable_module("MODULE_RG_MAIL_SERVER");	
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_JVM");
	enable_module("MODULE_RG_FTP_SERVER");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_ROUTE_MULTIWAN");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_IPV6");
	/* Can't be included in JPKG_MIPSEB because B3774
	enable_module("MODULE_RG_VOIP_OSIP");
	*/
	enable_module("MODULE_RG_FULL_PBX");
	enable_module("MODULE_RG_HOME_PBX");
	enable_module("MODULE_RG_RADIUS_SERVER");
	enable_module("CONFIG_HW_USB_RNDIS");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_TR_064");
	token_set_y("CONFIG_RG_MAINFS_IN_PHYS_MAPPED_FLASH");
	enable_module("MODULE_RG_VLAN");
    }
#endif
#if 0
    if (is_src || !strcmp(jpkg_dist, "JPKG_MIPSEB_XWAY"))
    {
	jpkg_dist_add("ARX188");
	jpkg_dist_add("ARX188_ETH");
	jpkg_dist_add("VRX288");
	jpkg_dist_add("VOX_2.0_DE");
	jpkg_dist_add("VOX_2.0_INT");
	token_set_y("CONFIG_RG_POLARSSL");
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_RG_DEV_IF_LANTIQ_VR9_ETH");
	token_set_y("CONFIG_RG_MAIL_SERVER_UW_IMAP");
	token_set_y("CONFIG_RG_LAME");
        token_set_y("CONFIG_RG_LIBICONV");
	token_set_y("CONFIG_RG_LLDP_RX");
	token_set_y("CONFIG_RG_PPTPC");
	token_set_y("CONFIG_RG_L2TPC");
	token_set_y("CONFIG_RG_LANTIQ_XWAY_DYN_DATAPATH");
	token_set_y("CONFIG_RG_LANTIQ_XWAY_STATIC_DATAPATH");	

	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_REDUCE_SUPPORT");
	enable_module("MODULE_RG_OSAP_AGENT");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_VIDEO_SURVEILLANCE");
	enable_module("MODULE_RG_SSL_VPN");
	enable_module("MODULE_RG_FULL_PBX");
	enable_module("MODULE_RG_HOME_PBX");
	enable_module("MODULE_RG_PSE");
	enable_module("MODULE_RG_ATA");
	enable_module("MODULE_RG_VOIP_ASTERISK_SIP");
	enable_module("MODULE_RG_VOIP_ASTERISK_H323");
	enable_module("MODULE_RG_VOIP_ASTERISK_MGCP_CALL_AGENT");
	enable_module("MODULE_RG_WEB_SERVER");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_IPV6");
	enable_module("MODULE_RG_MAIL_SERVER");
	enable_module("MODULE_RG_JVM");
	enable_module("MODULE_RG_FTP_SERVER");
	enable_module("MODULE_RG_BLUETOOTH");
	enable_module("MODULE_RG_ROUTE_MULTIWAN");
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_UPNP_AV");
	enable_module("MODULE_RG_PRINTSERVER");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_TR_064");
	enable_module("CONFIG_HW_ENCRYPTION");
	enable_module("MODULE_RG_MEDIA_SERVER");
	enable_module("MODULE_RG_ACCESS_DLNA");	
	token_set_y("CONFIG_RG_MTD_UTILS");

	/* JPKG needs all possible networking drivers */
	token_set_y("CONFIG_ATM");
	token_set_y("CONFIG_IFX_ATM");
	token_set_y("CONFIG_IFX_PTM");
	token_set_y("CONFIG_IFX_PPA_A5");
	token_set_y("CONFIG_IFX_PPA_D5");
	token_set_y("CONFIG_IFX_PPA_E5");
	token_set_y("CONFIG_IFX_PPA_DATAPATH");
	token_set_y("CONFIG_IFX_PPA_API_PROC");	
	token_set_y("CONFIG_IFX_3PORT_SWITCH");
	token_set_y("CONFIG_IFX_7PORT_SWITCH");
	token_set_y("CONFIG_IFX_ETH_FRAMEWORK");
    }
#endif
    if (is_src || !strcmp(jpkg_dist, "JPKG_MIPSEB_BCM9636X"))
    {
	jpkg_dist_add("YWZ00B_VODAFONE");
	jpkg_dist_add("VOX_1.5_IT");
	jpkg_dist_add("VOX_1.5_ES");
	jpkg_dist_add("VOX_1.5_NZ");
	/* XXX Remove temporarily VF-PT distribution from jpkg-s.
	 * When the project comes back to an active development phase, it
	 * should be added again.
	 */
#if 0
	jpkg_dist_add("VOX_1.5_PT");
#endif
	jpkg_dist_add("VOX_2.5_IT_1.5HW");
	token_set_y("CONFIG_ATM_PVC_SCAN");
	enable_module("MODULE_RG_IPSEC");
	token_set_y("CONFIG_RG_EMAZE_HTTP_PROXY");
    }
    if (is_src || !strcmp(jpkg_dist, "JPKG_MIPSEB_BCM9636X_BSP_4_12") ||
	!strcmp(jpkg_dist, "JPKG_MIPSEB_BCM9636X_BSP_4_12_IT") ||
	!strcmp(jpkg_dist, "JPKG_MIPSEB_BCM9636X_BSP_4_12_UK") ||
	!strcmp(jpkg_dist, "JPKG_MIPSEB_BCM9636X_BSP_4_12_DE") ||
	!strcmp(jpkg_dist, "JPKG_MIPSEB_BCM9636X_3X"))
    {
	token_set_y("CONFIG_ATM_PVC_SCAN");
	token_set_y("CONFIG_RG_TELNETS");
    }
    if (is_src || !strcmp(jpkg_dist, "JPKG_MIPSEB_BCM9636X_BSP_4_12") ||
	!strcmp(jpkg_dist, "JPKG_MIPSEB_BCM9636X_BSP_4_12_IT") ||
	!strcmp(jpkg_dist, "JPKG_MIPSEB_BCM9636X_BSP_4_12_UK"))
    {
	enable_module("MODULE_RG_IPSEC");
    }
    if (is_src || !strcmp(jpkg_dist, "JPKG_MIPSEB_BCM9636X_BSP_4_12") ||
	!strcmp(jpkg_dist, "JPKG_MIPSEB_BCM9636X_BSP_4_12_IT"))
    {
	token_set_y("CONFIG_RG_EMAZE_HTTP_PROXY");
	token_set_y("CONFIG_RG_BSP_UPGRADE");
	token_set_y("CONFIG_RG_RGLOADER_UPGRADE");
	jpkg_dist_add("VOX_2.5_IT");
    }
    if (is_src || !strcmp(jpkg_dist, "JPKG_MIPSEB_BCM9636X_BSP_4_12") ||
	!strcmp(jpkg_dist, "JPKG_MIPSEB_BCM9636X_BSP_4_12_UK"))
    {
	token_set_y("CONFIG_RG_BSP_UPGRADE");
	token_set_y("CONFIG_RG_RGLOADER_UPGRADE");
	jpkg_dist_add("VOX_2.5_UK");
    }
    if (is_src || !strcmp(jpkg_dist, "JPKG_MIPSEB_BCM9636X_BSP_4_12") ||
	!strcmp(jpkg_dist, "JPKG_MIPSEB_BCM9636X_BSP_4_12_DE"))
    {
	jpkg_dist_add("VOX_2.5_DE_VDSL_26");
    }
    if (is_src || !strcmp(jpkg_dist, "JPKG_MIPSEB_BCM9636X_3X"))
    {
	jpkg_dist_add("VOX_2.5_DE_VDSL");
	jpkg_dist_add("VOX_2.5_DE_VDSL_3X");
    }
    if (is_src || !strcmp(jpkg_dist, "JPKG_LOCAL_I386"))
    {
	/* Can't use JNET_SERVER in JPKG_SRC because it turns on
	 * CONFIG_RG_USE_LOCAL_TOOLCHAIN, which we don't want.
	 * Remove this when B32553 is fixed.
	 */
	/* do not create jpkgs for jnet/jrms for now
	 * When branch-dev-jrms-packages is merged, uncomment these lines  
	if (is_src)
	    set_jnet_server_configs();
	else
	    jpkg_dist_add("JNET_SERVER");
	*/
    }
    if (is_src)
    {
#if 0
	token_set_y("CONFIG_RG_DOC_SDK");
	token_set_y("CONFIG_RG_SAMPLES_COMMON");	
#endif
	token_set_y("CONFIG_RG_TCPDUMP");
	token_set_y("CONFIG_RG_LIBPCAP");
	token_set_y("CONFIG_RG_IPROUTE2_UTILS");
#if 0
        token_set_y("CONFIG_RG_JAVA");
        token_set_y("CONFIG_RG_JTA");
        token_set_y("CONFIG_RG_PROPER_JAVA_RDP");
        token_set_y("CONFIG_RG_JVFTP");
        token_set_y("CONFIG_RG_JCIFS");
        token_set_y("CONFIG_RG_SMB_EXPLORER");
        token_set_y("CONFIG_RG_TIGHT_VNC");
	token_set_y("GLIBC_IN_TOOLCHAIN");	
	token_set_y("CONFIG_RG_JNET_SERVER_TUTORIAL");
	token_set_y("CONFIG_RG_OSAP_AGENT");
#endif
	token_set_y("CONFIG_RG_GDBSERVER");
    }
    else
    {
        token_set_y("CONFIG_RG_JPKG_BIN");
	/* JPKG consistency test require these CONFIGs to be set for all ARCHs 
	 */
	token_set_y("CONFIG_RG_GDBSERVER");
	token_set_y("CONFIG_RG_KERNEL");
	token_set_y("CONFIG_RG_SAMPLES_COMMON");
    }

    /* Common additional features: */
    token_set_y("CONFIG_RG_JPKG");
    if (strcmp(jpkg_dist, "JPKG_LOCAL_I386"))
    {
	/* These shouldn't be turned on in binary local jpkg */
	token_set_y("CONFIG_RG_NETTOOLS_ARP");
	token_set_y("CONFIG_RG_TOOLS");
	token_set_y("CONFIG_RG_CONF_INFLATE");
        token_set_y("CONFIG_OPENRG");
        enable_module("MODULE_RG_DSLHOME");
    }
    /* Add all event configs */
    token_set_y("CONFIG_TARGET_EVENT_EPOLL");
    token_set_y("CONFIG_TARGET_EVENT_POLL");
    token_set_y("CONFIG_TARGET_EVENT_SELECT");
    token_set_y("CONFIG_LOCAL_EVENT_EPOLL");
    token_set_y("CONFIG_LOCAL_EVENT_POLL");
    token_set_y("CONFIG_LOCAL_EVENT_SELECT");
}

void set_vodafone_common_configs(void *lan_dev_name, char *wifi_dev, char *oui)
{
    token_set_y("CONFIG_RG_VODAFONE");

    if (token_get("CONFIG_HW_LCD"))
    {
	token_set_y("CONFIG_RG_VODAFONE_LCD");
	token_set_y("CONFIG_RG_VODAFONE_LCD_GUI");
	token_set_y("CONFIG_RG_LIBDISKO");
	if (token_get("MODULE_RG_MODULAR_UPGRADE"))
	    token_set_y("CONFIG_RG_VODAFONE_LCD_LOGO_DLM");
    }

    token_set_y("CONFIG_RG_WAN_AUTO_DETECT");

    if (token_get("CONFIG_RG_PRECONF"))
        token_set_y("CONFIG_RG_PPP_PRECONF");

    enable_module("MODULE_RG_VAS_CLIENT");

    token_set("CONFIG_RG_VENDOR_OUI", oui);
    token_set_y("CONFIG_RG_VODAFONE_CCF");
    if (wifi_dev)
	token_set("CONFIG_VF_WLAN_DEV", wifi_dev);

    token_set("CONFIG_VF_SWITCH_LAN_DEV", lan_dev_name);
}

void set_vodafone_nz_configs(void *lan_dev_name, char *wifi_dev)
{
    token_set("CONFIG_VF_DATA_VLAN", "1");
    token_set("CONFIG_VF_WAN_DATA_VLAN", "10");

    token_set("CONFIG_VF_DATA_LAN_DEV", "data_lan");
    dev_add(token_getz("CONFIG_VF_DATA_LAN_DEV"), 
	DEV_IF_USER_VLAN, DEV_IF_NET_INT);
    dev_set_dependency(token_getz("CONFIG_VF_DATA_LAN_DEV"), lan_dev_name);

    token_set("CONFIG_VF_ETH_DATA_WAN_DEV", "data_wan");
    dev_add(token_getz("CONFIG_VF_ETH_DATA_WAN_DEV"), 
	DEV_IF_USER_VLAN, DEV_IF_NET_EXT);
    dev_set_dependency(token_getz("CONFIG_VF_ETH_DATA_WAN_DEV"), lan_dev_name);

    dev_add_bridge("br0", DEV_IF_NET_INT, token_getz("CONFIG_VF_DATA_LAN_DEV"), 
	NULL);
    if (wifi_dev)
	dev_add_to_bridge("br0", wifi_dev);
    token_set("CONFIG_VF_LAN_UPD_DEV", "br0");

    /* Set wireless region */
    token_set("CONFIG_RG_WIRELESS_COUNTRY_REGION", "NEW ZEALAND");
    token_set("CONFIG_RG_WIRELESS_COUNTRY_CODE", "NZ");

    token_set("CONFIG_RG_LANGUAGES", "DEF");
    token_set("CONFIG_RG_DEFAULT_LANGUAGE", "DEF");    

    token_set_y("CONFIG_RG_VAP_SECURED");
    token_set_y("CONFIG_RG_VAP_GUEST");

    /* VPN server */
    enable_module("MODULE_RG_PPTP");
    enable_module("MODULE_RG_L2TP");
    enable_module("MODULE_RG_IPSEC");

    token_set_y("CONFIG_RG_VODAFONE_STATIC_NAT");

    token_set("CONFIG_RG_VODAFONE_ALLOWED_WAN", "DETECTING | ADSL | FTTH");

    token_set_y("CONFIG_RG_ADDRESS_BOOK");

    /* Static ARP */
    token_set_y("CONFIG_RG_VODAFONE_STATIC_ARP");

    /* 3G */
    token_set_y("CONFIG_RG_3G_USSD");

    /* Audio Codecs */
    token_set_y("CONFIG_RG_VOIP_ASTERISK_CODEC_ALAW");
    token_set_y("CONFIG_RG_VOIP_ASTERISK_CODEC_AMULAW");

    if (token_get("CONFIG_VF_WBM_INTERNATIONAL"))
    {
	token_set_y("CONFIG_RG_LIBCAPTCHA");
	token_set_y("CONFIG_RG_ALARM_CLOCK");
	token_set_y("CONFIG_RG_RINGING_SCHEDULE");
	token_set_y("CONFIG_RG_SPEED_DIAL");
	token_set_y("CONFIG_RG_DSLHOME_USB");
	token_set_y("CONFIG_RG_LOG_PROJ");
    }

    /* LCD */
    if (token_get("CONFIG_HW_LCD"))
    {
	token_set_y("CONFIG_RG_VODAFONE_LCD_LOGO");
	token_set_y("CONFIG_HW_LCD_SCREEN_SAVER");
	token_set("CONFIG_RG_VODAFONE_LCD_LOGO_ANIMATION_FPS", "22");
	token_set("CONFIG_RG_VODAFONE_LCD_LOGO_ANIMATION_DIR",
	    "animation_71_frames");
	token_set_y("CONFIG_RG_VODAFONE_LCD_BOOTING_ANIMATION");
	token_set("CONFIG_RG_VODAFONE_LCD_BOOTING_ANIMATION_DIR",
	    "boot_animation_de");
	token_set_y("CONFIG_RG_VODAFONE_LCD_WELCOME_SCREEN");
    }

    /* USB manager */
    token_set_y("CONFIG_USB_MNG");
}

static void vodafone_it_vlan_wan_dev_add(char *wan_dev_name, 
    char *data_wan_vlan_dev, char *voice_wan_vlan_dev)
{
    dev_add(data_wan_vlan_dev, DEV_IF_USER_VLAN, DEV_IF_NET_EXT);
    dev_set_dependency(data_wan_vlan_dev, wan_dev_name);

    dev_add(voice_wan_vlan_dev,	DEV_IF_USER_VLAN, DEV_IF_NET_EXT);
    dev_set_dependency(voice_wan_vlan_dev, wan_dev_name);
}

static void set_vodafone_it_uk_configs(void *lan_dev_name, void *eth_wan_dev_name,
    void *ptm_wan_dev_name, char *wifi_dev)
{
    char *tmp_dev_name;

    if (eth_wan_dev_name && ptm_wan_dev_name)
    {
	token_set("CONFIG_VF_ETH_WAN_DEV", eth_wan_dev_name);
	token_set("CONFIG_VF_ETH_DATA_WAN_DEV", "data_wan0");
	token_set("CONFIG_VF_ETH_VOICE_WAN_DEV", "voice_wan0");
	vodafone_it_vlan_wan_dev_add(eth_wan_dev_name, 
	    token_getz("CONFIG_VF_ETH_DATA_WAN_DEV"),
	    token_getz("CONFIG_VF_ETH_VOICE_WAN_DEV")); 

	token_set("CONFIG_VF_PTM_WAN_DEV", ptm_wan_dev_name);
	token_set("CONFIG_VF_PTM_DATA_WAN_DEV", "data_wan1");
	token_set("CONFIG_VF_PTM_VOICE_WAN_DEV", "voice_wan1");
	vodafone_it_vlan_wan_dev_add(ptm_wan_dev_name, 
    	    token_getz("CONFIG_VF_PTM_DATA_WAN_DEV"),
	    token_getz("CONFIG_VF_PTM_VOICE_WAN_DEV")); 
    }
    else if (eth_wan_dev_name)
    {
	token_set("CONFIG_VF_ETH_WAN_DEV", eth_wan_dev_name);
	token_set("CONFIG_VF_ETH_DATA_WAN_DEV", "data_wan");
	token_set("CONFIG_VF_ETH_VOICE_WAN_DEV", "voice_wan");
	vodafone_it_vlan_wan_dev_add(eth_wan_dev_name,
 	    token_getz("CONFIG_VF_ETH_DATA_WAN_DEV"),
	    token_getz("CONFIG_VF_ETH_VOICE_WAN_DEV")); 
    }
    else
    {
	token_set("CONFIG_VF_PTM_WAN_DEV", ptm_wan_dev_name);
	token_set("CONFIG_VF_PTM_DATA_WAN_DEV", "data_wan");
	token_set("CONFIG_VF_PTM_VOICE_WAN_DEV", "voice_wan");
	vodafone_it_vlan_wan_dev_add(ptm_wan_dev_name,
	    token_getz("CONFIG_VF_PTM_DATA_WAN_DEV"),
	    token_getz("CONFIG_VF_PTM_VOICE_WAN_DEV")); 
    }
    if (token_get("CONFIG_RG_ROUTED_LAN_VLAN_SEPARATION"))
    {
	dev_add_bridge("br0", DEV_IF_NET_INT,
	    token_getz("CONFIG_RG_ROUTED_LAN_HOME_VLAN_DEV"), NULL);
    }
    else
    {
	/* Enslave the VLANs to the data and voice bridges:
	 * voice-vlan is enslaved to the voice bridge (br1).
	 * data-vlan is enslaved to both bridges, because IPPHONES 
	 * can be connected to Openrg as a regular hosts (untagged) 
	 * and only be distinguished by dhcp-client option-60
	 */
	dev_add_bridge("br0", DEV_IF_NET_INT,
	    token_get("CONFIG_HW_SWITCH_LAN") &&
	    token_get_str("CONFIG_VF_DATA_LAN_DEV") ?
	    token_getz("CONFIG_VF_DATA_LAN_DEV") : lan_dev_name, NULL);
    }

    if (wifi_dev)
	dev_add_to_bridge("br0", wifi_dev);
    dev_add_bridge("br1", DEV_IF_NET_INT, NULL);
    if ((tmp_dev_name = token_get_str("CONFIG_VF_DATA_LAN_DEV")))
	dev_add_to_bridge("br1", tmp_dev_name);
    if ((tmp_dev_name = token_get_str("CONFIG_VF_VOICE_LAN_DEV")))
	dev_add_to_bridge("br1", tmp_dev_name);
    token_set("CONFIG_VF_LAN_UPD_DEV", "br0");

    if (token_get("CONFIG_RG_DUAL_CONCURRENT_WLAN"))
    {
	char vap_name[16];

	dev_add_bridge("br2", DEV_IF_NET_INT, NULL);
	sprintf(vap_name, "%s.1", token_get_str("CONFIG_VF_WLAN_DEV"));
	dev_add_to_bridge("br2", vap_name);
	sprintf(vap_name, "%s.1", token_get_str("CONFIG_VF_WLAN2_DEV"));
	dev_add_to_bridge("br2", vap_name);

	if (token_get("CONFIG_RG_FON"))
	{
	    dev_add_bridge("br3", DEV_IF_NET_INT, NULL);
	    sprintf(vap_name, "%s.2", token_get_str("CONFIG_VF_WLAN_DEV"));
	    dev_add_to_bridge("br3", vap_name);
	    sprintf(vap_name, "%s.2", token_get_str("CONFIG_VF_WLAN2_DEV"));
	    dev_add_to_bridge("br3", vap_name);
	    sprintf(vap_name, "%s.3", token_get_str("CONFIG_VF_WLAN_DEV"));
	    dev_add_to_bridge("br3", vap_name);
	    sprintf(vap_name, "%s.3", token_get_str("CONFIG_VF_WLAN2_DEV"));
	    dev_add_to_bridge("br3", vap_name);
	}
    }

    /* Set wireless region */
    token_set("CONFIG_RG_WIRELESS_COUNTRY_REGION", "ITALY");
    token_set("CONFIG_RG_WIRELESS_COUNTRY_CODE", "EU");
    token_set_y("CONFIG_RG_VAP_SECURED");
    token_set_y("CONFIG_RG_VAP_GUEST");

    /* Easy Management Support */
    if (token_get("CONFIG_RG_VODAFONE_IT"))
        token_set_y("CONFIG_RG_VODAFONE_EASY_MNG");

    /* Remote access */
    if (token_get("CONFIG_RG_VODAFONE_IT") ||
	token_get("CONFIG_RG_VODAFONE_UK"))
    {
	token_set_y("CONFIG_RG_VODAFONE_REMOTE_ACCESS");
    }

    token_set_y("CONFIG_RG_VODAFONE_STATIC_NAT");

    token_set("CONFIG_RG_VODAFONE_ALLOWED_WAN", "DETECTING | ADSL | VDSL |"
	" FTTH | FTTC");

    /* VPN server */
    if (!IS_DIST("VF_IT_HG55MAGV"))
    {
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_IPSEC");
    }

    /* Voice VAS */
    token_set_y("CONFIG_RG_WEB_SERVICE");

    /* Static ARP */
    token_set_y("CONFIG_RG_VODAFONE_STATIC_ARP");

    /* Audio Codecs */
    token_set_y("CONFIG_RG_VOIP_ASTERISK_CODEC_ALAW");
    token_set_y("CONFIG_RG_VOIP_ASTERISK_CODEC_AMULAW");

    token_set_y("CONFIG_RG_SSH_SERVER");

    token_set_y("CONFIG_VF_WBM_INTERNATIONAL");
    
    token_set_y("CONFIG_RG_EXTERNAL_API");

    token_set("CONFIG_RG_HW_RESET_TO_DEF_TIMEOUT_MS", "5000");

    if (IS_DIST("VOX_2.5_IT_1.5HW"))
    {
	token_set("CONFIG_RG_LANGUAGES", "DEF de tr MSG_CODE");
	token_set("CONFIG_RG_DEFAULT_LANGUAGE", "DEF");
	token_set_y("CONFIG_RG_VODAFONE_ACTIVATION_WIZARD");
        token_set_y("CONFIG_VF_WBM_INT_1_5HW");

	/* WebDav */ 
	token_set_y("CONFIG_RG_WEBDAV");
	token_set_y("CONFIG_RG_TCPDUMP");
    }
    
    if (token_get("CONFIG_VF_WBM_INTERNATIONAL"))
    {
	token_set_y("CONFIG_RG_LIBCAPTCHA");
	token_set_y("CONFIG_RG_TCPDUMP");
	/* token_set_y("CONFIG_RG_ALARM_CLOCK"); */
	token_set_y("CONFIG_RG_DSLHOME_USB");
	token_set_y("CONFIG_RG_LOG_PROJ");
	token_set_y("CONFIG_RG_POWER_MANAGEMENT");
	token_set_y("CONFIG_RG_UI_BRUTE_FORCE_PROTECTION");
    }

    /* LCD */
    if (IS_DIST("VOX_2.5_IT_1.5HW") && token_get("CONFIG_HW_LCD"))
    {
	token_set_y("CONFIG_RG_VODAFONE_LCD_LOGO");
	token_set_y("CONFIG_HW_LCD_SCREEN_SAVER");
	token_set("CONFIG_RG_VODAFONE_LCD_LOGO_ANIMATION_FPS", "22");
	token_set("CONFIG_RG_VODAFONE_LCD_LOGO_ANIMATION_DIR",
	    "animation_71_frames");
	token_set_y("CONFIG_RG_VODAFONE_LCD_BOOTING_ANIMATION");
	token_set("CONFIG_RG_VODAFONE_LCD_BOOTING_ANIMATION_DIR",
	    "boot_animation_it");
	token_set_y("CONFIG_RG_VODAFONE_LCD_WELCOME_SCREEN");
    }
    token_set_y("CONFIG_RG_VODAFONE_CUSTOM_FW_RULES");
    /* USB manager */
    token_set_y("CONFIG_USB_MNG");
    token_set_y("CONFIG_RG_VSAF_WBM");
    token_set_y("CONFIG_RG_WPS_PBC_MODE_ONLY");
}

static void set_vodafone_it_devices(void)
{
#define VF_IT_ETHOA_DATA_WAN_DEV "ethoa0"
#define VF_IT_ETHOA_VOICE_WAN_DEV "ethoa1"
#define VF_IT_VPN_IPSEC_TUNNEL_DEV "ips0"
#define VF_IT_VPN_IPSEC_TRANSPORT_DEV "ips1"
#define VF_IT_L2TP_TUNNEL "l2tp_tunnel0"
#define VF_IT_PPPOE_WHOLESALE_WAN_DEV "ppp0"
#define VF_IT_PPPOE_VOICE_WAN_DEV "ppp1"
#define VF_IT_FTTH_PPPOE_DATA_WAN_DEV "ppp2"
#define VF_IT_VPN_PPTPC_DEV "ppp200"
#define VF_IT_FTTH_PPPOE_VOICE_WAN_DEV "ppp3"
#define VF_IT_VPN_L2TPC_DEV "ppp300"
#define VF_IT_VDSL_PPPOE_DATA_WAN_DEV "ppp4"
#define VF_IT_VDSL_PPPOE_VOICE_WAN_DEV "ppp5"
#define VF_IT_VW_DATA_WAN_DEV "vw0"
#define VF_IT_VW_VOICE_WAN_DEV "vw1"
#define VF_IT_FTTH_VW_DATA_WAN_DEV "vw2"
#define VF_IT_FTTH_VW_VOICE_WAN_DEV "vw3"
#define VF_IT_VDSL_VW_DATA_WAN_DEV "vw4"
#define VF_IT_VDSL_VW_VOICE_WAN_DEV "vw5"
#define VF_IT_ATM_DEV "bcm_atm0"
#define VF_IT_ETH_DATA_WAN_DEV "data_wan0"
#define VF_IT_PTM_DATA_WAN_DEV "data_wan1"
#define VF_IT_ETH_VOICE_WAN_DEV "voice_wan0"
#define VF_IT_PTM_VOICE_WAN_DEV "voice_wan1"

    /* ethoa0 */
    dev_add(VF_IT_ETHOA_DATA_WAN_DEV, DEV_IF_ETHOA, DEV_IF_NET_EXT);
    /* depends on bcm_atm0 */
    dev_set_dependency(VF_IT_ETHOA_DATA_WAN_DEV, VF_IT_ATM_DEV);

    /* ethoa1 */
    dev_add(VF_IT_ETHOA_VOICE_WAN_DEV, DEV_IF_ETHOA, DEV_IF_NET_EXT);
    /* depends on bcm_atm0 */
    dev_set_dependency(VF_IT_ETHOA_VOICE_WAN_DEV, VF_IT_ATM_DEV);
    dev_set_logical_dependency(VF_IT_ETHOA_VOICE_WAN_DEV, VF_IT_ATM_DEV);

    /* ips0 ipsec_conn */
    dev_add(VF_IT_VPN_IPSEC_TUNNEL_DEV, DEV_IF_IPSEC_CONN, DEV_IF_NET_EXT);
    /* ips1 ipsec_conn */
    dev_add(VF_IT_VPN_IPSEC_TRANSPORT_DEV, DEV_IF_IPSEC_CONN, DEV_IF_NET_EXT);

    /* l2tp_tunnel0 */
    dev_add(VF_IT_L2TP_TUNNEL, DEV_IF_L2TP_TUNNEL, DEV_IF_NET_EXT);
    /* depends on ips1 */
    dev_set_dependency(VF_IT_L2TP_TUNNEL, VF_IT_VPN_IPSEC_TRANSPORT_DEV);

    /* ppp0 */
    dev_add(VF_IT_PPPOE_WHOLESALE_WAN_DEV, DEV_IF_PPPOE, DEV_IF_NET_EXT);
    /* depends on ethoa0 */
    dev_set_dependency(VF_IT_PPPOE_WHOLESALE_WAN_DEV, VF_IT_ETHOA_DATA_WAN_DEV);

    /* ppp1 */
    dev_add(VF_IT_PPPOE_VOICE_WAN_DEV, DEV_IF_PPPOE, DEV_IF_NET_EXT);
    /* depends on ethoa1 */
    dev_set_dependency(VF_IT_PPPOE_VOICE_WAN_DEV, VF_IT_ETHOA_VOICE_WAN_DEV);
    dev_set_logical_dependency(VF_IT_PPPOE_VOICE_WAN_DEV,
	VF_IT_ETHOA_VOICE_WAN_DEV);

    /* ppp2 */
    dev_add(VF_IT_FTTH_PPPOE_DATA_WAN_DEV, DEV_IF_PPPOE, DEV_IF_NET_EXT);
    /* depends on data_wan0 */
    dev_set_dependency(VF_IT_FTTH_PPPOE_DATA_WAN_DEV, VF_IT_ETH_DATA_WAN_DEV);

    /* ppp200 */
    dev_add(VF_IT_VPN_PPTPC_DEV, DEV_IF_PPTPC, DEV_IF_NET_EXT);

    /* ppp3 */
    dev_add(VF_IT_FTTH_PPPOE_VOICE_WAN_DEV, DEV_IF_PPPOE, DEV_IF_NET_EXT);
    /* depends on voice_wan0 */
    dev_set_dependency(VF_IT_FTTH_PPPOE_VOICE_WAN_DEV, VF_IT_ETH_VOICE_WAN_DEV);
    dev_set_logical_dependency(VF_IT_FTTH_PPPOE_VOICE_WAN_DEV,
	VF_IT_ETH_VOICE_WAN_DEV);

    /* ppp300 */
    dev_add(VF_IT_VPN_L2TPC_DEV, DEV_IF_L2TPC, DEV_IF_NET_EXT);
    /* depends on l2tp_tunnel0 */
    dev_set_dependency(VF_IT_VPN_L2TPC_DEV, VF_IT_L2TP_TUNNEL);

    /* ppp4 */
    dev_add(VF_IT_VDSL_PPPOE_DATA_WAN_DEV, DEV_IF_PPPOE, DEV_IF_NET_EXT);
    /* depends on data_wan1 */
    dev_set_dependency(VF_IT_VDSL_PPPOE_DATA_WAN_DEV, VF_IT_PTM_DATA_WAN_DEV);

    /* ppp5 */
    dev_add(VF_IT_VDSL_PPPOE_VOICE_WAN_DEV, DEV_IF_PPPOE, DEV_IF_NET_EXT);
    /* depends on  voice_wan1 */
    dev_set_dependency(VF_IT_VDSL_PPPOE_VOICE_WAN_DEV, VF_IT_PTM_VOICE_WAN_DEV);
    dev_set_logical_dependency(VF_IT_VDSL_PPPOE_VOICE_WAN_DEV,
	VF_IT_PTM_VOICE_WAN_DEV);

    /* vw0 */
    dev_add(VF_IT_VW_DATA_WAN_DEV, DEV_IF_VIRTUAL_WAN, DEV_IF_NET_EXT);
    /* depends on ethoa0 */
    dev_set_dependency(VF_IT_VW_DATA_WAN_DEV, VF_IT_ETHOA_DATA_WAN_DEV);
    dev_set_logical_dependency(VF_IT_VW_DATA_WAN_DEV, VF_IT_ETHOA_DATA_WAN_DEV);

    /* vw1 */
    dev_add(VF_IT_VW_VOICE_WAN_DEV, DEV_IF_VIRTUAL_WAN, DEV_IF_NET_EXT);
    /* depends on ethoa1 */
    dev_set_dependency(VF_IT_VW_VOICE_WAN_DEV, VF_IT_ETHOA_VOICE_WAN_DEV);
    dev_set_logical_dependency(VF_IT_VW_VOICE_WAN_DEV,
	VF_IT_ETHOA_VOICE_WAN_DEV);

    /* vw2 */
    dev_add(VF_IT_FTTH_VW_DATA_WAN_DEV, DEV_IF_VIRTUAL_WAN, DEV_IF_NET_EXT);
    /* depends on data_wan0 */
    dev_set_dependency(VF_IT_FTTH_VW_DATA_WAN_DEV, VF_IT_ETH_DATA_WAN_DEV);
    dev_set_logical_dependency(VF_IT_FTTH_VW_DATA_WAN_DEV,
	VF_IT_ETH_DATA_WAN_DEV);

    /* vw3 */
    dev_add(VF_IT_FTTH_VW_VOICE_WAN_DEV, DEV_IF_VIRTUAL_WAN, DEV_IF_NET_EXT);
    /* depends on voice_wan0 */
    dev_set_dependency(VF_IT_FTTH_VW_VOICE_WAN_DEV, VF_IT_ETH_VOICE_WAN_DEV);
    dev_set_logical_dependency(VF_IT_FTTH_VW_VOICE_WAN_DEV,
	VF_IT_ETH_VOICE_WAN_DEV);

    /* vw4 */
    dev_add(VF_IT_VDSL_VW_DATA_WAN_DEV, DEV_IF_VIRTUAL_WAN, DEV_IF_NET_EXT);
    /* depends on data_wan1 */
    dev_set_dependency(VF_IT_VDSL_VW_DATA_WAN_DEV, VF_IT_PTM_DATA_WAN_DEV);
    dev_set_logical_dependency(VF_IT_VDSL_VW_DATA_WAN_DEV,
	VF_IT_PTM_DATA_WAN_DEV);

    /* vw5 */
    dev_add(VF_IT_VDSL_VW_VOICE_WAN_DEV, DEV_IF_VIRTUAL_WAN, DEV_IF_NET_EXT);
    /* depends on voice_wan1 */
    dev_set_dependency(VF_IT_VDSL_VW_VOICE_WAN_DEV, VF_IT_PTM_VOICE_WAN_DEV);
    dev_set_logical_dependency(VF_IT_VDSL_VW_VOICE_WAN_DEV,
	VF_IT_PTM_VOICE_WAN_DEV);
}

void set_vodafone_it_configs(void *lan_dev_name, void *eth_wan_dev_name,
    void *ptm_wan_dev_name, char *wifi_dev)
{
    token_set("CONFIG_VF_WAN_DATA_VLAN", "1036");
    token_set("CONFIG_VF_WAN_VOICE_VLAN", "1035");
    token_set("CONFIG_RG_LANGUAGES", "DEF it MSG_CODE");
    token_set("CONFIG_RG_DEFAULT_LANGUAGE", "it");
    token_set_y("CONFIG_RG_VODAFONE_CGN");
    token_set_y("CONFIG_RG_SETUP_WIZARD");
    /* 3G */
    token_set_y("CONFIG_RG_3G_USSD");
    token_set_y("CONFIG_RG_ADDRESS_BOOK");

    token_set("CONFIG_VF_DATA_VLAN", "1");
    token_set("CONFIG_VF_VOICE_VLAN", "2");
    token_set("CONFIG_VF_DATA_LAN_DEV", "data_lan");
    dev_add(token_getz("CONFIG_VF_DATA_LAN_DEV"), 
	DEV_IF_USER_VLAN, DEV_IF_NET_INT);
    dev_set_dependency(token_getz("CONFIG_VF_DATA_LAN_DEV"), lan_dev_name);

    token_set("CONFIG_VF_VOICE_LAN_DEV", "voice_lan");
    dev_add(token_getz("CONFIG_VF_VOICE_LAN_DEV"),
	DEV_IF_USER_VLAN, DEV_IF_NET_INT);
    dev_set_dependency(token_getz("CONFIG_VF_VOICE_LAN_DEV"), lan_dev_name);
    token_set_y("CONFIG_RG_VODAFONE_HTTP_REVERSE_PROXY");
    /* 3G */
    token_set_y("CONFIG_RG_3G_USSD");
    set_vodafone_it_uk_configs(lan_dev_name, eth_wan_dev_name, ptm_wan_dev_name,
	wifi_dev);
    token_set_y("CONFIG_RG_VODAFONE_VOICE_VAS");

    token_set_y("CONFIG_RG_RGCONF_CRYPTO_MD5");
    if (token_get("CONFIG_VF_WBM_INTERNATIONAL"))
    {
	token_set_y("CONFIG_RG_RINGING_SCHEDULE");
	token_set_y("CONFIG_RG_SPEED_DIAL");
	token_set_y("CONFIG_RG_VODAFONE_3G_ACTIVATION");
    }
    if (token_get("CONFIG_RG_DSLHOME_DATAMODEL_TR_181"))
	token_set_y("CONFIG_RG_IPV6_WBM");
    token_set("CONFIG_RG_HTTP_XFRAME_PROTECTION", "SAMEORIGIN");
    token_set_m("CONFIG_RG_NET_STATS_PEAK_THROUGHPUT");
}

static void set_vodafone_uk_devices(void)
{
#define VF_UK_ATM_DEV "bcm_atm0"
#define VF_UK_DATA_WAN_DEV "data_wan"
#define VF_UK_VOICE_WAN_DEV "voice_wan"
#define VF_UK_PPPOE_WHOLESALE_WAN_DEV "ppp0"
#define VF_UK_PPPOA_DATA_WAN_DEV "ppp1"
#define VF_UK_PPPOE_DATA_WAN_DEV "ppp2"
#define VF_UK_VDSL_PPPOE_VOICE_WAN_DEV "ppp3"
#define VF_UK_VPN_PPTPC_DEV "ppp200"
#define VF_UK_VPN_L2TPC_DEV "ppp300"
#define VF_UK_VPN_IPSEC_TUNNEL_DEV "ips0"
#define VF_UK_VPN_IPSEC_TRANSPORT_DEV "ips1"

    /* ppp0 */
    dev_add(VF_UK_PPPOE_WHOLESALE_WAN_DEV, DEV_IF_PPPOA, DEV_IF_NET_EXT);
    /* depends on bcm_atm0 */
    dev_set_dependency(VF_UK_PPPOE_WHOLESALE_WAN_DEV, VF_UK_ATM_DEV);
    dev_set_logical_dependency(VF_UK_PPPOE_WHOLESALE_WAN_DEV, VF_UK_ATM_DEV);

    /* ppp2 */
    dev_add(VF_UK_PPPOE_DATA_WAN_DEV, DEV_IF_PPPOE, DEV_IF_NET_EXT);
    /* depends on data_wan */
    dev_set_dependency(VF_UK_PPPOE_DATA_WAN_DEV, VF_UK_DATA_WAN_DEV);

    /* ppp3 */
    dev_add(VF_UK_VDSL_PPPOE_VOICE_WAN_DEV, DEV_IF_PPPOE, DEV_IF_NET_EXT);
    /* depends on voice_wan */
    dev_set_dependency(VF_UK_VDSL_PPPOE_VOICE_WAN_DEV, VF_UK_VOICE_WAN_DEV);
    dev_set_logical_dependency(VF_UK_VDSL_PPPOE_VOICE_WAN_DEV,
	VF_UK_VOICE_WAN_DEV);

    /* ppp1 */
    dev_add(VF_UK_PPPOA_DATA_WAN_DEV, DEV_IF_PPPOA, DEV_IF_NET_EXT);
    /* depends on bcm_atm0 */
    dev_set_dependency(VF_UK_PPPOA_DATA_WAN_DEV, VF_UK_ATM_DEV);

    /* ips0 */
    dev_add(VF_UK_VPN_IPSEC_TUNNEL_DEV, DEV_IF_IPSEC_CONN, DEV_IF_NET_EXT);
    /* ips1 */
    dev_add(VF_UK_VPN_IPSEC_TRANSPORT_DEV, DEV_IF_IPSEC_CONN, DEV_IF_NET_EXT);

    /* ppp300 */
    dev_add(VF_UK_VPN_L2TPC_DEV, DEV_IF_L2TPC, DEV_IF_NET_EXT);
    /* depends on ips1 */
    dev_set_dependency(VF_UK_VPN_L2TPC_DEV, VF_UK_VPN_IPSEC_TRANSPORT_DEV);

    /* ppp200 */
    dev_add(VF_UK_VPN_PPTPC_DEV, DEV_IF_PPTPC, DEV_IF_NET_EXT);
}

void set_vodafone_uk_configs(void *lan_dev_name, void *eth_wan_dev_name,
    void *ptm_wan_dev_name, char *wifi_dev)
{
    token_set("CONFIG_VF_WAN_DATA_VLAN", "101");
    token_set("CONFIG_VF_WAN_VOICE_VLAN", "101");
    token_set("CONFIG_RG_LANGUAGES", "DEF MSG_CODE");
    token_set("CONFIG_RG_DEFAULT_LANGUAGE", "DEF");
    token_set_y("CONFIG_RG_OPENREACH_COMPLIANT");

    if (token_get("CONFIG_RG_ROUTED_LAN_VLAN_SEPARATION"))
    {
	token_set("CONFIG_RG_ROUTED_LAN_HOME_VLAN_DEV", "data_lan0");
	dev_add(token_getz("CONFIG_RG_ROUTED_LAN_HOME_VLAN_DEV"),
	    DEV_IF_VLAN, DEV_IF_NET_INT);
	dev_set_dependency(token_getz("CONFIG_RG_ROUTED_LAN_HOME_VLAN_DEV"),
	    lan_dev_name);

	token_set("CONFIG_RG_ROUTED_LAN_SEP_VLAN_DEV", "data_lan1");
	dev_add(token_getz("CONFIG_RG_ROUTED_LAN_SEP_VLAN_DEV"),
	    DEV_IF_USER_VLAN, DEV_IF_NET_INT);
	dev_set_dependency(token_getz("CONFIG_RG_ROUTED_LAN_SEP_VLAN_DEV"),
	    lan_dev_name);
    }

    set_vodafone_it_uk_configs(lan_dev_name, eth_wan_dev_name, ptm_wan_dev_name,
	wifi_dev);
    if (token_get("CONFIG_VF_WBM_INTERNATIONAL"))
    {
	/* Common LED features */
	token_set_y("CONFIG_RG_LEDS_PROXIMITY_OVERRIDE");
	token_set_y("CONFIG_RG_LEDS_BOTTOM_INTENSITY");
    }
    token_set_y("CONFIG_RG_USER_FIRMWARE_UPGRADE");
    token_set_y("CONFIG_RG_LOG_PROJ_MSGS_PPPOE");
    token_set_y("CONFIG_RG_LOG_PROJ_MSGS_PPP_AUTH");
}

void set_vodafone_pt_configs(void *lan_dev_name, char *wifi_dev)
{
    token_set("CONFIG_VF_DATA_VLAN", "1");
    token_set("CONFIG_VF_WAN_DATA_VLAN", "100");
    token_set("CONFIG_VF_WAN_VOICE_VLAN", "101");
    token_set("CONFIG_VF_WAN_IPTV_VLAN", "105");

    token_set("CONFIG_VF_DATA_LAN_DEV", "data_lan");
    dev_add(token_getz("CONFIG_VF_DATA_LAN_DEV"),
	DEV_IF_USER_VLAN, DEV_IF_NET_INT);
    dev_set_dependency(token_getz("CONFIG_VF_DATA_LAN_DEV"), lan_dev_name);

    token_set("CONFIG_VF_ETH_DATA_WAN_DEV", "data_wan");
    dev_add(token_getz("CONFIG_VF_ETH_DATA_WAN_DEV"), 
	DEV_IF_USER_VLAN, DEV_IF_NET_EXT);
    dev_set_dependency(token_getz("CONFIG_VF_ETH_DATA_WAN_DEV"), lan_dev_name);

    token_set("CONFIG_VF_ETH_VOICE_WAN_DEV", "voice_wan");
    dev_add(token_getz("CONFIG_VF_ETH_VOICE_WAN_DEV"), 
	DEV_IF_USER_VLAN, DEV_IF_NET_EXT);
    dev_set_dependency(token_getz("CONFIG_VF_ETH_VOICE_WAN_DEV"), lan_dev_name);

    token_set("CONFIG_VF_ETH_IPTV_WAN_DEV", "iptv_wan");
    dev_add(token_getz("CONFIG_VF_ETH_IPTV_WAN_DEV"), 
	DEV_IF_USER_VLAN, DEV_IF_NET_EXT);
    dev_set_dependency(token_getz("CONFIG_VF_ETH_IPTV_WAN_DEV"), lan_dev_name);

    dev_add_bridge("br0", DEV_IF_NET_INT, token_getz("CONFIG_VF_DATA_LAN_DEV"),
	NULL);
    if (wifi_dev)
	dev_add_to_bridge("br0", wifi_dev);
    token_set("CONFIG_VF_LAN_UPD_DEV", "br0");

    /* Set wireless region */
    token_set("CONFIG_RG_WIRELESS_COUNTRY_REGION", "PORTUGAL");
    token_set("CONFIG_RG_WIRELESS_COUNTRY_CODE", "EU");

    token_set("CONFIG_RG_LANGUAGES", "DEF pt");
    token_set("CONFIG_RG_DEFAULT_LANGUAGE", "pt");

    token_set_y("CONFIG_RG_VAP_SECURED");
    
    /* Remote access */
    token_set_y("CONFIG_RG_VODAFONE_REMOTE_ACCESS");

    token_set("CONFIG_RG_VODAFONE_ALLOWED_WAN", "DETECTING | ADSL | FTTH");

    /* VPN server */
    enable_module("MODULE_RG_PPTP");
    enable_module("MODULE_RG_L2TP");
    enable_module("MODULE_RG_IPSEC");

    enable_module("MODULE_RG_SNMP");

    token_set_y("CONFIG_RG_SSH_SERVER");
    token_set("CONFIG_RG_VOIP_DEFLAW", "alaw");
}

void set_vodafone_es_configs(void *lan_dev_name, char *wifi_dev)
{
    dev_add_bridge("br0", DEV_IF_NET_INT, lan_dev_name, NULL);
    if (wifi_dev)
	dev_add_to_bridge("br0", wifi_dev);

    /* Set wireless region */
    token_set("CONFIG_RG_WIRELESS_COUNTRY_REGION", "SPAIN");
    token_set("CONFIG_RG_WIRELESS_COUNTRY_CODE", "EU");

    token_set("CONFIG_RG_LANGUAGES", "DEF es");
    token_set("CONFIG_RG_DEFAULT_LANGUAGE", "es");

    enable_module("MODULE_RG_SNMP");
    token_set_y("CONFIG_RG_VAP_SECURED");

    token_set_y("CONFIG_RG_TCPDUMP");
    token_set("CONFIG_RG_VOIP_DEFLAW", "alaw");
    token_set("CONFIG_VF_LAN_UPD_DEV", "br0");

    /* Audio Codecs */
    token_set_y("CONFIG_RG_VOIP_ASTERISK_CODEC_ALAW");
}

void set_vodafone_de_configs(char *lan_dev, char *lan_upd_dev, char *switch_dev,
    char *wifi_dev, char *wifi2_dev)
{
    token_set_y("CONFIG_RG_VODAFONE_DE");

    /* Set wireless region */
    token_set("CONFIG_RG_WIRELESS_COUNTRY_REGION", "GERMANY");
    token_set("CONFIG_RG_WIRELESS_COUNTRY_CODE", "EU");

    if (token_get("CONFIG_HW_LCD"))
    {
	token_set_y("CONFIG_RG_VODAFONE_LCD");
	token_set_y("CONFIG_RG_VODAFONE_LCD_GUI");
	token_set_y("CONFIG_RG_LIBDISKO");
	token_set_y("CONFIG_HW_LCD_SCREEN_SAVER");
	token_set_y("CONFIG_RG_VODAFONE_LCD_LOGO");
	token_set("CONFIG_RG_VODAFONE_LCD_LOGO_ANIMATION_FPS", "22");
	token_set("CONFIG_RG_VODAFONE_LCD_LOGO_ANIMATION_DIR",
	    "animation_71_frames");
	token_set_y("CONFIG_RG_VODAFONE_LCD_BOOTING_ANIMATION");
	token_set("CONFIG_RG_VODAFONE_LCD_BOOTING_ANIMATION_DIR",
	    "boot_animation_de");
	token_set_y("CONFIG_RG_VODAFONE_LCD_WELCOME_SCREEN");
	token_set_y("CONFIG_RG_LCD_LANGUAGE_SECTION");
    }

    if (!IS_DIST("VOX_2.5_DE_VDSL_IN_MAKING"))
    {
    token_set("CONFIG_RG_LANGUAGES", "DEF de tr MSG_CODE");
    token_set("CONFIG_RG_DEFAULT_LANGUAGE", "de");
    token_set_y("CONFIG_RG_VODAFONE_USB_UPGRADE");
    token_set_y("CONFIG_RG_SSH_SERVER");
    token_set("CONFIG_RG_SSH_SERVER_CWMP_SRV", "n");
    token_set_y("CONFIG_RG_SCP");
    token_set_y("CONFIG_RG_SSH_AUTHORIZED_KEYS");
    token_set_y("CONFIG_RG_SSH_BY_KEY_ENABLE");
    token_set_y("CONFIG_RG_WEB_SERVICE");
    token_set("CONFIG_RG_VOIP_DEFLAW", "alaw");
    token_set("CONFIG_VF_LAN_UPD_DEV", lan_upd_dev);
    dev_add_bridge("br0", DEV_IF_NET_INT, lan_dev, NULL);
    dev_add_bridge("br2", DEV_IF_NET_INT, NULL);
    token_set_y("CONFIG_RG_VODAFONE_ACTIVATION_WIZARD");
    token_set_y("CONFIG_RG_RINGING_SCHEDULE");
    token_set_y("CONFIG_RG_SPEED_DIAL");
    token_set("CONFIG_RG_VOIP_ASTERISK_FFS_STORAGE", "y");
    token_set("CONFIG_RG_VOIP_ASTERISK_DISK_STORAGE", "n");
    token_set("CONFIG_RG_VOIP_ASTERISK_FFS_STORAGE_PARTITION", "FFS");
    token_set_y("CONFIG_RG_VOIP_ASTERISK_VENDOR_SOUNDS");
    token_set("CONFIG_RG_VOIP_ASTERISK_CALLER_ID_PREFIX", "00");
    token_set_y("CONFIG_RG_CWMP_RANDOM_PORT");
    token_set_y("CONFIG_USB_MNG");
    token_set_y("CONFIG_RG_VODAFONE_CCF_CONSISTENCY_CHECK");
    token_set_y("CONFIG_RG_VODAFONE_DE_ETH_PROVISIONING_HACK");

    /* Audio Codecs */
    token_set_y("CONFIG_RG_VOIP_ASTERISK_CODEC_ALAW");
    token_set_y("CONFIG_RG_VOIP_ASTERISK_CODEC_AMULAW");
    token_set_y("CONFIG_RG_VOIP_ASTERISK_CODEC_G726");

    /* Certificates */
    token_set("CONFIG_RG_ROOT_CERT_NAME", "EasyBox Root CA");

    /* JS compression */
    token_set_y("CONFIG_RG_COMPRESSED_JS");
    }

    token_set("CONFIG_VF_WLAN_DEV", wifi_dev);
    if (wifi2_dev)
	token_set("CONFIG_VF_WLAN2_DEV", wifi2_dev);
    token_set("CONFIG_VF_SWITCH_LAN_DEV", switch_dev);
    token_set_y("CONFIG_RG_DSLHOME_STRICT_CONFIDENTIAL_EMPTY");
    /* Remove time consuming sub-trees from IGD.* query */
    token_set_y("CONFIG_RG_DSLHOME_REDUCED_PARAMS");

    token_set_y("CONFIG_RG_NAT_AWARE_WAN");
    token_set_y("CONFIG_RG_LIBCAPTCHA");
    token_set_y("CONFIG_VF_WBM_INTERNATIONAL");
    token_set_y("CONFIG_RG_FIREWALL_ALERTS");
    token_set_y("CONFIG_RG_TCPDUMP");
    /* XXX: This test is probably used on the VOX 2.0 DE project only */
    token_set_y("CONFIG_RG_RSS_TEST");
    
    /* sip proxy */
    token_set_y("CONFIG_RG_SIP_PROXY");
    token_set_y("CONFIG_RG_VODAFONE_DATA_LAN_VOIP_EXTENSIONS");
    token_set("CONFIG_RG_MAX_NUMBER_OF_SIP_EXTENSIONS", "3");
    token_set("CONFIG_RG_BASE_SIP_EXTENSION_NAME", "401");

    token_set("CONFIG_RG_VENDOR_OUI", "E448C7");

    if ((IS_DIST("VOX_2.5_DE_VDSL") || IS_DIST("VOX_2.5_DE_VDSL_26") ||
	IS_DIST("VOX_2.5_DE_VDSL_3X")) &&
	token_get("CONFIG_RG_VAP_GUEST") &&
	token_get("CONFIG_RG_DUAL_CONCURRENT_WLAN") &&
	token_get_str("CONFIG_VF_WLAN2_DEV"))
    {
	char vap_name[16];

	dev_add_bridge("br2", DEV_IF_NET_INT, NULL);
	sprintf(vap_name, "%s.1", token_get_str("CONFIG_VF_WLAN_DEV"));
	dev_add_to_bridge("br2", vap_name);
	sprintf(vap_name, "%s.1", token_get_str("CONFIG_VF_WLAN2_DEV"));
	dev_add_to_bridge("br2", vap_name);
    }
    
    if (!IS_DIST("VOX_2.5_DE_VDSL") && !IS_DIST("VOX_2.5_DE_VDSL_IN_MAKING") &&
	!IS_DIST("VOX_2.5_DE_VDSL_26") && !IS_DIST("VOX_2.5_DE_VDSL_3X"))
    {
	token_set_y("CONFIG_RG_VODAFONE_REMOTE_ACCESS");
	token_set_y("CONFIG_RG_VOIP_VOICEMAIL");
	token_set_y("CONFIG_RG_VOIP_DSP_TRANSCODING");
	token_set_y("CONFIG_RG_OPENVPN");

	/* Samba configs */
	token_set_y("CONFIG_RG_SMB_RESTRICTED_SHARE_VISIBILITY");
	token_set_y("CONFIG_RG_SMB_RESTRICT_HOMES");
	token_set_y("CONFIG_RG_DISK_MNG_MOUNT_PER_USER");
	token_set_y("CONFIG_RG_DISK_MNG_MOUNT_PER_USER_FTP");
    
	/* WebDav */ 
	token_set_y("CONFIG_RG_WEBDAV");

	token_set_y("CONFIG_RG_ALARM_CLOCK");
    }

    token_set("CONFIG_RG_HW_RESET_TO_DEF_TIMEOUT_MS", "2500");

    if (IS_DIST("VOX_2.5_DE_VDSL") || IS_DIST("VOX_2.5_DE_VDSL_26") ||
	IS_DIST("VOX_2.5_DE_VDSL_3X"))
    {
	token_set_y("CONFIG_RG_ADDRESS_BOOK");
    }
    if (token_get("CONFIG_RG_DSLHOME_DATAMODEL_TR_181"))
	token_set_y("CONFIG_RG_IPV6_WBM");
}

void set_vodafone_de_vdsl_configs(char *dsl_dev, char *atm_dev, char *ptm_dev,
    char *eth_dev)
{
#define VF_DE_VDSL_ATM atm_dev
#define VF_DE_VDSL_PTM ptm_dev
#define VF_DE_VDSL_ETH eth_dev
#define VF_DE_VDSL_ATM_PPP_DATA "ppp0"
#define VF_DE_VDSL_ATM_PPP_VOICE "ppp1"
#define VF_DE_VDSL_ATM_VW_IPTV "vw0"
#define VF_DE_VDSL_PTM_PPP_DATA "ppp2"
#define VF_DE_VDSL_PTM_PPP_VOICE "ppp3"
#define VF_DE_VDSL_PTM_VW_IPTV "vw1"
#define VF_DE_VDSL_ETH_PPP_DATA "ppp4"
#define VF_DE_VDSL_ETH_PPP_VOICE "ppp5"
#define VF_DE_VDSL_ETH_VW_IPTV "vw2"
#define VF_DE_VDSL_ETHOA_DATA "ethoa0"
#define VF_DE_VDSL_ETHOA_VOICE "ethoa1"
#define VF_DE_VDSL_ETHOA_IPTV "ethoa2"
#define VF_DE_VDSL_PTM_DATA_WAN "data_wan0"
#define VF_DE_VDSL_PTM_VOICE_WAN "voice_wan0"
#define VF_DE_VDSL_PTM_IPTV_WAN "iptv_wan0"
#define VF_DE_VDSL_ETH_DATA_WAN "data_wan1"
#define VF_DE_VDSL_ETH_VOICE_WAN "voice_wan1"
#define VF_DE_VDSL_ETH_IPTV_WAN "iptv_wan1"
#define VF_DE_VDSL_PTM_BITSTREAM_DATA_WAN "data_wan2"
#define VF_DE_VDSL_PTM_BITSTREAM_PPP_DATA "ppp6"
#define VF_DE_VDSL_ATM_BITSTREAM_DATA_WAN "data_wan3"
#define VF_DE_VDSL_ATM_BITSTREAM_PPP_DATA "ppp7"

    token_set("CONFIG_VF_ETH_WAN_DEV", VF_DE_VDSL_ETH); 
    token_set("CONFIG_VF_ETH_DATA_WAN_DEV", VF_DE_VDSL_ETH_DATA_WAN); 
    token_set("CONFIG_VF_ETH_VOICE_WAN_DEV", VF_DE_VDSL_ETH_VOICE_WAN); 
    token_set("CONFIG_VF_ETH_IPTV_WAN_DEV", VF_DE_VDSL_ETH_IPTV_WAN); 
    
    token_set("CONFIG_VF_DSL_DEV", dsl_dev); 

    /* DNS */
    token_set_y("CONFIG_RG_DNS_DOMAIN_ROUTING");

    token_set_m("CONFIG_RG_VIRTUAL_WAN");
    
    /* IPTV */
    token_set_y("CONFIG_RG_VODAFONE_IPTV_PUSH_SERVICE");

    /* ATM */

    dev_add(VF_DE_VDSL_ETHOA_DATA, DEV_IF_ETHOA, DEV_IF_NET_EXT);
    dev_set_dependency(VF_DE_VDSL_ETHOA_DATA, VF_DE_VDSL_ATM);
    dev_set_logical_dependency(VF_DE_VDSL_ETHOA_DATA, VF_DE_VDSL_ATM);
    dev_add(VF_DE_VDSL_ATM_PPP_DATA, DEV_IF_PPPOE, DEV_IF_NET_EXT);
    dev_set_dependency(VF_DE_VDSL_ATM_PPP_DATA, VF_DE_VDSL_ETHOA_DATA);
    dev_set_logical_dependency(VF_DE_VDSL_ATM_PPP_DATA, VF_DE_VDSL_ETHOA_DATA);

    dev_add(VF_DE_VDSL_ETHOA_VOICE, DEV_IF_ETHOA, DEV_IF_NET_EXT);
    dev_set_dependency(VF_DE_VDSL_ETHOA_VOICE, VF_DE_VDSL_ATM);
    dev_set_logical_dependency(VF_DE_VDSL_ETHOA_VOICE, VF_DE_VDSL_ATM);
    dev_add(VF_DE_VDSL_ATM_PPP_VOICE, DEV_IF_PPPOE, DEV_IF_NET_EXT);
    dev_set_dependency(VF_DE_VDSL_ATM_PPP_VOICE, VF_DE_VDSL_ETHOA_VOICE);
    dev_set_logical_dependency(VF_DE_VDSL_ATM_PPP_VOICE, 
	VF_DE_VDSL_ETHOA_VOICE);

    dev_add(VF_DE_VDSL_ETHOA_IPTV, DEV_IF_ETHOA, DEV_IF_NET_EXT);
    dev_set_dependency(VF_DE_VDSL_ETHOA_IPTV, VF_DE_VDSL_ATM);
    dev_set_logical_dependency(VF_DE_VDSL_ETHOA_IPTV, VF_DE_VDSL_ATM);
    dev_add(VF_DE_VDSL_ATM_VW_IPTV, DEV_IF_VIRTUAL_WAN, DEV_IF_NET_EXT);
    dev_set_dependency(VF_DE_VDSL_ATM_VW_IPTV, VF_DE_VDSL_ETHOA_IPTV);
    dev_set_logical_dependency(VF_DE_VDSL_ATM_VW_IPTV, VF_DE_VDSL_ETHOA_IPTV);

    dev_add(VF_DE_VDSL_ATM_BITSTREAM_DATA_WAN, DEV_IF_USER_VLAN,
	DEV_IF_NET_EXT);
    dev_set_dependency(VF_DE_VDSL_ATM_BITSTREAM_DATA_WAN,
	VF_DE_VDSL_ETHOA_DATA);
    dev_set_logical_dependency(VF_DE_VDSL_ATM_BITSTREAM_DATA_WAN,
	VF_DE_VDSL_ETHOA_DATA);
    dev_add(VF_DE_VDSL_ATM_BITSTREAM_PPP_DATA, DEV_IF_PPPOE, DEV_IF_NET_EXT);
    dev_set_dependency(VF_DE_VDSL_ATM_BITSTREAM_PPP_DATA,
	VF_DE_VDSL_ATM_BITSTREAM_DATA_WAN);
    dev_set_logical_dependency(VF_DE_VDSL_ATM_BITSTREAM_PPP_DATA,
	VF_DE_VDSL_ETHOA_DATA);

    /* PTM */

    dev_add(VF_DE_VDSL_PTM_DATA_WAN, DEV_IF_USER_VLAN, DEV_IF_NET_EXT);
    dev_set_dependency(VF_DE_VDSL_PTM_DATA_WAN, VF_DE_VDSL_PTM);
    dev_set_logical_dependency(VF_DE_VDSL_PTM_DATA_WAN, VF_DE_VDSL_PTM);
    dev_add(VF_DE_VDSL_PTM_PPP_DATA, DEV_IF_PPPOE, DEV_IF_NET_EXT);
    dev_set_dependency(VF_DE_VDSL_PTM_PPP_DATA, VF_DE_VDSL_PTM_DATA_WAN);
    dev_set_logical_dependency(VF_DE_VDSL_PTM_PPP_DATA,
	VF_DE_VDSL_PTM_DATA_WAN);

    dev_add(VF_DE_VDSL_PTM_VOICE_WAN, DEV_IF_USER_VLAN, DEV_IF_NET_EXT);
    dev_set_dependency(VF_DE_VDSL_PTM_VOICE_WAN, VF_DE_VDSL_PTM);
    dev_set_logical_dependency(VF_DE_VDSL_PTM_VOICE_WAN, VF_DE_VDSL_PTM);
    dev_add(VF_DE_VDSL_PTM_PPP_VOICE, DEV_IF_PPPOE, DEV_IF_NET_EXT);
    dev_set_dependency(VF_DE_VDSL_PTM_PPP_VOICE, VF_DE_VDSL_PTM_VOICE_WAN);
    dev_set_logical_dependency(VF_DE_VDSL_PTM_PPP_VOICE, 
	VF_DE_VDSL_PTM_VOICE_WAN);

    dev_add(VF_DE_VDSL_PTM_IPTV_WAN, DEV_IF_USER_VLAN, DEV_IF_NET_EXT);
    dev_set_dependency(VF_DE_VDSL_PTM_IPTV_WAN, VF_DE_VDSL_PTM);
    dev_set_logical_dependency(VF_DE_VDSL_PTM_IPTV_WAN, VF_DE_VDSL_PTM);
    dev_add(VF_DE_VDSL_PTM_VW_IPTV, DEV_IF_VIRTUAL_WAN, DEV_IF_NET_EXT);
    dev_set_dependency(VF_DE_VDSL_PTM_VW_IPTV, VF_DE_VDSL_PTM_IPTV_WAN);
    dev_set_logical_dependency(VF_DE_VDSL_PTM_VW_IPTV, VF_DE_VDSL_PTM_IPTV_WAN);

    dev_add(VF_DE_VDSL_PTM_BITSTREAM_DATA_WAN, DEV_IF_USER_VLAN,
        DEV_IF_NET_EXT);
    dev_set_dependency(VF_DE_VDSL_PTM_BITSTREAM_DATA_WAN, VF_DE_VDSL_PTM);
    dev_set_logical_dependency(VF_DE_VDSL_PTM_BITSTREAM_DATA_WAN,
	VF_DE_VDSL_PTM);
    dev_add(VF_DE_VDSL_PTM_BITSTREAM_PPP_DATA, DEV_IF_PPPOE, DEV_IF_NET_EXT);
    dev_set_dependency(VF_DE_VDSL_PTM_BITSTREAM_PPP_DATA,
        VF_DE_VDSL_PTM_BITSTREAM_DATA_WAN);
    dev_set_logical_dependency(VF_DE_VDSL_PTM_BITSTREAM_PPP_DATA,
        VF_DE_VDSL_PTM_BITSTREAM_DATA_WAN);

    /* ETH */

    dev_add(VF_DE_VDSL_ETH_DATA_WAN, DEV_IF_USER_VLAN, DEV_IF_NET_EXT);
    dev_set_dependency(VF_DE_VDSL_ETH_DATA_WAN, VF_DE_VDSL_ETH);
    dev_set_logical_dependency(VF_DE_VDSL_ETH_DATA_WAN, VF_DE_VDSL_ETH);
    dev_add(VF_DE_VDSL_ETH_PPP_DATA, DEV_IF_PPPOE, DEV_IF_NET_EXT);
    dev_set_dependency(VF_DE_VDSL_ETH_PPP_DATA, VF_DE_VDSL_ETH_DATA_WAN);
    dev_set_logical_dependency(VF_DE_VDSL_ETH_PPP_DATA,
	VF_DE_VDSL_ETH_DATA_WAN);

    dev_add(VF_DE_VDSL_ETH_VOICE_WAN, DEV_IF_USER_VLAN, DEV_IF_NET_EXT);
    dev_set_dependency(VF_DE_VDSL_ETH_VOICE_WAN, VF_DE_VDSL_ETH);
    dev_set_logical_dependency(VF_DE_VDSL_ETH_VOICE_WAN, VF_DE_VDSL_ETH);
    dev_add(VF_DE_VDSL_ETH_PPP_VOICE, DEV_IF_PPPOE, DEV_IF_NET_EXT);
    dev_set_dependency(VF_DE_VDSL_ETH_PPP_VOICE, VF_DE_VDSL_ETH_VOICE_WAN);
    dev_set_logical_dependency(VF_DE_VDSL_ETH_PPP_VOICE, 
	VF_DE_VDSL_ETH_VOICE_WAN);

    dev_add(VF_DE_VDSL_ETH_IPTV_WAN, DEV_IF_USER_VLAN, DEV_IF_NET_EXT);
    dev_set_dependency(VF_DE_VDSL_ETH_IPTV_WAN, VF_DE_VDSL_ETH);
    dev_set_logical_dependency(VF_DE_VDSL_ETH_IPTV_WAN, VF_DE_VDSL_ETH);
    dev_add(VF_DE_VDSL_ETH_VW_IPTV, DEV_IF_VIRTUAL_WAN, DEV_IF_NET_EXT);
    dev_set_dependency(VF_DE_VDSL_ETH_VW_IPTV, VF_DE_VDSL_ETH_IPTV_WAN);
    dev_set_logical_dependency(VF_DE_VDSL_ETH_VW_IPTV, VF_DE_VDSL_ETH_IPTV_WAN);
    
    token_set_y("CONFIG_RG_VODAFONE_WAN_AUTO_RECONNECT");
    token_set("CONFIG_RG_VLAN_8021Q_MAX", "3");
}

void set_vodafone_de_vdsl_configs_short(char *dsl_dev, char *atm_dev,
    char *ptm_dev, char *eth_dev)
{
#define VF_DE_VDSL_ETH eth_dev
#define VF_DE_VDSL_ATM atm_dev
#define VF_DE_VDSL_ATM_PPP_DATA "ppp0"
#define VF_DE_VDSL_ETH_PPP_DATA "ppp4"
#define VF_DE_VDSL_ETHOA_DATA "ethoa0"
#define VF_DE_VDSL_ETH_DATA_WAN "data_wan1"

    token_set("CONFIG_VF_ETH_WAN_DEV", VF_DE_VDSL_ETH); 
    token_set("CONFIG_VF_ETH_DATA_WAN_DEV", VF_DE_VDSL_ETH_DATA_WAN); 

    token_set("CONFIG_VF_DSL_DEV", dsl_dev); 

    /* DNS */
    token_set_y("CONFIG_RG_DNS_DOMAIN_ROUTING");

    token_set_m("CONFIG_RG_VIRTUAL_WAN");

    /* ATM */
    dev_add(VF_DE_VDSL_ETHOA_DATA, DEV_IF_ETHOA, DEV_IF_NET_EXT);
    dev_set_dependency(VF_DE_VDSL_ETHOA_DATA, VF_DE_VDSL_ATM);
    dev_set_logical_dependency(VF_DE_VDSL_ETHOA_DATA, VF_DE_VDSL_ATM);
    dev_add(VF_DE_VDSL_ATM_PPP_DATA, DEV_IF_PPPOE, DEV_IF_NET_EXT);
    dev_set_dependency(VF_DE_VDSL_ATM_PPP_DATA, VF_DE_VDSL_ETHOA_DATA);
    dev_set_logical_dependency(VF_DE_VDSL_ATM_PPP_DATA, VF_DE_VDSL_ETHOA_DATA);

    /* ETH */

    dev_add(VF_DE_VDSL_ETH_DATA_WAN, DEV_IF_USER_VLAN, DEV_IF_NET_EXT);
    dev_set_dependency(VF_DE_VDSL_ETH_DATA_WAN, VF_DE_VDSL_ETH);
    dev_set_logical_dependency(VF_DE_VDSL_ETH_DATA_WAN, VF_DE_VDSL_ETH);
    dev_add(VF_DE_VDSL_ETH_PPP_DATA, DEV_IF_PPPOE, DEV_IF_NET_EXT);
    dev_set_dependency(VF_DE_VDSL_ETH_PPP_DATA, VF_DE_VDSL_ETH_DATA_WAN);
    dev_set_logical_dependency(VF_DE_VDSL_ETH_PPP_DATA,
	VF_DE_VDSL_ETH_DATA_WAN);    

    token_set("CONFIG_RG_VLAN_8021Q_MAX", "3");
}

void distribution_features(void)
{
    if (!dist)
	conf_err("ERROR: DIST is not defined\n");

    /* MIPS */
    if (IS_DIST("ARX188_LSP"))
    {
	hw = "ARX188";
	os = "LINUX_26";

	token_set_y("CONFIG_LSP_DIST");
	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set("LIBC_IN_TOOLCHAIN", "y");
	
	token_set_y("CONFIG_HW_ETH_LAN");
	token_set_y("CONFIG_HW_ETH_WAN");
    }
    else if (IS_DIST("ARX188") || IS_DIST("ARX188_ETH"))
    {
	hw = "ARX188";
	os = "LINUX_26";

	token_set("CONFIG_RG_JPKG_DIST", "JPKG_MIPSEB_XWAY");

	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MODULES_INITRAMFS");
	token_set_y("CONFIG_RG_MAINFS_IN_PHYS_MAPPED_FLASH");
	
	/*  RG Priority 1  */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_FILESERVER");

	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_VLAN");
	if (!IS_DIST("ARX188_ETH"))
	    enable_module("MODULE_RG_DSL");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_IPSEC");
	enable_module("CONFIG_HW_ENCRYPTION");

	/* Devices */
	token_set_y("CONFIG_HW_ETH_LAN");
	/* Ethernet WAN is available in ADSL distribution as
	 * well, however, it cannot be accelerated.
	 */
	token_set_y("CONFIG_HW_ETH_WAN");
        if (token_get("MODULE_RG_DSL"))
            token_set_y("CONFIG_HW_DSL_WAN");

	/* this enslaves all LAN devices, if no other device is enslaved
	 * explicitly */
	dev_add_bridge("br0", DEV_IF_NET_INT, "eth0", NULL);
	enable_module("CONFIG_RG_ATHEROS_HW_AR5416");
        token_set_y("CONFIG_RG_ATHEROS_FUSION");
	token_set_y("CONFIG_ATHEROS_CONSERVE_RAM");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");

	/* USB */
	enable_module("CONFIG_HW_USB_STORAGE");

	/* Voice */
	enable_module("MODULE_RG_HOME_PBX");
	enable_module("MODULE_RG_VOIP_ASTERISK_SIP");
	enable_module("CONFIG_HW_DSP");
	
	/* ISDN daughter card with IPAC-X chip */
	if (token_get("CONFIG_HW_ISDN"))
	{
	    token_set_y("CONFIG_RG_VOIP_ASTERISK_CAPI");
	    token_set_y("CONFIG_NETBRICKS_ISDN_STACK");
	}

	/* Fast Path */
	enable_module("MODULE_RG_PSE");
	if (token_get("MODULE_RG_PSE"))
	    token_set_y("CONFIG_RG_PSE_HW");

	/* build options */
	//token_set_y("CONFIG_RG_FOOTPRINT_REDUCTION");
	token_set("LIBC_IN_TOOLCHAIN", "n");
	token_set_y("CONFIG_ULIBC");
    }
    else if (IS_DIST("ARX188_IPV6") || IS_DIST("ARX188_ETH_IPV6"))
    {
	hw = "ARX188";
	os = "LINUX_26";

	token_set("CONFIG_RG_JPKG_DIST", "JPKG_MIPSEB_XWAY");

	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MODULES_INITRAMFS");
	token_set_y("CONFIG_RG_MAINFS_IN_PHYS_MAPPED_FLASH");
	
	/*  RG Priority 1  */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_DSLHOME");

	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_URL_FILTERING");

	/* Devices */
	token_set_y("CONFIG_HW_ETH_LAN");
	/* Ethernet WAN is available in ADSL distribution as
	 * well, however, it cannot be accelerated.
	 */
	token_set_y("CONFIG_HW_ETH_WAN");

	/* this enslaves all LAN devices, if no other device is enslaved
	 * explicitly */
	dev_add_bridge("br0", DEV_IF_NET_INT, "eth0", NULL);

	/* Wireless devices */
	enable_module("CONFIG_RG_ATHEROS_HW_AR5416");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");

	/* IPv6 */
	enable_module("MODULE_RG_IPV6");
	token_set_y("CONFIG_RG_IPROUTE2_UTILS");

	/* USB */
	enable_module("CONFIG_HW_USB_STORAGE");	

	if (!IS_DIST("ARX188_ETH_IPV6"))
	    enable_module("MODULE_RG_DSL");
        if (token_get("MODULE_RG_DSL"))
            token_set_y("CONFIG_HW_DSL_WAN");

	/* Fast Path */
	enable_module("MODULE_RG_PSE");
	if (token_get("MODULE_RG_PSE"))
	    token_set_y("CONFIG_RG_PSE_HW");

	/* build options */
	token_set("LIBC_IN_TOOLCHAIN", "y");
    }
    else if (IS_DIST("VRX288_LSP") || IS_DIST("GRX288_LSP"))
    {
	if (IS_DIST("VRX288_LSP"))
	    hw = "VRX288";
	else
	    hw = "GRX288";
	os = "LINUX_26";

	token_set_y("CONFIG_LSP_DIST");
	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set("LIBC_IN_TOOLCHAIN", "y");
	
	token_set_y("CONFIG_HW_ETH_LAN");
	if (IS_DIST("VRX288_LSP"))
	    token_set_y("CONFIG_HW_VDSL_WAN");
    }
    else if (IS_DIST("VRX288") || IS_DIST("GRX288") ||
	IS_DIST("GRX288_VODAFONE_DE"))
    {
	if (IS_DIST("VRX288"))
	    hw = "VRX288";
	else
	    hw = "GRX288";
	os = "LINUX_26";

	token_set("CONFIG_RG_JPKG_DIST", "JPKG_MIPSEB_XWAY");

	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MODULES_INITRAMFS");
	token_set_y("CONFIG_RG_MAINFS_IN_PHYS_MAPPED_FLASH");
	
	/* Devices */
	token_set_y("CONFIG_HW_ETH_LAN");
	token_set_y("CONFIG_HW_ETH_WAN");
	if (IS_DIST("VRX288"))
	{
	    token_set_y("CONFIG_HW_VDSL_WAN");
	    token_set_y("CONFIG_HW_DSL_WAN");
	    enable_module("MODULE_RG_DSL");
	}

	/*  RG Priority 1  */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	/* Not tested
	 * enable_module("MODULE_RG_REDUCE_SUPPORT");
	 */
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_PPP");

	/*  RG Priority 2  */
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_HOME_PBX");
	enable_module("MODULE_RG_VOIP_ASTERISK_SIP");
	enable_module("CONFIG_HW_DSP");
	enable_module("MODULE_RG_FILESERVER");
	enable_module("CONFIG_HW_USB_STORAGE");	
	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_PSE");
	if (token_get("MODULE_RG_PSE"))
	    token_set_y("CONFIG_RG_PSE_HW");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");

	/*  RG Priority 3  */
	/* enable_module("MODULE_RG_ACCESS_DLNA"); 
	enable_module("MODULE_RG_MEDIA_SERVER");*/
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_IPSEC");
	enable_module("CONFIG_HW_ENCRYPTION");

	/*  RG Priority 4  */
	enable_module("MODULE_RG_PRINTSERVER");

	/* this enslaves all LAN devices, if no other device is enslaved
	 * explicitly */
	dev_add_bridge("br0", DEV_IF_NET_INT, "eth0", NULL);
        
	/* build options */
	//token_set_y("CONFIG_RG_FOOTPRINT_REDUCTION");
	token_set("LIBC_IN_TOOLCHAIN", "n");
	token_set_y("CONFIG_ULIBC");

	if (IS_DIST("VRX288"))
	{
	    token_set_y("CONFIG_RG_PERM_STORAGE_UBI");

	    /* Disable CONFIG_HW_FLASH_NOR to avoid having 2 OpenRG volumes */
	    token_set("CONFIG_HW_FLASH_NOR", "n");

	    /* Volume size for permst - OpenRG Flash layout */
	    token_set("CONFIG_RG_PERM_STORAGE_UBI_VOLUME_SIZE", "0x33ae000");
	    /* Partion Size ncludes the volume size, 4 PEBs for UBI and 80 
	     * possible bab-blocks (worst case scenario).
	     */
	    token_set("CONFIG_RG_UBI_PARTITION_SIZE", "0x3f00000");
	    /* 1MiB for u-boot and it's data at the beginning of the flash */
	    token_set("CONFIG_RG_UBI_PARTITION_OFFSET", "0x100000");
	}

	if (IS_DIST("GRX288_VODAFONE_DE"))
	{
	    token_set_y("CONFIG_RG_URL_KEYWORD_FILTER");
	    token_set_y("CONFIG_RG_VODAFONE"); 

	    set_vodafone_de_configs("eth0", "eth0", NULL, "ra01_0", NULL);

	    /* 3G Support */
	    token_set_y("CONFIG_RG_3G_WAN"); 
	    token_set_y("CONFIG_RG_3G_SMS");
	    token_set_y("CONFIG_RG_3G_SMS_WHITELIST");
	    token_set_y("CONFIG_RG_3G_VOICE");

	    token_set_y("CONFIG_RG_VOIP_CALL_LOG");
	    token_set_y("CONFIG_RG_MTD_UTILS");
	    token_set_y("CONFIG_RG_FFS");

	    /* WBM */
	    token_set_y("CONFIG_WBM_VODAFONE");

	    token_set_y("CONFIG_BOOTLDR_SIGNED_IMAGES");
	}
    }
    else if (IS_DIST("CN3XXX_LSP"))
    {
	hw = "CN3XXX";
	os = "LINUX_26";

	token_set_y("CONFIG_LSP_DIST");
	
	token_set("LIBC_IN_TOOLCHAIN", "y");
	token_set_y("CONFIG_GLIBC");

	token_set_y("CONFIG_HW_ETH_LAN");

	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MODULES_INITRAMFS");
    }
    else if (IS_DIST("CN3XXX"))
    {
	hw = "CN3XXX";
	os = "LINUX_26";

	token_set("CONFIG_RG_JPKG_DIST", "JPKG_OCTEON");

	token_set_y("CONFIG_RG_SMB");

	/*  SMB Priority 1  */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_FILESERVER");
	enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_SSL_VPN");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_WEB_SERVER");

	enable_module("MODULE_RG_REDUCE_SUPPORT");
	enable_module("MODULE_RG_OSAP_AGENT");

	if (token_get("MODULE_RG_REDUCE_SUPPORT"))
	    enable_module("MODULE_RG_HOME_PBX");
	else
	    enable_module("MODULE_RG_FULL_PBX");

	enable_module("MODULE_RG_VOIP_ASTERISK_SIP"); 

	enable_module("MODULE_RG_PSE");

	/*  SMB Priority 2  */
	enable_module("MODULE_RG_PRINTSERVER");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_ROUTE_MULTIWAN");
	
	/*  SMB Priority 3  */
	enable_module("MODULE_RG_UPNP_AV");
	enable_module("MODULE_RG_BLUETOOTH");
	enable_module("MODULE_RG_RADIUS_SERVER");
	enable_module("MODULE_RG_VIDEO_SURVEILLANCE");
	
	/*  SMB Priority 4  */
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_IPV6");
	if (!token_get("MODULE_RG_REDUCE_SUPPORT"))
	    enable_module("MODULE_RG_VOIP_ASTERISK_MGCP_CALL_AGENT");
	enable_module("MODULE_RG_MAIL_SERVER");
	enable_module("MODULE_RG_FTP_SERVER");
	/* pkg/kaffe doesn't support 64bit */
	//enable_module("MODULE_RG_JVM");

	/*  SMB Priority 5  */
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_TR_064");
	
	token_set_y("CONFIG_RG_FOOTPRINT_REDUCTION");
	token_set_y("CONFIG_RG_NETTOOLS_ARP");
	token_set_y("CONFIG_DYN_LINK");
	token_set_y("CONFIG_RG_IMAGE_ELF");
	
	token_set("LIBC_IN_TOOLCHAIN", "n");
	token_set_y("CONFIG_ULIBC");

	/* HW Configuration Section */

	enable_module("CONFIG_HW_ENCRYPTION");

	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
	enable_module("CONFIG_HW_USB_STORAGE");

	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MODULES_INITRAMFS");
	token_set_y("CONFIG_RG_MAINFS_BUILTIN");

	/* Fast Path */
	if (token_get("MODULE_RG_PSE"))
	    token_set_y("CONFIG_RG_PSE_HW");

	/* Currently we use only 1 8M flash and boot from network. Ignore the
	 * image size
	 */
	token_set_y("CONFIG_RG_IGNORE_IMAGE_SECTION_SIZE");
    }
    else if (IS_DIST("BCM94702"))
    {
	hw = "BCM94702";

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");

	/* OpenRG HW support */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
    }
    else if (IS_DIST("BCM94704"))
    {
	hw = "BCM94704";

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_SNMP");
    	token_set_y("CONFIG_RG_EVENT_LOGGING"); /* Event Logging */
	token_set_y("CONFIG_RG_ENTFY");	/* Email notification */

	/* OpenRG HW support */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
	enable_module("CONFIG_HW_80211G_BCM43XX");

	dev_add_bridge("br0", DEV_IF_NET_INT, "bcm0", "eth0", NULL);
    }
    else if (IS_DIST("USI_BCM94712"))
    {
	hw = "BCM94712";

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
        enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_L2TP");

	/* OpenRG HW support */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
	enable_module("CONFIG_HW_80211G_BCM43XX");

	dev_add_bridge("br0", DEV_IF_NET_INT, "bcm0.0", "eth0", NULL);

	token_set_y("CONFIG_DYN_LINK");
    }
    else if (IS_DIST("SRI_USI_BCM94712"))
    {
	hw = "BCM94712";

	token_set_y("CONFIG_RG_SMB");

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");

	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	enable_module("MODULE_RG_URL_FILTERING");
	token_set("CONFIG_RG_SURFCONTROL_PARTNER_ID", "6003");
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_SNMP");

	/* OpenRG HW support */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
	enable_module("CONFIG_HW_80211G_BCM43XX");
	
	dev_add_bridge("br0", DEV_IF_NET_INT, "bcm0.0", "eth0", NULL);

	/* The Broadcom nas application is dynamically linked */
	token_set_y("CONFIG_DYN_LINK");

	/* Wireless GUI options */
	/* Do NOT show Radius icon in advanced */
	token_set_y("CONFIG_RG_RADIUS_WBM_IN_CONN");
    }
    else if (IS_DIST("WRT54G"))
    {
	hw = "WRT54G";

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
        enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	enable_module("MODULE_RG_SNMP");

	/* OpenRG HW support */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
	enable_module("CONFIG_HW_80211G_BCM43XX");
	token_set_m("CONFIG_HW_BUTTONS");

	dev_add_bridge("br0", DEV_IF_NET_INT, "bcm0.2", "eth0", NULL);

	token_set_y("CONFIG_ARCH_BCM947_CYBERTAN");
	token_set_y("CONFIG_RG_BCM947_NVRAM_CONVERT");
	token_set_y("CONFIG_DYN_LINK");
	token_set_y("CONFIG_GUI_LINKSYS");
    }
    else if ( IS_DIST("PACKET-IAD") || IS_DIST("BB-ROUTER") ||
	IS_DIST("FE-ROUTER"))
    {	
	os = "LINUX_26";

	token_set("CONFIG_RG_JPKG_DIST", "JPKG_ARMV6J");

	/* board */
	if (IS_DIST("PACKET-IAD"))
	    hw = "PACKET-IAD";
	else if (IS_DIST("BB-ROUTER"))
	    hw = "BB-ROUTER";
	else if (IS_DIST("FE-ROUTER"))
	    hw = "FE-ROUTER";

	token_set_y("CONFIG_RG_SMB");
	if (IS_DIST("BB-ROUTER"))
	    token_set_y("CONFIG_RG_MAINFS_IN_PHYS_MAPPED_FLASH");

	/*  SMB Priority 1  */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_FILESERVER");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	enable_module("MODULE_RG_VOIP_ASTERISK_SIP");
        enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_HOME_PBX");
	/* CONFIG_RG_DSLHOME_VOUCHERS is disabled in feature_config */
	enable_module("MODULE_RG_DSLHOME"); 
	enable_module("MODULE_RG_UPNP"); 
	enable_module("MODULE_RG_PSE"); 
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_PPTP");
	/*Not enough space on flash
	enable_module("MODULE_RG_SSL_VPN");	
	enable_module("MODULE_RG_L2TP");
	*/
	
	/*  SMB Priority 2  */
	enable_module("MODULE_RG_VLAN");
	/*Not enough space on flash
	enable_module("MODULE_RG_PRINTSERVER");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_ROUTE_MULTIWAN");
	*/
	
	/*  SMB Priority 3  */
	/*Not enough space on flash
	enable_module("MODULE_RG_BLUETOOTH");
	enable_module("MODULE_RG_RADIUS_SERVER");
	enable_module("MODULE_RG_VIDEO_SURVEILLANCE"); 
	enable_module("MODULE_RG_UPNP_AV");
	*/
	
	/*  SMB Priority 4  */
	/*Not enough space on flash
	enable_module("MODULE_RG_SNMP");	
	enable_module("MODULE_RG_IPV6");
	enable_module("MODULE_RG_VOIP_ASTERISK_MGCP_CALL_AGENT");
	enable_module("MODULE_RG_MAIL_SERVER");
	enable_module("MODULE_RG_JVM");
	enable_module("MODULE_RG_FTP_SERVER");
	*/
	enable_module("MODULE_RG_REDUCE_SUPPORT"); 
	enable_module("MODULE_RG_OSAP_AGENT"); 
	
	/*  SMB Priority 5  */
	/*Not enough space on flash
	enable_module("MODULE_RG_VOIP_ASTERISK_H323");
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_TR_064");
	*/
	
	enable_module("CONFIG_HW_80211N_RALINK_RT2860");

	/* Comcerto chipset */
	token_set_y("CONFIG_COMCERTO_COMMON_821XX"); /* do not want to re-use CONFIG_COMCERTO_COMMON */

	/* HW Configuration Section */
	enable_module("CONFIG_HW_USB_HOST_EHCI");
	enable_module("CONFIG_HW_USB_STORAGE");
	token_set_y("CONFIG_HW_ETH_LAN");
	token_set_y("CONFIG_HW_ETH_WAN");

        enable_module("CONFIG_HW_DSP");
 
	/* Fast Path */
	if (token_get("MODULE_RG_PSE"))
	    token_set_y("CONFIG_RG_PSE_HW");

 	/* Cryptographic hardware accelerator */
 	if (token_get("MODULE_RG_IPSEC"))
	    enable_module("CONFIG_HW_ENCRYPTION");
 
	if (IS_DIST("PACKET-IAD") || IS_DIST("BB-ROUTER"))
	    token_set_y("CONFIG_COMCERTO_VITESSE");

	if ( IS_DIST("FE-ROUTER") )
	{
	    if (token_get("CONFIG_HW_DSP"))
	    {
		/* FXS support */
		token_set_y("CONFIG_SPI_SI3215");
		/* FXO support */
		token_set_y("CONFIG_SPI_SI3050");
	    }
	    token_set_m("CONFIG_HW_BUTTONS");
	    token_set_m("CONFIG_HW_LEDS");
	}
	

	token_set_y("CONFIG_RG_FOOTPRINT_REDUCTION");
	token_set_y("CONFIG_RG_NETTOOLS_ARP");

	dev_add_bridge("br0", DEV_IF_NET_INT, "eth2", NULL);
    }
    else if (IS_DIST("WADB100G_26"))
    {
    	hw = "WADB100G";
	os = "LINUX_26";
	
	token_set("CONFIG_RG_JPKG_DIST", "JPKG_MIPSEB");

	/* Small flash */
	token_set("CONFIG_RG_STANDARD_SIZE_CONF_SECTION", "n");
	token_set("CONFIG_RG_STANDARD_SIZE_IMAGE_SECTION", "n");

	/*  RG Priority 1  */
    	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_VIDEO_SURVEILLANCE");
    	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
    	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
    	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
    	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_DSL");
	enable_module("MODULE_RG_SSL_VPN");
	enable_module("MODULE_RG_PSE");
	enable_module("MODULE_RG_REDUCE_SUPPORT");
	enable_module("MODULE_RG_OSAP_AGENT");
	
	/*  RG Priority 2  */
    	enable_module("MODULE_RG_PPTP");
    	enable_module("MODULE_RG_L2TP");
	
	/*  RG Priority 3  */
    	enable_module("MODULE_RG_VLAN");

	/*  RG Priority 4  */
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_TR_064");
	/*Not enough space on flash 
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_JVM");
	*/

	/*  RG Priority 7  */
	/*Not enough space on flash 
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_IPV6");
	enable_module("MODULE_RG_ROUTE_MULTIWAN");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	*/
	

	token_set_y("CONFIG_ULIBC_SHARED");

	token_set_y("CONFIG_RG_MTD");

	/* OpenRG HW support */
	
	token_set_y("CONFIG_HW_DSL_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
	token_set_y("CONFIG_HW_SWITCH_LAN");
	enable_module("CONFIG_HW_80211G_BCM43XX");
	enable_module("CONFIG_HW_USB_RNDIS");

	/* B40399: HW led/button support is required for ASUS kernel 2.6
	token_set_m("CONFIG_HW_BUTTONS");
        token_set_y("CONFIG_HW_LEDS");
	*/

	dev_add_bridge("br0", DEV_IF_NET_INT, "bcm0", "bcm1", NULL);
	dev_add_to_bridge_if_opt("br0", "usb0", "CONFIG_HW_USB_RNDIS");

	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MODULES_INITRAMFS");
    }
    else if (IS_DIST("ASUS6020VI_26"))
    {
    	hw = "ASUS6020VI";
	os = "LINUX_26";
	
	token_set("CONFIG_RG_JPKG_DIST", "JPKG_MIPSEB");

	token_set_y("CONFIG_BOOTLDR_CFE");
	token_set_y("CONFIG_BOOTLDR_CFE_DUAL_IMAGE");

	/* Small flash */
	token_set("CONFIG_RG_STANDARD_SIZE_CONF_SECTION", "n");
	token_set("CONFIG_RG_STANDARD_SIZE_IMAGE_SECTION", "n");

	/*  RG Priority 1  */
    	enable_module("MODULE_RG_FOUNDATION");
    	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_VIDEO_SURVEILLANCE");
    	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
    	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
    	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
    	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_DSL");
	enable_module("MODULE_RG_SSL_VPN");
	enable_module("MODULE_RG_REDUCE_SUPPORT");
	enable_module("MODULE_RG_OSAP_AGENT");
	enable_module("MODULE_RG_PSE");
	
	/*  RG Priority 2  */
	/* Not enough space on flash 
 	enable_module("MODULE_RG_PPTP");
  	enable_module("MODULE_RG_L2TP");
	*/
	
	/*  RG Priority 3  */
	/* Not enough space on flash 
  	enable_module("MODULE_RG_VLAN");
	*/

	/*  RG Priority 4  */
	/* Not enough RAM on board for IPSEC
	enable_module("MODULE_RG_IPSEC");
	*/
	/* Not enough space on flash 
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_TR_064");
	enable_module("MODULE_RG_JVM");
	*/

	/*  RG Priority 7  */
	/* Not enough space on flash
	enable_module("MODULE_RG_ROUTE_MULTIWAN");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_IPV6");
	*/
	
	/* OpenRG HW support */
	
	token_set_y("CONFIG_HW_DSL_WAN");
	/*
	token_set_y("CONFIG_HW_SWITCH_LAN");
	*/
	token_set_y("CONFIG_HW_ETH_LAN");
	enable_module("CONFIG_HW_80211G_BCM43XX");

	/* B40399: HW led/button support is required for ASUS kernel 2.6
	token_set_m("CONFIG_HW_BUTTONS");
	token_set_y("CONFIG_HW_LEDS");
	*/

	token_set_y("CONFIG_ULIBC_SHARED");

	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MAINFS_IN_PHYS_MAPPED_FLASH");

	dev_add_bridge("br0", DEV_IF_NET_INT, "bcm1", NULL);
    }
    else if (IS_DIST("JNET_SERVER"))
    {
	set_jnet_server_configs();
	token_set_y("CONFIG_DEBIAN_DPKG_VER_CHECK");
	token_set_y("CONFIG_RG_USE_LOCAL_TOOLCHAIN");
	token_set_y("CONFIG_RG_BUILD_LOCAL_TARGETS_ONLY");
	token_set_y("CONFIG_RG_ZLIB");
	token_set_y("CONFIG_LOCAL_EVENT_EPOLL");
	token_set("CONFIG_RG_LANGUAGES", "DEF fr ru es ko zh_TW ja de it he "
	    "zh_CN");
    }
    else if (IS_DIST("VAS_PORTAL"))
    {
	set_vas_configs();
	token_set_y("CONFIG_RG_USE_LOCAL_TOOLCHAIN");
	token_set_y("CONFIG_RG_BUILD_LOCAL_TARGETS_ONLY");
	token_set("CONFIG_RG_LANGUAGES", "DEF fr ru es ko zh_TW ja de it he "
	    "zh_CN");
    }
    else if (IS_DIST("HOSTTOOLS"))
    {
	set_hosttools_configs();
	/* we want to use objects from JPKG_UML (local_*.o.i686-jungo-linux-gnu)
	 * despite the fact that we use the host local compiler */
	token_set_y("CONFIG_RG_USE_LOCAL_TOOLCHAIN");
	token_set("I386_TARGET_MACHINE", "i686-jungo-linux-gnu");
	token_set("TARGET_MACHINE", token_get_str("I386_TARGET_MACHINE"));

	token_set_y("CONFIG_GLIBC");
	token_set_y("CONFIG_RG_BUILD_LOCAL_TARGETS_ONLY");
	token_set("CONFIG_RG_JPKG_DIST", "JPKG_UML");
    }
    else if (IS_DIST("MAKELICRG"))
    {
	token_set_y("CONFIG_RG_USE_LOCAL_TOOLCHAIN");
	token_set_y("CONFIG_GLIBC");
	token_set_y("CONFIG_RG_BUILD_LOCAL_TARGETS_ONLY");
    }
    else if (IS_DIST("BCM963168AC5") || IS_DIST("VOX_2.5_IT") ||
        IS_DIST("VOX_2.5_UK"))
    {
	os = "LINUX_26";

	if (IS_DIST("BCM963168AC5"))
	{
	    hw = "BCM963168AC5";

	    enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	    token_set_y("CONFIG_RG_FLASH_LAYOUT_BCM963168AC5_REF");
	}
	else if (IS_DIST("VOX_2.5_IT"))
	{
	    hw = "BCM963168_VOX_2.5_IT";
	    token_set("CONFIG_RG_JPKG_DIST",
		"JPKG_MIPSEB_BCM9636X_BSP_4_12_IT");
	    token_set_y("CONFIG_RG_FLASH_LAYOUT_VF_IT_2_5");
	}
	else if (IS_DIST("VOX_2.5_UK"))
	{
	    hw = "BCM963168_VOX_2.5_UK";
	    hw_compatible = "BCM963168_VOX_2.5_IT";
	    token_set("CONFIG_RG_JPKG_DIST",
		"JPKG_MIPSEB_BCM9636X_BSP_4_12_UK");
	    token_set_y("CONFIG_RG_FLASH_LAYOUT_VF_UK_2_5");
	}

	if (IS_DIST("VOX_2.5_IT") || IS_DIST("VOX_2.5_UK"))
	{
	    if (IS_DIST("VOX_2.5_IT"))
	    {
	        token_set_y("CONFIG_RG_VODAFONE_IT");
	        token_set_y("CONFIG_RG_VODAFONE_2_5_IT");
                token_set_y("CONFIG_RG_VODAFONE_CCF_ALLOW_NO_PASSWORD");
		token_set_y("CONFIG_RG_DISK_MNG_MOUNT_PER_USER_FTP");
	    }
	    
	    token_set_y("CONFIG_RG_VODAFONE_2_5");

	    token_set_y("CONFIG_RG_PERM_STORAGE_UBI");
	    token_set_y("CONFIG_UBIFS_FS");

	    enable_module("MODULE_RG_TR_064");
            enable_module("MODULE_RG_IPSEC");
	    enable_module("MODULE_RG_REDUCE_SUPPORT");
	    enable_module("MODULE_RG_ROUTE_MULTIWAN");
            enable_module("MODULE_RG_MODULAR_UPGRADE");
            enable_module("MODULE_RG_FTP_SERVER");
            token_set_y("CONFIG_RG_URL_KEYWORD_FILTER");

	    /* VSAF */
	    token_set_y("CONFIG_RG_VSAF");
	    token_set("CONFIG_RG_VSAF_RUNNING_DIR", "/mnt/ffs/ubifs/vsaf");
	    token_set_y("CONFIG_RG_VODAFONE_PUSH_NOTIFICATIONS");
	    token_set_y("CONFIG_RG_QOS_INGRESS");
	    token_set_m("CONFIG_JFFS2_FS");

	    token_set_y("CONFIG_RG_MTD_UTILS");
	    token_set_y("CONFIG_RG_FFS");
	    token_set("CONFIG_RG_FFS_PARTITION_NAME", "FFS");
	    token_set_y("CONFIG_RG_TFTP_UPGRADE");
	    token_set_y("CONFIG_RG_VODAFONE_MANUFACTURE");
	    
	    /* WBM */
	    token_set_y("CONFIG_WBM_VODAFONE");
	    token_set_m("CONFIG_RG_KLOG");
            token_set_y("CONFIG_INOTIFY");
            token_set_y("CONFIG_INOTIFY_USER");

   	    /* Printer Support */
	    enable_module("MODULE_RG_PRINTSERVER");
	    
            token_set_y("CONFIG_RG_HW_WATCHDOG");
	    token_set_y("CONFIG_RG_MULTIWAN");
	    token_set_y("CONFIG_BOOTLDR_SIGNED_IMAGES");

	    token_set_y("CONFIG_RG_EXTRA_MIME_TYPES");
 	    token_set_y("CONFIG_RG_PERM_STORAGE_INIT_OPTIMIZATION");
            token_set_y("CONFIG_RG_VODAFONE_DSL_MONITOR_HACK");

	    token_set_y("CONFIG_HW_LEDS");

	    if (!token_get("CONFIG_RG_BSP_UPGRADE") ||
		token_get("CONFIG_RG_JPKG"))
	    {
		token_set_y("CONFIG_RG_MAINFS_IN_UBI");
	    }

	    token_set_y("CONFIG_RG_PROXIMITY_SENSOR");

	    /* Calibration Data in /mnt/ffs/caldata */
	    token_set_y("CONFIG_VF_FFS_CALDATA");

	    token_set_y("CONFIG_RG_VODAFONE_DM_EXTERNAL_CLIENT_CERT");
	}

	token_set("LIBC_IN_TOOLCHAIN", "n");
	token_set("CONFIG_RG_KERNEL_COMP_METHOD", "gzip");

	token_set_y("CONFIG_RG_INITFS_RAMFS");

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_DSL");	
	enable_module("MODULE_RG_PSE");
	enable_module("MODULE_RG_DSLHOME");
        enable_module("MODULE_RG_FILESERVER");
        enable_module("MODULE_RG_VLAN");
        enable_module("MODULE_RG_UPNP");
	if (IS_DIST("VOX_2.5_IT") || IS_DIST("VOX_2.5_UK"))
	{
	    enable_module("MODULE_RG_IPV6");
	    token_set_y("CONFIG_RG_DSLHOME_DATAMODEL_TR_181");
	    token_set_y("CONFIG_RG_DSLHOME_DATAMODEL_TR_181_ENCAPSULATED");
	}
	/* Hack: Both devices are 5G. The code expects that one device is 2.4G
	 * and assumes the other device will turn it on. */
	token_set_y("CONFIG_RG_DEV_IF_BCM432X_WLAN");
	enable_module("CONFIG_HW_80211N_BCM432X");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	enable_module("CONFIG_RG_WPS");
	token_set_y("CONFIG_RG_VODAFONE_WL_RESCAN");

	enable_module("CONFIG_HW_USB_STORAGE");
	enable_module("CONFIG_HW_USB_HOST_EHCI");
	enable_module("CONFIG_HW_USB_HOST_OHCI");

	token_set_y("CONFIG_HW_ETH_LAN");
	token_set_y("CONFIG_HW_SWITCH_LAN");
	token_set_y("CONFIG_HW_DSL_WAN");
	token_set_y("CONFIG_HW_VDSL_WAN");

	token_set_m("CONFIG_HW_BUTTONS");
	token_set_y("CONFIG_SMP");

	if (IS_DIST("VOX_2.5_IT") || IS_DIST("VOX_2.5_UK"))
	    token_set_y("CONFIG_RG_PSE_HW");

	if (IS_DIST("VOX_2.5_IT") || IS_DIST("VOX_2.5_UK"))
	{
	    token_set("CONFIG_VF_WLAN_DEV", "wl0");
	    token_set("CONFIG_VF_WLAN2_DEV", "wl1");
	    token_set_y("CONFIG_RG_DUAL_CONCURRENT_WLAN");
	    token_set_y("CONFIG_RG_3G_WAN");
	}

	if (IS_DIST("VOX_2.5_UK"))
	{
	    token_set_y("CONFIG_RG_VODAFONE_3G_ACTIVATION");
	    token_set_y("CONFIG_RG_VODAFONE_UK");
	    token_set("CONFIG_RG_ETH_WAN_DETECT", "n");
	    token_set_y("CONFIG_RG_ETHERNET_OAM");
	    token_set_y("CONFIG_RG_ROUTED_LAN_VLAN_SEPARATION");
	    token_set_y("CONFIG_RG_VODAFONE_CCF_ENCRYPT");

	    set_vodafone_common_configs("bcmsw", NULL, "E448C7");
	    set_vodafone_uk_devices();
	    set_vodafone_uk_configs("bcmsw", NULL, "bcm_ptm0", NULL);
	    /* These configs are enabled on 5.4.8.1.160.13 but are temporary
	     * disabled on 5.4.8.1 till merging needed changes */
#if 0
	    token_set_y("CONFIG_RG_ETHERNET_OAM");
#endif
	    /* rg_conf obscured values are encrypted by AES key provided by
	     * manufacturer */
	    token_set_y("CONFIG_RG_RGCONF_CRYPTO_AES");
	    token_set_y("CONFIG_RG_RGCONF_CRYPTO_AES_VENDOR_KEY");

            /* Twonky Media Server */
            token_set_y("CONFIG_RG_TWONKY");
	    token_set("CONFIG_RG_TWONKY_FRIENDLYNAME", "Vodafone Connect");
	    token_set_y("CONFIG_RG_ROUTED_LAN");
	}
	if (IS_DIST("VOX_2.5_IT"))
	{
	    /* Emaze parental control */
	    token_set_y("CONFIG_RG_EMAZE_HTTP_PROXY");
	    token_set_y("CONFIG_EPOLL");

	    /* Fon Support */
	    token_set_y("CONFIG_RG_FON");
	    token_set("CONFIG_RG_FON_BR_DEV", "br3");
	    token_set_y("CONFIG_RG_RADIUS_PROXY");

            token_set_y("CONFIG_RG_3G_VOICE");
	    token_set_y("CONFIG_RG_3G_SMS");
            token_set_y("CONFIG_RG_3G_SMS_WHITELIST");
	    token_set_y("CONFIG_RG_3G_SMS_MSISDN");
            token_set_y("CONFIG_RG_VODAFONE_3G_VOICE_OVER_PS");
	    token_set_y("CONFIG_VF_FAILOVER_INFORM");
            token_set_y("CONFIG_RG_3G_PER_BEARER_APN");
	    token_set_y("CONFIG_HW_ETH_WAN");
	    set_vodafone_common_configs("bcmsw", NULL, "90356E");
	    set_vodafone_it_devices();
	    set_vodafone_it_configs("bcmsw", "eth0", "bcm_ptm0", NULL);
            token_set_y("CONFIG_RG_VOIP_TEMPLATE_EXTENSIONS");
	    token_set_y("CONFIG_RG_PARAM_SPLIT");

            /* Twonky Media Server */
            token_set_y("CONFIG_RG_TWONKY");
	    token_set("CONFIG_RG_TWONKY_FRIENDLYNAME", "Vodafone Station");

	    token_set("CONFIG_RG_SAMBA_SERVER_SIGNING", "mandatory");

	    /* Common LED features */
	    token_set_y("CONFIG_RG_LEDS_BOTTOM_INTENSITY");
	    token_set_y("CONFIG_RG_LEDS_PROXIMITY_OVERRIDE");

	    if (token_get("CONFIG_RG_3G_WAN"))
		token_set_y("CONFIG_RG_MOB_STATS");

	    token_set("CONFIG_PRIORITY_NTFS_3G", "17");
	    token_set("CONFIG_PRIORITY_SAMBA", "17");
	    token_set("CONFIG_PRIORITY_TWONKY", "18");
	    token_set("CONFIG_PRIORITY_VSAF", "19");

	    token_set("CONFIG_RG_DATA_MODEL_MANUFACTURER", "Vodafone");
	}

#if 0
      	dev_add_bridge("br0", DEV_IF_NET_INT, "bcmsw", "wl0", "wl1", NULL);
#endif
	/* VoIP */
	if (IS_DIST("BCM963168AC5") || IS_DIST("VOX_2.5_IT"))
	{
	    enable_module("CONFIG_HW_DSP");
	    enable_module("MODULE_RG_HOME_PBX");
	    enable_module("MODULE_RG_VOIP_ASTERISK_SIP");
	}
	if (IS_DIST("VOX_2.5_IT"))
        {
	    token_set_y("CONFIG_RG_VOIP_CALL_LOG");
            token_set_y("CONFIG_RG_VODAFONE_WORKAROUND_DELAY_SIP_REG");

            /* ipphone backup support */
            token_set_y("CONFIG_RG_VODAFONE_IPPHONE_BACKUP");
  
            token_set_y("BRCM_VRG_COUNTRY_CFG_ITALY");
            token_set_y("CONFIG_RG_VODAFONE_DATA_LAN_VOIP_EXTENSIONS");
	    token_set_y("CONFIG_RG_VODAFONE_APPEND_SIP_PREFIX");
	}

	if (!token_get("CONFIG_RG_IPV6"))
	    token_set("CONFIG_RG_IPV6_SET_DEFAULT_ULA", "n");
    }
    else if (IS_DIST("BCM963168AC5_3X"))
    {
	os = "LINUX_3X";
	hw = "BCM963168AC5";

	token_set("LIBC_IN_TOOLCHAIN", "n");

	token_set_y("CONFIG_RG_INITFS_RAMFS");

	token_set_y("CONFIG_RG_FLASH_LAYOUT_BCM963168AC5_REF");

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_VLAN");	
//	enable_module("MODULE_RG_DSL");	
	enable_module("MODULE_RG_PSE");
	enable_module("MODULE_RG_FILESERVER");
//	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
//	enable_module("CONFIG_RG_WPS");
//	token_set_y("CONFIG_RG_VAP_HOME");
//	token_set_y("CONFIG_RG_VAP_SECURED");

	/* Hack: Both devices are 5G. The code expects that one device is 2.4G
	 * and assumes the other device will turn it on. */
//	token_set_y("CONFIG_RG_DEV_IF_BCM432X_WLAN");

	enable_module("CONFIG_HW_USB_STORAGE");
	enable_module("CONFIG_HW_USB_HOST_EHCI");
	enable_module("CONFIG_HW_USB_HOST_OHCI");

	token_set_y("CONFIG_HW_ETH_LAN");
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_SWITCH_LAN");
//	token_set_y("CONFIG_HW_DSL_WAN");
//	token_set_y("CONFIG_HW_VDSL_WAN");

	token_set_y("CONFIG_HW_BUTTONS");

//	enable_module("CONFIG_HW_80211N_BCM432X");

//     	dev_add_bridge("br0", DEV_IF_NET_INT, "bcmsw", "wl0", "wl1", NULL);
      	dev_add_bridge("br0", DEV_IF_NET_INT, "bcmsw", NULL);

	/* VoIP */
#if 0
	enable_module("MODULE_RG_HOME_PBX");
	enable_module("MODULE_RG_VOIP_ASTERISK_SIP");
	enable_module("CONFIG_HW_DSP");
#endif
    }
    else if (IS_DIST("RGLOADER_BCM963168AC5"))
    {
	hw = "BCM963168AC5";
	os = "LINUX_26";

	token_set_y("CONFIG_RG_FLASH_LAYOUT_BCM963168AC5_REF");

	token_set_y("CONFIG_RG_RGLOADER");
	token_set_m("CONFIG_RG_KRGLDR");

	token_set_y("CONFIG_PROC_FS");
	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MAINFS_BUILTIN");

	token_set("CONFIG_SDRAM_SIZE", "32");
	token_set("LIBC_IN_TOOLCHAIN", "n");
	token_set_y("CONFIG_HW_ETH_LAN");
	dev_add("bcmsw", DEV_IF_BCM9636X_ETH, DEV_IF_NET_INT);

	token_set("CONFIG_RG_FACTORY_ON_FLASH_SECTION", "n");
	token_set_y("CONFIG_RG_NETWORK_BOOT");
    }
    else if (IS_DIST("BCM963168AC5_LSP") || IS_DIST("BCM963168_VOX_2.5_IT_LSP")
	|| IS_DIST("BCM963168_VOX_2.5_DE_LSP") ||
	IS_DIST("BCM963168_VOX_2.5_DE_LSP_3X"))
    {
	if (IS_DIST("BCM963168_VOX_2.5_DE_LSP_3X"))
	    os = "LINUX_3X";
	else
	    os = "LINUX_26";

	if (IS_DIST("BCM963168AC5_LSP"))
	{
	    hw = "BCM963168AC5";
	    token_set_y("CONFIG_RG_FLASH_LAYOUT_BCM963168AC5_REF");
	}
	else if (IS_DIST("BCM963168_VOX_2.5_IT_LSP"))
	{
	    hw = "BCM963168_VOX_2.5_IT";
	    token_set_y("CONFIG_RG_FLASH_LAYOUT_VF_IT_2_5");
	}
	else if (IS_DIST("BCM963168_VOX_2.5_DE_LSP") ||
	    IS_DIST("BCM963168_VOX_2.5_DE_LSP_3X"))
	{
	    hw = "BCM963168_VOX_2.5_DE";
	    token_set_y("CONFIG_RG_FLASH_LAYOUT_VF_DE_2_5");
	}

	token_set_y("CONFIG_LSP_DIST");
	token_set("LIBC_IN_TOOLCHAIN", "n");

	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MAINFS_BUILTIN");

	token_set_y("CONFIG_HW_ETH_LAN");

	if (IS_DIST("BCM963168_VOX_2.5_IT_LSP") ||
	    IS_DIST("BCM963168_VOX_2.5_DE_LSP") ||
	    IS_DIST("BCM963168_VOX_2.5_DE_LSP_3X"))
	{
	    token_set_y("CONFIG_RG_MTD_UTILS");
	    token_set_y("CONFIG_RG_PERM_STORAGE_UBI");
	    token_set_y("CONFIG_UBIFS_FS");
	    token_set_m("CONFIG_JFFS2_FS");
	}
    }
    else if (IS_DIST("BCM963168AC5_LSP_3X"))
    {
	os = "LINUX_3X";
	hw = "BCM963168AC5";

	token_set_y("CONFIG_RG_FLASH_LAYOUT_BCM963168AC5_REF");

	token_set_y("CONFIG_LSP_DIST");
	token_set("LIBC_IN_TOOLCHAIN", "n");

	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MAINFS_BUILTIN");
	token_set_y("CONFIG_HW_ETH_LAN");
    }
    else if (IS_DIST("BCM96362_LSP") || IS_DIST("WVDB113G_LSP") ||
	IS_DIST("YWZ00A_LSP") || IS_DIST("YWZ00B_LSP") ||
	IS_DIST("HG558BZA_LSP") || IS_DIST("AGC00A_LSP"))
    {
	os = "LINUX_26";
	if (IS_DIST("BCM96362_LSP"))
	{
	    hw = "BCM96362";
	    token_set_y("CONFIG_RG_FLASH_LAYOUT_BCM96362_REF");
	}
	else if (IS_DIST("WVDB113G_LSP"))
	{
	    hw = "WVDB113G";
	    token_set_y("CONFIG_RG_FLASH_LAYOUT_WVDB113G_REF");
	}
	else if (IS_DIST("YWZ00A_LSP"))
	{
	    hw = "YWZ00A";
	    token_set_y("CONFIG_RG_FLASH_LAYOUT_YWZ00A_REF");
	}
	else if (IS_DIST("YWZ00B_LSP"))
	{
	    hw = "YWZ00B";
	    token_set_y("CONFIG_RG_FLASH_LAYOUT_YWZ00B_REF");
	}
	else if (IS_DIST("HG558BZA_LSP"))
	{
	    hw = "HG558BZA";
	    token_set_y("CONFIG_RG_FLASH_LAYOUT_HG558BZA_REF");
	}
	else if (IS_DIST("AGC00A_LSP"))
	{
	    hw = "AGC00A";
	    token_set_y("CONFIG_RG_FLASH_LAYOUT_VF_ES_1_5");
	}

	token_set_y("CONFIG_LSP_DIST");
	token_set("LIBC_IN_TOOLCHAIN", "n");

        enable_module("CONFIG_HW_USB_STORAGE");
	enable_module("CONFIG_HW_USB_HOST_EHCI");
	enable_module("CONFIG_HW_USB_HOST_OHCI");
	enable_module("MODULE_RG_ROUTE_MULTIWAN");

	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MAINFS_BUILTIN");
	token_set_y("CONFIG_HW_ETH_LAN");
	token_set_y("CONFIG_HW_DSL_WAN");

	/* use LSP for UBI writing */
	token_set("CONFIG_RG_UBI_PARTITION", "UBI");
	token_set_y("CONFIG_RG_MTD_UTILS");
    }
    else if (IS_DIST("YWZ00A") || IS_DIST("YWZ00B") || IS_DIST("WVDB113G") ||
	IS_DIST("BCM96362"))
    {
	os = "LINUX_26";
	if (IS_DIST("YWZ00A"))
	{
	    hw = "YWZ00A";
	    token_set_y("CONFIG_RG_FLASH_LAYOUT_YWZ00A_REF");
	}
	else if (IS_DIST("YWZ00B"))
	{
	    hw = "YWZ00B";
	    token_set_y("CONFIG_RG_FLASH_LAYOUT_YWZ00B_REF");
	}
	else if (IS_DIST("WVDB113G"))
	{
	    hw = "WVDB113G";
	    token_set_y("CONFIG_RG_FLASH_LAYOUT_WVDB113G_REF");
	}
	else if (IS_DIST("BCM96362"))
	{
	    hw = "BCM96362";
	    token_set_y("CONFIG_RG_FLASH_LAYOUT_BCM96362_REF");
	}

	token_set("LIBC_IN_TOOLCHAIN", "n");

	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MODULES_INITRAMFS");

    	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
    	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_PPP");
        enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_VLAN");	
	enable_module("MODULE_RG_DSL");	

        enable_module("CONFIG_HW_USB_STORAGE");
	enable_module("CONFIG_HW_USB_HOST_EHCI");
	enable_module("CONFIG_HW_USB_HOST_OHCI");
	enable_module("MODULE_RG_ROUTE_MULTIWAN");

	token_set_y("CONFIG_HW_SWITCH_LAN");
	token_set_y("CONFIG_HW_ETH_LAN");
	token_set_y("CONFIG_HW_DSL_WAN");

	if (IS_DIST("YWZ00B") || IS_DIST("BCM96362"))
	{
	    token_set_y("CONFIG_RG_PERM_STORAGE_UBI");
            token_set("CONFIG_RG_UBI_PARTITION_SIZE", "0x02800000");
	}

	/* VOIP */
	enable_module("MODULE_RG_HOME_PBX");
	token_set_y("CONFIG_RG_VOIP_CALL_LOG");
	enable_module("MODULE_RG_VOIP_ASTERISK_SIP");
	enable_module("CONFIG_HW_DSP");

	/* VOIP Localization, default Italy */
	token_set_y("BRCM_VRG_COUNTRY_CFG_ITALY");
    }
    else if (IS_DIST("HG558BZA"))
    {
	os = "LINUX_26";
	hw = "HG558BZA";

	token_set("LIBC_IN_TOOLCHAIN", "n");

	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MODULES_INITRAMFS");	

	token_set_y("CONFIG_RG_FLASH_LAYOUT_HG558BZA_REF");

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
    	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_PPP");
        enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_VLAN");	
	enable_module("MODULE_RG_DSL");	
	enable_module("MODULE_RG_DSLHOME");

	/* USB */
	enable_module("MODULE_RG_FILESERVER");
	enable_module("CONFIG_HW_USB_STORAGE");
	enable_module("CONFIG_HW_USB_HOST_EHCI");
	enable_module("CONFIG_HW_USB_HOST_OHCI");
	enable_module("MODULE_RG_ROUTE_MULTIWAN");

	token_set_y("CONFIG_HW_SWITCH_LAN");
	token_set_y("CONFIG_HW_ETH_LAN");
	token_set_y("CONFIG_HW_DSL_WAN");

	token_set_y("CONFIG_RG_MTD_UTILS");
	token_set_y("CONFIG_RG_PERM_STORAGE_UBI");
	token_set("CONFIG_RG_UBI_PARTITION_SIZE", "0x02800000");

	/* WLAN */
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	enable_module("CONFIG_HW_BCM9636X_WLAN");
	enable_module("CONFIG_RG_WPS");

	token_set_y("CONFIG_RG_VODAFONE_CUSTOM_FW_RULES");
	set_vodafone_common_configs("bcmsw", "wl0", "002489");
	set_vodafone_it_configs("bcmsw", "bcmsw", NULL, "wl0");

	/* VOIP */
	enable_module("MODULE_RG_HOME_PBX");
	token_set_y("CONFIG_RG_VOIP_CALL_LOG");
	enable_module("MODULE_RG_VOIP_ASTERISK_SIP");
	enable_module("CONFIG_HW_DSP");

	/* VOIP Localization, default Italy */
	token_set_y("BRCM_VRG_COUNTRY_CFG_ITALY");

	token_set_y("CONFIG_HW_LEDS");
	token_set_y("CONFIG_HW_BUTTONS");
    }
    else if (IS_DIST("YWZ00B_VODAFONE") || IS_DIST("BCM96362_VODAFONE") || 
	IS_DIST("VOX_1.5_IT") || IS_DIST("HG558_VODAFONE") ||
	IS_DIST("VOX_1.5_ES") || IS_DIST("VOX_1.5_PT") ||
	IS_DIST("VOX_1.5_NZ") || IS_DIST("VOX_2.5_IT_1.5HW"))
    {
	int has_3g_voice = 1, has_voice = 1;

	if (IS_DIST("YWZ00B_VODAFONE"))
	{
	    hw = "YWZ00B";
	    has_3g_voice = 0;
	    has_voice = 0;
	    token_set_y("CONFIG_RG_FLASH_LAYOUT_YWZ00B_REF");
	}
	else if (IS_DIST("VOX_1.5_IT") || IS_DIST("VOX_2.5_IT_1.5HW"))
	{
	    hw = "BCM96362_VOX_1.5_IT";
	    hw_compatible = "YWZ00B HG558BZA";
	    token_set_y("CONFIG_RG_VODAFONE_IT");
	    token_set_y("CONFIG_RG_FLASH_LAYOUT_VF_IT_1_5");
        }
	else if (IS_DIST("VOX_1.5_NZ"))
	{
	    hw = "BCM96362_VOX_1.5_NZ";
	    hw_compatible = "YWZ00B";
	    has_3g_voice = 0;
	    token_set_y("CONFIG_RG_VODAFONE_NZ");
	    token_set_y("CONFIG_RG_FLASH_LAYOUT_VF_NZ_1_5");
        }
	else if (IS_DIST("VOX_1.5_ES"))
	{
	    hw = "BCM96362_VOX_1.5_ES";
	    token_set_y("CONFIG_RG_VODAFONE_ES");
	    token_set_y("CONFIG_RG_FLASH_LAYOUT_VF_ES_1_5");
	}
	else if (IS_DIST("VOX_1.5_PT"))
	{
	    hw = "BCM96362_VOX_1.5_PT";
	    token_set_y("CONFIG_RG_VODAFONE_PT");
	    token_set_y("CONFIG_RG_FLASH_LAYOUT_VF_PT_1_5");
	}
	else if (IS_DIST("BCM96362_VODAFONE"))
	{
	    hw = "BCM96362";
	    token_set_y("CONFIG_RG_FLASH_LAYOUT_BCM96362_REF");
	}
	else if (IS_DIST("HG558_VODAFONE"))
	{
	    hw = "HG558BZA";
	    token_set_y("CONFIG_RG_VODAFONE_IT");
	    token_set_y("CONFIG_RG_FLASH_LAYOUT_HG558BZA_REF");
	}
	os = "LINUX_26";

	token_set("CONFIG_RG_JPKG_DIST", "JPKG_MIPSEB_BCM9636X");

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_TR_064");
	enable_module("MODULE_RG_FILESERVER");
	enable_module("MODULE_RG_DSL");
	enable_module("MODULE_RG_PSE");
	enable_module("MODULE_RG_REDUCE_SUPPORT");

        enable_module("CONFIG_HW_USB_STORAGE");
	enable_module("CONFIG_HW_USB_HOST_EHCI");
	enable_module("CONFIG_HW_USB_HOST_OHCI");
	enable_module("MODULE_RG_ROUTE_MULTIWAN");
	enable_module("MODULE_RG_MODULAR_UPGRADE");
        enable_module("MODULE_RG_FTP_SERVER");

	if (IS_DIST("VOX_1.5_IT") || IS_DIST("VOX_2.5_IT_1.5HW"))
	{
	    enable_module("MODULE_RG_IPV6");
	    token_set_y("CONFIG_RG_DSLHOME_DATAMODEL_TR_181");
	    token_set_y("CONFIG_RG_DSLHOME_DATAMODEL_TR_181_ENCAPSULATED");
	    
	    if (IS_DIST("VOX_1.5_IT"))
		token_set_y("CONFIG_RG_DASHBOARD_INTERCEPT");
	}

	token_set_y("CONFIG_RG_URL_KEYWORD_FILTER");

	/* VSAF */
	token_set_y("CONFIG_RG_VSAF");

	token_set_y("CONFIG_HW_LEDS");
	token_set_y("CONFIG_HW_BUTTONS");

	if (IS_DIST("YWZ00B_VODAFONE") || IS_DIST("VOX_1.5_IT") ||
	    IS_DIST("HG558_VODAFONE") || IS_DIST("VOX_1.5_NZ") ||
	    IS_DIST("VOX_2.5_IT_1.5HW"))
	{
	    token_set_y("CONFIG_HW_LCD");
	    token_set_y("CONFIG_HW_TOUCH_BUTTONS");
	}

	token_set_y("CONFIG_HW_SWITCH_LAN");
	token_set_y("CONFIG_HW_ETH_LAN");
	token_set_y("CONFIG_HW_DSL_WAN");

	/* build */
	token_set("LIBC_IN_TOOLCHAIN", "n");
	token_set("CONFIG_RG_KERNEL_COMP_METHOD", "gzip");

 	token_set_y("CONFIG_RG_INITFS_RAMFS");
	if (!token_get("CONFIG_RG_BSP_UPGRADE"))
	    token_set_y("CONFIG_RG_MAINFS_IN_UBI");

	/* WLAN */
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	enable_module("CONFIG_HW_BCM9636X_WLAN");
	enable_module("CONFIG_RG_WPS");

	token_set_y("CONFIG_RG_VODAFONE_CUSTOM_FW_RULES");

	set_vodafone_common_configs("bcmsw", "wl0", "002489");
	if (token_get("CONFIG_RG_VODAFONE_IT"))
	    set_vodafone_it_configs("bcmsw", "bcmsw", NULL, "wl0");
	if (token_get("CONFIG_RG_VODAFONE_ES"))
	    set_vodafone_es_configs("bcmsw", "wl0");
	if (token_get("CONFIG_RG_VODAFONE_PT"))
	    set_vodafone_pt_configs("bcmsw", "wl0");
	if (token_get("CONFIG_RG_VODAFONE_NZ"))
	    set_vodafone_nz_configs("bcmsw", "wl0");

	token_set_y("CONFIG_RG_VODAFONE_WL_RESCAN");

	token_set_y("CONFIG_RG_PERM_STORAGE_UBI");
        token_set("CONFIG_RG_UBI_PARTITION_SIZE", "0x02800000");

	/* 3G Support */
	token_set_y("CONFIG_RG_3G_WAN");
	if (token_get("CONFIG_RG_VODAFONE_IT"))
	{
	    token_set_y("CONFIG_RG_3G_SMS");
	    token_set_y("CONFIG_RG_3G_SMS_WHITELIST");
	    if (!IS_DIST("VOX_2.5_IT_1.5HW"))
		token_set_y("CONFIG_RG_3G_SMS_MSISDN");

	    token_set_y("CONFIG_RG_VODAFONE_3G_VOICE_OVER_PS");
	}
	if (has_3g_voice)
	    token_set_y("CONFIG_RG_3G_VOICE");

	/* JFFS2 Support */
	token_set_m("CONFIG_JFFS2_FS");
	token_set("CONFIG_RG_FFS_PARTITION_NAME", "JFFS2");

	/* VOIP */
	if (has_voice)
	{
	    enable_module("MODULE_RG_HOME_PBX");
	    token_set_y("CONFIG_RG_VOIP_CALL_LOG");
	    enable_module("MODULE_RG_VOIP_ASTERISK_SIP");
	    enable_module("CONFIG_HW_DSP");

	    if (token_get("CONFIG_RG_VODAFONE_IT"))
	    {
		token_set_y("CONFIG_RG_VODAFONE_WORKAROUND_DELAY_SIP_REG");

		/* ipphone backup support */
		token_set_y("CONFIG_RG_VODAFONE_IPPHONE_BACKUP");
	    }
	
	    /* VOIP Localization */
	    if (token_get("CONFIG_RG_VODAFONE_ES"))
		token_set_y("BRCM_VRG_COUNTRY_CFG_SPAIN");
	    else if (token_get("CONFIG_RG_VODAFONE_IT"))
		token_set_y("BRCM_VRG_COUNTRY_CFG_ITALY");
    	    else if (token_get("CONFIG_RG_VODAFONE_PT"))
		token_set_y("BRCM_VRG_COUNTRY_CFG_PORTUGAL");
	    else if (token_get("CONFIG_RG_VODAFONE_NZ"))
		token_set_y("BRCM_VRG_COUNTRY_CFG_NEW_ZEALAND");
	    else
		conf_err("ERROR: Missing country code for voice!");

	    /* Voice Template Extensions */
	    if (token_get("CONFIG_RG_VODAFONE_IT") ||
		token_get("CONFIG_RG_VODAFONE_NZ"))
	    {
		token_set_y("CONFIG_RG_VOIP_TEMPLATE_EXTENSIONS");
		token_set_y("CONFIG_RG_VODAFONE_DATA_LAN_VOIP_EXTENSIONS");
	    }
	}

	token_set_y("CONFIG_RG_MTD_UTILS");
	token_set_y("CONFIG_RG_FFS");
	token_set_y("CONFIG_RG_TFTP_UPGRADE");

	/* WBM */
	token_set_y("CONFIG_WBM_VODAFONE");
	if (token_get("CONFIG_RG_VODAFONE_IT"))
	    token_set_y("CONFIG_RG_EXTERNAL_API");

        /* Crash logger */
	if (IS_DIST("YWZ00B_VODAFONE") || IS_DIST("VOX_1.5_IT") ||
	    IS_DIST("VOX_1.5_ES") || IS_DIST("VOX_1.5_PT") ||
	    IS_DIST("VOX_1.5_NZ") || IS_DIST("VOX_2.5_IT_1.5HW"))
	{
	    token_set_m("CONFIG_RG_KLOG");
	}

	/* Twonky Media Server */
        token_set_y("CONFIG_RG_TWONKY");
	token_set("CONFIG_RG_TWONKY_FRIENDLYNAME", "Vodafone ADSL Router");
        token_set_y("CONFIG_INOTIFY");
        token_set_y("CONFIG_INOTIFY_USER");

	/* Printer Support */
	enable_module("MODULE_RG_PRINTSERVER");

        token_set_y("CONFIG_RG_HW_WATCHDOG");
	token_set_y("CONFIG_RG_MULTIWAN");
	token_set_y("CONFIG_BOOTLDR_SIGNED_IMAGES");
	token_set_y("CONFIG_RG_EXTRA_MIME_TYPES");

        if (IS_DIST("VOX_1.5_IT") || IS_DIST("VOX_1.5_ES") ||
	    IS_DIST("VOX_1.5_PT") || IS_DIST("VOX_1.5_NZ") ||
	    IS_DIST("VOX_2.5_IT_1.5HW"))
	{
	    token_set_y("CONFIG_RG_PERM_STORAGE_INIT_OPTIMIZATION");
	}

	/* Debugging tools */
#if 0	/* Enable those features only for development compilations */
	token_set_y("CONFIG_RG_DBG_ULIBC_MALLOC");
	/* Redirect malloc error messages from stdout to /dev/console */
	token_set_y("CONFIG_RG_DBG_MALLOC_2_CON");
	/* Crash task in case of found memory error */
	token_set_y("CONFIG_RG_DBG_ULIBC_MALLOC_CRASH");
	/* This feature fills freed memory with 0xeb value. If you found case of
	 * usage released memory - use is_mem_valid(ptr) for memory validation.
	 * Impairs board performance severely!!! */ 
	token_set("CONFIG_RG_DBG_ULIBC_MEM_POISON", "256");
#endif

	if (IS_DIST("VOX_1.5_ES") || IS_DIST("VOX_1.5_NZ"))
	    token_set_y("CONFIG_RG_ROUTED_LAN");

	if (token_get("CONFIG_RG_VODAFONE_IT"))
	    token_set_y("CONFIG_RG_VODAFONE_DSL_MONITOR_HACK");

	token_set_y("CONFIG_RG_VODAFONE_MANUFACTURE");

	if (IS_DIST("VOX_1.5_IT") || (IS_DIST("VOX_1.5_NZ")
	    && !token_get("CONFIG_VF_WBM_INTERNATIONAL")))
	{
	    token_set_y("CONFIG_RG_WPS_PBC_MODE_ONLY");
	    token_set_y("CONFIG_RG_WPS_PHYSICAL_PBC");
	    token_set_y("CONFIG_RG_WPS_VIRTUAL_PBC");
	}
	else
	{
	    token_set_y("CONFIG_RG_WPS_PHYSICAL_PBC");
	    token_set_y("CONFIG_RG_WPS_VIRTUAL_PBC");
	    token_set_y("CONFIG_RG_WPS_PHYSICAL_DISPLAY_PIN");
	    token_set_y("CONFIG_RG_WPS_VIRTUAL_DISPLAY_PIN");
	    token_set_y("CONFIG_RG_WPS_KEYPAD");
	}
    }
    else if (IS_DIST("RGLOADER_YWZ00B") || IS_DIST("RGLOADER_BCM96362") ||
	IS_DIST("RGLOADER_HG558BZA") || IS_DIST("RGLOADER_AGC00A")  || 
	IS_DIST("RGLOADER_BCM963168_ADB") ||
	IS_DIST("RGLOADER_BCM963168_AUQ00X_DE") ||
	IS_DIST("RGLOADER_BCM963168_AUQ00X_IT") ||
	IS_DIST("RGLOADER_BCM963168_HUAWEI"))
    {        
	os = "LINUX_26";
	if (IS_DIST("RGLOADER_YWZ00B"))
	{
	    hw = "YWZ00B";
	    token_set_y("CONFIG_RG_FLASH_LAYOUT_YWZ00B_REF");
	}
        else if (IS_DIST("RGLOADER_BCM96362"))
	{
            hw = "BCM96362";
	    token_set_y("CONFIG_RG_FLASH_LAYOUT_BCM96362_REF");
	}
	else if (IS_DIST("RGLOADER_HG558BZA"))
	{
            hw = "HG558BZA";
	    token_set_y("CONFIG_RG_FLASH_LAYOUT_HG558BZA_REF");
	}
	else if (IS_DIST("RGLOADER_AGC00A"))
	{
	    hw = "AGC00A";
	    token_set_y("CONFIG_RG_FLASH_LAYOUT_VF_ES_1_5");
	}
	else if (IS_DIST("RGLOADER_BCM963168_ADB"))
	{
	    hw = "BCM963168_ADB";
	    token_set_y("CONFIG_RG_FLASH_LAYOUT_VF_IT_2_5");
	}
	else if (IS_DIST("RGLOADER_BCM963168_AUQ00X_DE"))
	{
	    hw = "BCM963168_AUQ00X";
	    token_set_y("CONFIG_RG_FLASH_LAYOUT_VF_DE_2_5");
	}
	else if (IS_DIST("RGLOADER_BCM963168_AUQ00X_IT"))
	{
	    hw = "BCM963168_AUQ00X";
	    token_set_y("CONFIG_RG_FLASH_LAYOUT_VF_IT_2_5");
	}
	else if (IS_DIST("RGLOADER_BCM963168_HUAWEI"))
	{
	    hw = "BCM963168_HUAWEI";
	    token_set_y("CONFIG_RG_FLASH_LAYOUT_VF_IT_2_5");
	}
        
	token_set_y("CONFIG_RG_RGLOADER");
        token_set_m("CONFIG_RG_KRGLDR");

 	token_set_y("CONFIG_PROC_FS");
	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MAINFS_BUILTIN");

	token_set_y("CONFIG_RG_PERM_STORAGE_UBI");
	if (IS_DIST("RGLOADER_BCM963168_ADB") ||
	    IS_DIST("RGLOADER_BCM963168_AUQ00X_DE") ||
	    IS_DIST("RGLOADER_BCM963168_AUQ00X_IT") ||
	    IS_DIST("RGLOADER_BCM963168_HUAWEI"))
	{
	    token_set_y("CONFIG_UBIFS_FS");
	}
	token_set_y("CONFIG_BRCM_MEMORY_RESTRICTION_96M");

	token_set("CONFIG_RG_CRAMFS_COMP_METHOD", "none");
	if (IS_DIST("RGLOADER_YWZ00B") || IS_DIST("RGLOADER_HG558BZA"))
	{
	    token_set_y("CONFIG_HW_LCD");
	    token_set_y("CONFIG_JFFS2_FS");
	}

	token_set_y("CONFIG_BOOTLDR_SIGNED_IMAGES"); 
	token_set_y("CONFIG_RG_RMT_UPDATE");
	token_set_y("CONFIG_RG_X509");
    	token_set_y("CONFIG_RG_FOOTPRINT_REDUCTION");
	token_set("LIBC_IN_TOOLCHAIN", "n");

	if (IS_DIST("RGLOADER_BCM963168_AUQ00X_DE"))
	{
	    token_set_y("CONFIG_RG_NETWORK_BOOT");
	    token_set_y("CONFIG_HW_ETH_LAN");
	    dev_add("bcmsw", DEV_IF_BCM9636X_ETH, DEV_IF_NET_INT);
	    token_set("RG_LAN_IP_SUBNET", "2");
	    token_set_y("CONFIG_HW_LEDS");
	    token_set_y("CONFIG_RG_VODAFONE");
	    token_set_y("CONFIG_RG_VODAFONE_DE");
	    token_set_y("CONFIG_RG_VODAFONE_2_5");
	}

	if (IS_DIST("RGLOADER_BCM963168_AUQ00X_IT"))
	{
	    /* RGLoader/CFE intended to run BSP 4.12 use hacky voice id */
	    token_set("BRCM_VOICE_BOARD_ID", "LE9662_ZSI");
	}

	if (IS_DIST("RGLOADER_BCM963168_ADB") ||
	    IS_DIST("RGLOADER_BCM963168_AUQ00X_DE") ||
	    IS_DIST("RGLOADER_BCM963168_AUQ00X_IT") ||
	    IS_DIST("RGLOADER_BCM963168_HUAWEI"))
	{
	    if (is_default_license)
		conf_err("You must specify LIC when compiling rgloader.\n");
	}
    }
    else if (IS_DIST("BCM6358_3X_LSP"))
    {
	/* BCM 6358 with kernel "in-tree" BSP */
	hw = "BCM6358";
	os = "LINUX_3X";

	token_set_y("CONFIG_LSP_DIST");

	token_set_y("CONFIG_HW_ETH_LAN");
	token_set("LIBC_IN_TOOLCHAIN", "n");
	token_set_y("CONFIG_DYN_LINK");

	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set("CONFIG_RG_INITFS_RAMFS_COMP_METHOD", "xz");
	token_set("CONFIG_RG_MAINFS", "n");

	/* Use arch/mips/boot/compressed vmlinuz image loader,
	 * with gzip compressed kernel - best ratio of uncompress:footprint */
	token_set_y("CONFIG_SYS_SUPPORTS_ZBOOT");
	token_set_y("CONFIG_KERNEL_GZIP");
    }
    else if (IS_DIST("BCM96358_LSP"))
    {
	hw = "BCM96358";
	os = "LINUX_26";

	token_set_y("CONFIG_LSP_DIST");

	/* Devices */
	/* XXX: Add all devices when drivers are ready.
	 */
	token_set_y("CONFIG_HW_ETH_LAN");
	
	token_set("LIBC_IN_TOOLCHAIN", "n");

	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MODULES_INITRAMFS");
    }
    /* Two bcm96358 boards exist, bcm96358GW and bcm96358M, for now their
     * features are the same, but in the future if different features will be 
     * implemented, must configure two dists */
    else if (IS_DIST("BCM96358"))
    {
	hw = "BCM96358";
	os = "LINUX_26";

	token_set("CONFIG_RG_JPKG_DIST", "JPKG_MIPSEB");
	
	token_set_y("CONFIG_RG_MAINFS_IN_PHYS_MAPPED_FLASH");

	/* Small flash */
	token_set("CONFIG_RG_STANDARD_SIZE_CONF_SECTION", "n");
	token_set("CONFIG_RG_STANDARD_SIZE_IMAGE_SECTION", "n");

	/*  RG Priority 1  */
    	enable_module("MODULE_RG_FOUNDATION");
    	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_DSL");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_VIDEO_SURVEILLANCE");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
        enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_SSL_VPN");
	enable_module("MODULE_RG_REDUCE_SUPPORT");
	enable_module("MODULE_RG_OSAP_AGENT");
	if (token_get("CONFIG_RG_NETWORK_BOOT_IMAGE"))
	{
	    enable_module("CONFIG_HW_DSP");
	    enable_module("MODULE_RG_HOME_PBX");
	    enable_module("MODULE_RG_VOIP_ASTERISK_SIP");
	    enable_module("MODULE_RG_WEB_SERVER");
	    enable_module("MODULE_RG_FILESERVER");
	    enable_module("MODULE_RG_UPNP_AV");
	}
	enable_module("MODULE_RG_PSE");

	/*  RG Priority 2  */
	if (token_get("CONFIG_RG_NETWORK_BOOT_IMAGE"))
	{
	    enable_module("MODULE_RG_L2TP");
	    enable_module("MODULE_RG_PPTP");
	    enable_module("MODULE_RG_PRINTSERVER");
	}
	token_set_y("CONFIG_RG_PSE_SW_DRIVER_GIVE_UNALIGNED_IP");

	/*  RG Priority 3  */
	/*Not enough space on flash
	 * XXX: wrap with CONFIG_RG_NETWORK_BOOT_IMAGE
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_BLUETOOTH");
        */

	/*  RG Priority 4  */
	/*Not enough space on flash
	 * XXX: wrap with CONFIG_RG_NETWORK_BOOT_IMAGE
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_TR_064");
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_VOIP_ASTERISK_H323");	
	enable_module("MODULE_RG_VOIP_ASTERISK_MGCP_CALL_AGENT");
	enable_module("MODULE_RG_MAIL_SERVER");	
	enable_module("MODULE_RG_JVM");
	enable_module("MODULE_RG_FTP_SERVER");
        */	
	
	/*  RG Priority 7  */
	/*Not enough space on flash
	 * XXX: wrap with CONFIG_RG_NETWORK_BOOT_IMAGE
	enable_module("MODULE_RG_ROUTE_MULTIWAN");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_IPV6");
        */
    
    	token_set_y("CONFIG_ULIBC_SHARED");
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_SWITCH_LAN");
	token_set_y("CONFIG_HW_DSL_WAN");
	token_set_y("CONFIG_RG_NETTOOLS_ARP");

	if (token_get("CONFIG_RG_NETWORK_BOOT_IMAGE"))
	{
	    /* USB Host */
	    enable_module("CONFIG_HW_USB_HOST_EHCI");
	    enable_module("CONFIG_HW_USB_HOST_OHCI");
	    enable_module("CONFIG_HW_USB_STORAGE");
	}

	enable_module("CONFIG_HW_80211G_BCM43XX");
      	dev_add_bridge("br0", DEV_IF_NET_INT, "bcm1", NULL);

	token_set("LIBC_IN_TOOLCHAIN", "n");
	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MODULES_INITRAMFS");

	/* USB device */
	/* RNDIS is disabled due to lack of flash.
	 * XXX: wrap with CONFIG_RG_NETWORK_BOOT_IMAGE
	enable_module("CONFIG_HW_USB_RNDIS");
	*/
	dev_add_to_bridge_if_opt("br0", "usb0", "CONFIG_HW_USB_RNDIS");
    }
    else if (IS_DIST("HH1620_RARE"))
    {
	hw = "HH1620";
	os = "LINUX_26";

	token_set_y("CONFIG_RG_RARE");

#if 1
	token_set_y("CONFIG_HW_ETH_LAN");
#else
	token_set_y("CONFIG_HW_SWITCH_LAN");
	token_set_y("CONFIG_HW_SWITCH_PORT_SEPARATION");
#endif

	if (token_get("CONFIG_HW_SWITCH_PORT_SEPARATION"))
	{
	    token_set_y("CONFIG_HW_ETH_WAN");
	    switch_virtual_port_dev_add("eth0", "bcm1_main", 3, DEV_IF_NET_EXT);
	}	

	/* build */
	token_set("LIBC_IN_TOOLCHAIN", "n");

	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MODULES_INITRAMFS");
	token_set_y("CONFIG_RG_PERM_STORAGE_RAM");
    }
    else if (IS_DIST("BCM6358_3X_RARE"))
    {
	hw = "BCM6358";
	os = "LINUX_3X";

	token_set_y("CONFIG_RG_RARE");
	token_set_y("CONFIG_HW_ETH_LAN");
	token_set_y("CONFIG_HW_ETH_WAN");

	token_set("LIBC_IN_TOOLCHAIN", "n");
	token_set_y("CONFIG_DYN_LINK");

	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set("CONFIG_RG_INITFS_RAMFS_COMP_METHOD", "xz");
	token_set_y("CONFIG_RG_MAINFS");

	token_set_y("CONFIG_RG_PERM_STORAGE_RAM");	
    }
    else if (IS_DIST("BCM6358_3X") || IS_DIST("BCM6358_3X_8021Q_WAN"))
    {
	hw = "BCM6358";
	os = "LINUX_3X";
	int is_8021q_dist = IS_DIST("BCM6358_3X_8021Q_WAN");

	token_set_y("CONFIG_BOOTLDR_CFE");

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_PSE");
	enable_module("MODULE_RG_QOS");

	token_set_y("CONFIG_HW_ETH_LAN");
	if (is_8021q_dist)
	{
	    /* 8021q WAN distibution has both Lan & Wan as vlans */
	    token_set_y("CONFIG_RG_8021Q_IF");
	    dev_add("eth1.1", DEV_IF_USER_VLAN, DEV_IF_NET_INT);
	    dev_add("eth1.3", DEV_IF_USER_VLAN, DEV_IF_NET_EXT);	    
	}
	else
	{
	    dev_add_bridge("br0", DEV_IF_NET_INT, "eth1", NULL);
	    token_set_y("CONFIG_HW_ETH_WAN");
	}

	token_set("LIBC_IN_TOOLCHAIN", "n");
	token_set_y("CONFIG_DYN_LINK");

	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set("CONFIG_RG_INITFS_RAMFS_COMP_METHOD", "gzip");
	token_set_y("CONFIG_RG_MAINFS");
	token_set_y("CONFIG_RG_MODULES_INITRAMFS");

	token_set_y("CONFIG_RG_MAINFS_IN_PHYS_MAPPED_FLASH");
    }
    else if (IS_DIST("DWV_96358") || IS_DIST("HH1620") || IS_DIST("CT6382D"))
    {
	if (IS_DIST("HH1620"))
	    hw = "HH1620";
	else if (IS_DIST("DWV_96358"))
	    hw = "DWV_BCM96358";
	else if (IS_DIST("CT6382D"))
	    hw = "CT6382D";
	os = "LINUX_26";

	token_set("CONFIG_RG_JPKG_DIST", "JPKG_MIPSEB");

	token_set_y("CONFIG_BOOTLDR_CFE");
	if (!IS_DIST("CT6382D"))
	    token_set_y("CONFIG_BOOTLDR_CFE_DUAL_IMAGE");
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_VAS_CLIENT");
	enable_module("MODULE_RG_UPNP_AV");
	/* Additional LAN Interface - See below */
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
        enable_module("MODULE_RG_DSL");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN"); /* WLAN */
	enable_module("MODULE_RG_FILESERVER");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_VIDEO_SURVEILLANCE");
	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_TR_064");
	enable_module("MODULE_RG_PSE");
	enable_module("MODULE_RG_REDUCE_SUPPORT");
	enable_module("MODULE_RG_OSAP_AGENT");

	/* Additional LAN Interface */
	enable_module("CONFIG_HW_USB_RNDIS"); /* USB Slave */

	/* VOIP */
	enable_module("MODULE_RG_HOME_PBX");
	token_set_y("CONFIG_RG_VOIP_CALL_LOG");
	enable_module("MODULE_RG_VOIP_ASTERISK_SIP");
	enable_module("CONFIG_HW_DSP");
	if (IS_DIST("DWV_96358") || IS_DIST("HH1620"))
	    token_set_y("CONFIG_HW_FXO");

	/* WLAN */
	if (!token_get("CONFIG_RG_ATHEROS_HW_AR5416") &&
	    !token_get("CONFIG_RG_ATHEROS_HW_AR5212"))
	{
	    enable_module("CONFIG_HW_80211G_BCM43XX");
	}
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");

	/* HW switch */
	token_set_y("CONFIG_HW_SWITCH_LAN");
	/* DSL */
	if (token_get("MODULE_RG_DSL"))
	    token_set_y("CONFIG_HW_DSL_WAN");
	/* USB Host */
	enable_module("CONFIG_HW_USB_HOST_EHCI");
	enable_module("CONFIG_HW_USB_HOST_OHCI");
	enable_module("CONFIG_HW_USB_STORAGE");	
	
	dev_add_bridge("br0", DEV_IF_NET_INT, NULL);
	dev_add_to_bridge_if_opt("br0", "wl0", "CONFIG_HW_80211G_BCM43XX");
	dev_add_to_bridge_if_opt("br0", "usb0", "CONFIG_HW_USB_RNDIS");

	if (token_get("CONFIG_HW_SWITCH_PORT_SEPARATION"))
	{
	    token_set_y("CONFIG_HW_ETH_WAN");
	    switch_virtual_port_dev_add("eth0", "bcm1_main", 3, DEV_IF_NET_EXT);
	    dev_add_to_bridge("br0", "bcm1_main");
	}
	else
	    dev_add_to_bridge("br0", "bcm1");


	/* build */
	token_set("LIBC_IN_TOOLCHAIN", "n");

	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MODULES_INITRAMFS");
	token_set_y("CONFIG_RG_MAINFS_IN_PHYS_MAPPED_FLASH");
        
	token_set_y("CONFIG_HW_BUTTONS");

	token_set_m("CONFIG_RG_KLOG"); /* Availale only for the 8MB flash */

	if (IS_DIST("DWV_96358"))
	    token_set_y("CONFIG_HW_LEDS");
#if 0
	/* B37386: Footprint Reduction cause crashes in our dist... */
	token_set_y("CONFIG_RG_FOOTPRINT_REDUCTION");
#endif
    }
    else if (IS_DIST("DWV_96358_VODAFONE") || IS_DIST("TW_96358_VODAFONE") ||
	IS_DIST("BCM_96358_VODAFONE_TWONKY"))
    {
	char *wifi_dev;

	hw = "DWV_BCM96358";
	os = "LINUX_26";

	token_set("CONFIG_RG_JPKG_DIST", "JPKG_MIPSEB");
	token_set_y("CONFIG_BCM_ARL_VLAN_LEARNING");
	token_set_y("CONFIG_BOOTLDR_CFE");
	token_set_y("CONFIG_BOOTLDR_CFE_DUAL_IMAGE");
	enable_module("MODULE_RG_FOUNDATION");
	/* Additional LAN Interface - See below */
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
        enable_module("MODULE_RG_DSL");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN"); /* WLAN */
	/* Disable features in case of Twonky to gain memory */
   	if (IS_DIST("TW_96358_VODAFONE"))
	    enable_module("MODULE_RG_FILESERVER");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_TR_064");
	enable_module("MODULE_RG_PSE");
	token_set_y("CONFIG_RG_URL_KEYWORD_FILTER");

	/* VOIP */
	if (IS_DIST("DWV_96358_VODAFONE"))
	{
	    enable_module("MODULE_RG_HOME_PBX");
	    token_set_y("CONFIG_RG_VOIP_CALL_LOG");
	    enable_module("MODULE_RG_VOIP_ASTERISK_SIP");
	    enable_module("CONFIG_HW_DSP");
	    token_set_y("CONFIG_RG_3G_VOICE");

	    /* WBM */
	    token_set_y("CONFIG_WBM_VODAFONE");
	    token_set_y("CONFIG_RG_EXTERNAL_API");

            token_set_y("CONFIG_RG_DISK_MNG");
	}

	/* WLAN */
	enable_module("CONFIG_HW_80211G_BCM43XX");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	if (!token_get("CONFIG_RG_JPKG"))
	    token_set_y("CONFIG_BCM43XX_MULTIPLE_BSSID");

	/* HW switch */
	token_set_y("CONFIG_HW_SWITCH_LAN");
	/* DSL */
	token_set_y("CONFIG_HW_DSL_WAN");
	/* USB Host */
	enable_module("CONFIG_HW_USB_HOST_EHCI");
	enable_module("CONFIG_HW_USB_HOST_OHCI");
	enable_module("CONFIG_HW_USB_STORAGE");	
	enable_module("MODULE_RG_ROUTE_MULTIWAN");
	enable_module("MODULE_RG_MODULAR_UPGRADE");
	
	if (token_get("CONFIG_HW_SWITCH_PORT_SEPARATION"))
	{
	    token_set_y("CONFIG_HW_ETH_WAN");
	    switch_virtual_port_dev_add("eth0", "bcm1_main", 3, DEV_IF_NET_EXT);
	    dev_add_to_bridge("br0", "bcm1_main");
	}

	/* build */
	token_set("LIBC_IN_TOOLCHAIN", "n");

	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MODULES_INITRAMFS");
	token_set_y("CONFIG_RG_MAINFS_IN_PHYS_MAPPED_FLASH");
        
	token_set_y("CONFIG_HW_BUTTONS");

	token_set_m("CONFIG_RG_KLOG"); /* Availale only for the 8MB flash */

	token_set_y("CONFIG_HW_LEDS");
	
	wifi_dev = token_get("CONFIG_HW_80211G_BCM43XX") ? "wl0" : NULL;
	set_vodafone_common_configs("bcm1", wifi_dev, "002489");

	/* 3G Support */
	token_set_y("CONFIG_RG_3G_WAN");
	token_set_y("CONFIG_RG_3G_SMS");

        if (IS_DIST("BCM_96358_VODAFONE_TWONKY"))
	{
	    token_set_y("CONFIG_RG_TWONKY");
	    token_set("CONFIG_RG_TWONKY_FRIENDLYNAME",
		"Vodafone ADSL Router");
	    token_set_y("CONFIG_INOTIFY");
	    token_set_y("CONFIG_INOTIFY_USER");
	    token_set_y("CONFIG_RG_DISK_MNG");
	}

	/* Debugging tools */
#if 0	/* Enable those features only for development compilations */
	token_set_y("CONFIG_RG_DBG_ULIBC_MALLOC");
	/* Redirect malloc error messages from stdout to /dev/console */
	token_set_y("CONFIG_RG_DBG_MALLOC_2_CON");
	/* Crash task in case of found memory error */
	token_set_y("CONFIG_RG_DBG_ULIBC_MALLOC_CRASH");
	/* This feature fills freed memory with 0xeb value. If you found case of
	 * usage released memory - use is_mem_valid(ptr) for memory validation.
	 * Impairs board performance severely!!! */ 
	token_set("CONFIG_RG_DBG_ULIBC_MEM_POISON", "256");
#endif
    }
    else if (IS_DIST("DWV_96358_IPV6") || IS_DIST("DWV_96358_DSL_IPV6") || 
	IS_DIST("DWV_96358_ETH_IPV6"))
    {
	hw = "DWV_BCM96358";
	os = "LINUX_26";

	token_set("CONFIG_RG_JPKG_DIST", "JPKG_MIPSEB");

	token_set_y("CONFIG_BOOTLDR_CFE");
	token_set_y("CONFIG_BOOTLDR_CFE_DUAL_IMAGE");
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN"); /* WLAN */
	enable_module("MODULE_RG_IPV6");
	token_set_y("CONFIG_RG_IPROUTE2_UTILS");
	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_PSE");
	enable_module("MODULE_RG_HOME_PBX");
	token_set_y("CONFIG_RG_VOIP_CALL_LOG");
	enable_module("MODULE_RG_VOIP_ASTERISK_SIP");
	enable_module("CONFIG_HW_DSP");
	enable_module("MODULE_RG_FILESERVER");
	/* USB Host */
	enable_module("CONFIG_HW_USB_HOST_EHCI");
	enable_module("CONFIG_HW_USB_HOST_OHCI");
	enable_module("CONFIG_HW_USB_STORAGE");
#if 0
	/* XXX: Does not compile yet (because of PPP changes) */
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_L2TP");
#endif

	enable_module("MODULE_RG_UPNP");
	
	if (!IS_DIST("DWV_96358_ETH_IPV6"))
	    enable_module("MODULE_RG_DSL");
	if (!IS_DIST("DWV_96358_DSL_IPV6"))
	    token_set_y("CONFIG_HW_SWITCH_PORT_SEPARATION");

	enable_module("CONFIG_HW_80211G_BCM43XX");

	/* HW switch */
	token_set_y("CONFIG_HW_SWITCH_LAN");
	
	dev_add_bridge("br0", DEV_IF_NET_INT, NULL);
	dev_add_to_bridge_if_opt("br0", "wl0", "CONFIG_HW_80211G_BCM43XX");

	if (token_get("CONFIG_HW_SWITCH_PORT_SEPARATION"))
	{
	    token_set_y("CONFIG_HW_ETH_WAN");
	    switch_virtual_port_dev_add("eth0", "bcm1_main", 3, DEV_IF_NET_EXT);
	    dev_add_to_bridge("br0", "bcm1_main");
	}
	else
	    dev_add_to_bridge("br0", "bcm1");

	/* build */
	token_set("LIBC_IN_TOOLCHAIN", "n");

	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MODULES_INITRAMFS");
	token_set_y("CONFIG_RG_MAINFS_IN_PHYS_MAPPED_FLASH");
        
	token_set_y("CONFIG_HW_BUTTONS");
	token_set_y("CONFIG_HW_LEDS");

	/* DSL */
	if (token_get("MODULE_RG_DSL"))
	    token_set_y("CONFIG_HW_DSL_WAN");

	token_set_m("CONFIG_RG_KLOG"); /* Availale only for the 8MB flash */

#if 0
	/* B37386: Footprint Reduction cause crashes in our dist... */
	token_set_y("CONFIG_RG_FOOTPRINT_REDUCTION");
#endif
	
    }
    else if (IS_DIST("VF_ES_HG55VDFA") || IS_DIST("VF_ES_HG56BZRB"))
    {
	char *wifi_name;

	os = "LINUX_26";

	if (IS_DIST("VF_ES_HG55VDFA"))
	    hw = "HG55VDFA";
	else if (IS_DIST("VF_ES_HG56BZRB"))
	    hw = "HG56BZRB";

	token_set_y("CONFIG_RG_VODAFONE_ES");
	token_set("CONFIG_RG_JPKG_DIST", "JPKG_MIPSEB");

	token_set_y("CONFIG_BOOTLDR_CFE");
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_TR_064");
	enable_module("MODULE_RG_FILESERVER");
        enable_module("MODULE_RG_DSL");
	enable_module("MODULE_RG_PSE");
	enable_module("MODULE_RG_REDUCE_SUPPORT");

	enable_module("CONFIG_HW_USB_STORAGE");
	enable_module("CONFIG_HW_USB_HOST_EHCI");
	enable_module("CONFIG_HW_USB_HOST_OHCI");
	enable_module("MODULE_RG_ROUTE_MULTIWAN");
	enable_module("MODULE_RG_MODULAR_UPGRADE");
        enable_module("MODULE_RG_FTP_SERVER");

	token_set_y("CONFIG_RG_URL_KEYWORD_FILTER");

//	token_set_y("CONFIG_HW_LEDS");
//	token_set_y("CONFIG_HW_BUTTONS");

	token_set_y("CONFIG_HW_SWITCH_LAN");
	token_set_y("CONFIG_HW_DSL_WAN");

	token_set("LIBC_IN_TOOLCHAIN", "n");
 	token_set_y("CONFIG_RG_INITFS_RAMFS");

	/* WLAN */
	if (IS_DIST("VF_ES_HG55VDFA"))
	{
	    enable_module("CONFIG_RG_ATHEROS_HW_AR5416");
	    token_set_y("CONFIG_RG_ATHEROS_NO_EEPROM");
	    wifi_name = "ath0";
	}
	else if (IS_DIST("VF_ES_HG56BZRB"))
	{
	    enable_module("CONFIG_HW_80211N_RALINK_RT3062");
	    wifi_name = "ra0";
	}
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	//enable_module("CONFIG_RG_WPS"); // TODO
	token_set_y("CONFIG_RG_VODAFONE_WL_RESCAN");
	token_set_y("CONFIG_RG_VODAFONE_CUSTOM_FW_RULES");

	set_vodafone_common_configs("bcm1", wifi_name, "002489");
	set_vodafone_es_configs("bcm1", wifi_name);

	token_set_y("CONFIG_RG_3G_WAN");
	token_set_y("CONFIG_RG_3G_VOICE");

	/* VOIP */
	enable_module("MODULE_RG_HOME_PBX");
	token_set_y("CONFIG_RG_VOIP_CALL_LOG");
	enable_module("MODULE_RG_VOIP_ASTERISK_SIP");
	enable_module("CONFIG_HW_DSP");
	token_set_y("BRCM_VRG_COUNTRY_CFG_SPAIN");

	token_set_y("CONFIG_RG_FFS");
	token_set_y("CONFIG_RG_TFTP_UPGRADE");

	token_set_y("CONFIG_WBM_VODAFONE");

	token_set_m("CONFIG_RG_KLOG");

	/* Twonky Media Server */
        token_set_y("CONFIG_RG_TWONKY");
	token_set("CONFIG_RG_TWONKY_FRIENDLYNAME", "Vodafone ADSL Router");
        token_set_y("CONFIG_INOTIFY");
        token_set_y("CONFIG_INOTIFY_USER");

	/* Printer Support */
	enable_module("MODULE_RG_PRINTSERVER");

	token_set_y("CONFIG_RG_MULTIWAN");
	token_set_y("CONFIG_RG_EXTRA_MIME_TYPES");

	token_set_y("CONFIG_RG_ROUTED_LAN");

	token_set_y("CONFIG_RG_VODAFONE_MANUFACTURE");

	token_set_y("CONFIG_RG_MODULES_INITRAMFS");
    }
    else if (IS_DIST("VF_IT_HG55MAGV"))
    {
	hw = "HG55MAGV";
	os = "LINUX_26";

	token_set_y("CONFIG_RG_VODAFONE_IT");

	token_set("CONFIG_RG_JPKG_DIST", "JPKG_MIPSEB");

	token_set_y("CONFIG_BOOTLDR_CFE");
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_TR_064");
	enable_module("MODULE_RG_FILESERVER");
        enable_module("MODULE_RG_DSL");
	enable_module("MODULE_RG_PSE");
	enable_module("MODULE_RG_REDUCE_SUPPORT");

	enable_module("MODULE_RG_ROUTE_MULTIWAN");
        enable_module("MODULE_RG_FTP_SERVER");

	enable_module("MODULE_RG_MODULAR_UPGRADE");

	token_set_y("CONFIG_RG_URL_KEYWORD_FILTER");

	token_set("CONFIG_RG_SAMBA_CLIENT", "n");

	/* WLAN */
        enable_module("CONFIG_HW_80211G_BCM43XX");
        enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
//	XXX require implementation for bcm43xx, does not seem driver supports
//	enable_module("CONFIG_RG_WPS");

//	XXX check if needed (B102564), might need implementation for driver
//	token_set_y("CONFIG_RG_VODAFONE_WL_RESCAN");

	/* HW switch */
	token_set_y("CONFIG_HW_SWITCH_LAN");

	/* DSL */
	token_set_y("CONFIG_HW_DSL_WAN");
	
	/* USB Host */
	enable_module("CONFIG_HW_USB_HOST_EHCI");
	enable_module("CONFIG_HW_USB_HOST_OHCI");
	enable_module("CONFIG_HW_USB_STORAGE");

	token_set_y("CONFIG_RG_VODAFONE_CUSTOM_FW_RULES");

	set_vodafone_common_configs("bcm1", "wl0", "002489");
        set_vodafone_it_configs("bcm1", "bcm1", NULL, "wl0");

	/* 3G Support */
	token_set_y("CONFIG_RG_3G_WAN");
	token_set_y("CONFIG_RG_3G_SMS");
	token_set_y("CONFIG_RG_3G_SMS_WHITELIST");
	token_set_y("CONFIG_RG_3G_SMS_MSISDN");
	token_set_y("CONFIG_RG_3G_VOICE");

	/* VOIP */
	enable_module("MODULE_RG_HOME_PBX");
	token_set_y("CONFIG_RG_VOIP_CALL_LOG");
	enable_module("MODULE_RG_VOIP_ASTERISK_SIP");
	enable_module("CONFIG_HW_DSP");
	token_set_y("CONFIG_RG_VODAFONE_WORKAROUND_DELAY_SIP_REG");
	/* ipphone backup support */
	token_set_y("CONFIG_RG_VODAFONE_IPPHONE_BACKUP");
	token_set_y("BRCM_VRG_COUNTRY_CFG_ITALY");
	/* Voice Template Extensions */
	token_set_y("CONFIG_RG_VOIP_TEMPLATE_EXTENSIONS");
	token_set_y("CONFIG_RG_VODAFONE_DATA_LAN_VOIP_EXTENSIONS");

	token_set_y("CONFIG_RG_FFS");
	token_set_y("CONFIG_RG_TFTP_UPGRADE");

	/* WBM */
	token_set_y("CONFIG_WBM_VODAFONE");
	token_set_y("CONFIG_RG_EXTERNAL_API");

	/* build */
	token_set("LIBC_IN_TOOLCHAIN", "n");

	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MODULES_INITRAMFS");

//	XXX cramfs in flash?	
//	token_set_y("CONFIG_RG_MAINFS_IN_PHYS_MAPPED_FLASH");

//	XXX check if needed - only useful with cramfs in flash
//	token_set_y("CONFIG_RG_PERM_STORAGE_INIT_OPTIMIZATION");

//	this requires a flash section
//	token_set_m("CONFIG_RG_KLOG");

	/* Printer Support */
	enable_module("MODULE_RG_PRINTSERVER");

//	XXX requires rg module implementation in vendor/broadcom/bcm963xx
//	token_set_y("CONFIG_RG_HW_WATCHDOG");

	token_set_y("CONFIG_RG_MULTIWAN");
	token_set_y("CONFIG_RG_EXTRA_MIME_TYPES");

	token_set_y("CONFIG_RG_VODAFONE_DSL_MONITOR_HACK");

//	XXX probably not needed - not manufacturing, only upgrading
//	turned on for now since a func there is needed. (erase_ccf_flash...)
	token_set_y("CONFIG_RG_VODAFONE_MANUFACTURE");

	token_set_y("CONFIG_HW_BUTTONS");
	token_set_y("CONFIG_HW_LEDS");
    }
    else if (IS_DIST("DWV_96348"))
    {
    	hw = "DWV_BCM96348";
	os = "LINUX_26";
	
	token_set_y("CONFIG_HW_DWV_96348");

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");

	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_URL_FILTERING");
	token_set("CONFIG_RG_SURFCONTROL_PARTNER_ID", "6003");
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_VIDEO_SURVEILLANCE");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_TR_064");

	/* Additional LAN Interface */
	enable_module("CONFIG_HW_USB_RNDIS"); /* USB Slave */

	/* WLAN */
	enable_module("CONFIG_HW_80211G_BCM43XX");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");

	/* HW switch */
#if 0
	/* B38935: HW switch does not work */
	token_set_y("CONFIG_HW_SWITCH_LAN");
#else
	token_set_y("CONFIG_HW_ETH_LAN");
#endif

	/* ETH WAN. TODO: Set as DMZ? */
	token_set_y("CONFIG_HW_ETH_WAN");

	/* DSL */
	enable_module("MODULE_RG_DSL");
	token_set_y("CONFIG_HW_DSL_WAN");

	/* USB Host */
#if 0
	enable_module("CONFIG_HW_USB_HOST_EHCI");
	enable_module("CONFIG_HW_USB_HOST_OHCI");
	enable_module("MODULE_RG_FILESERVER");
	enable_module("MODULE_RG_PRINTSERVER");
	enable_module("CONFIG_HW_USB_STORAGE");   
#endif

	token_set_m("CONFIG_HW_BUTTONS");
        token_set_y("CONFIG_HW_LEDS");

	dev_add_bridge("br0", DEV_IF_NET_INT, "bcm1", "wl0", "usb0", NULL);

	/* build */
	token_set("LIBC_IN_TOOLCHAIN", "n");
	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MODULES_INITRAMFS");

	token_set_y("CONFIG_RG_FOOTPRINT_REDUCTION");
    }
    else if (IS_DIST("RGLOADER_UML"))
    {
	hw = "UML";
	os = "LINUX_24";

	token_set_y("CONFIG_RG_RGLOADER");
	token_set_m("CONFIG_RG_KRGLDR");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_LAN");
	token_set_y("CONFIG_RG_TELNETS");
	token_set_y("CONFIG_RG_DYN_FLASH_LAYOUT");
	token_set("CONFIG_RG_JPKG_DIST", "JPKG_UML");
    }
    else if (IS_DIST("JPKG_SRC"))
    {
	hw = "JPKG";
        token_set("JPKG_ARCH", "\"src\"");
	token_set_y("CONFIG_RG_JPKG_SRC");

	set_jpkg_dist_configs(dist);
    }
    else if (IS_DIST("JPKG_UML"))
    {
	hw = "JPKG";
        token_set("JPKG_ARCH", "\"uml\"");
        token_set_y("CONFIG_RG_JPKG_UML");

	set_jpkg_dist_configs(dist);
    }
    else if (IS_DIST("JPKG_ARMV5B"))
    {
	hw = "JPKG";
        token_set("JPKG_ARCH", "\"armv5b\"");
	token_set_y("CONFIG_RG_JPKG_ARMV5B");
	    
	token_set_y("CONFIG_GLIBC");
	token_set_y("GLIBC_IN_TOOLCHAIN");

	set_jpkg_dist_configs(dist);
    }
    else if (IS_DIST("JPKG_ARMV6J"))
    {
	hw = "JPKG";
        token_set("JPKG_ARCH", "\"armv6j\"");
        token_set_y("CONFIG_RG_JPKG_ARMV6J");

	set_jpkg_dist_configs(dist);
    }
    else if (IS_DIST("JPKG_MIPSEB"))
    {
	hw = "JPKG";
        token_set("JPKG_ARCH", "\"mipseb\"");
        token_set_y("CONFIG_RG_JPKG_MIPSEB");

	set_jpkg_dist_configs(dist);
    }
    else if (IS_DIST("JPKG_OCTEON"))
    {
	hw = "JPKG";
	token_set("JPKG_ARCH", "\"octeon\"");

	token_set_y("CONFIG_RG_JPKG_OCTEON");

	set_jpkg_dist_configs(dist);
    }    
    else if (IS_DIST("JPKG_MIPSEB_XWAY"))
    {
	hw = "JPKG";
        token_set("JPKG_ARCH", "\"mipseb-xway\"");
        token_set_y("CONFIG_RG_JPKG_MIPSEB_XWAY");

	set_jpkg_dist_configs(dist);
    }
    else if (IS_DIST("JPKG_MIPSEB_BCM9636X") ||
	IS_DIST("JPKG_MIPSEB_BCM9636X_3X"))
    {
	hw = "JPKG";
        token_set("JPKG_ARCH", "\"mipseb-bcm9636x\"");
        token_set_y("CONFIG_RG_JPKG_MIPSEB_BCM9636X");
	token_set_y("CONFIG_RG_VODAFONE_DSL_MONITOR_HACK");

	set_jpkg_dist_configs(dist);
    }
    else if (IS_DIST("JPKG_MIPSEB_BCM9636X_BSP_4_12") ||
	IS_DIST("JPKG_MIPSEB_BCM9636X_BSP_4_12_IT") ||
	IS_DIST("JPKG_MIPSEB_BCM9636X_BSP_4_12_UK") ||
	IS_DIST("JPKG_MIPSEB_BCM9636X_BSP_4_12_DE"))
    {
	hw = "JPKG";
        token_set("JPKG_ARCH", "\"mipseb-bcm9636x-4_12\"");
        token_set_y("CONFIG_RG_JPKG_MIPSEB_BCM9636X_BSP_4_12");
	token_set_y("CONFIG_RG_VODAFONE_DSL_MONITOR_HACK");

	set_jpkg_dist_configs(dist);
    }
    else if (IS_DIST("JPKG_LOCAL_I386"))
    {
	hw = NULL;
        token_set("JPKG_ARCH", "\"local-i386\"");
	token_set_y("CONFIG_RG_JPKG_LOCAL_I386");

	set_jpkg_dist_configs(dist);
    }
    else if (IS_DIST("UML_LSP"))
    {
	hw = "UML";
	os = "LINUX_24";

	token_set_y("CONFIG_RG_UML");

	token_set_y("CONFIG_LSP_DIST");
    }
    else if (IS_DIST("UML_LSP_26"))
    {
	hw = "UML";
	os = "LINUX_26";

	token_set_y("CONFIG_RG_UML");
	
	token_set_y("CONFIG_GLIBC");

	token_set_y("CONFIG_LSP_DIST");
    }
    else if (IS_DIST("UML_3X"))
    {
	hw = "UML";
	os = "LINUX_3X";

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_PSE");
	enable_module("MODULE_RG_QOS");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");

	dev_add("br0", DEV_IF_BRIDGE, DEV_IF_NET_INT);
	token_set_y("CONFIG_DEF_BRIDGE_LANS");
    }
    else if (IS_DIST("UML") || IS_DIST("UML_GLIBC") || IS_DIST("UML_DUAL_WAN")
	|| IS_DIST("UML_ATM") || IS_DIST("UML_26") || IS_DIST("UML_VALGRIND")
	|| IS_DIST("UML_OPENRG"))
    {
	hw = "UML";

	if (IS_DIST("UML_26") || IS_DIST("UML_VALGRIND")
	    || IS_DIST("UML_OPENRG"))
	{
	    os = "LINUX_26";
	}

	if (IS_DIST("UML_GLIBC"))
	{
	    token_set_y("CONFIG_GLIBC");
	    token_set_y("GLIBC_IN_TOOLCHAIN");
	}

	if (IS_DIST("UML_VALGRIND"))
	    token_set_y("CONFIG_VALGRIND");

	if (IS_DIST("UML_OPENRG"))
	    token_set_y("CONFIG_RG_INTEGRAL");
	else
	{
	    token_set_y("CONFIG_RG_SMB");
	    /* Comment out after B162267 is fixed */	   
	    //token_set_y("CONFIG_RG_SPEED_TEST");
	    enable_module("MODULE_RG_VIDEO_SURVEILLANCE");
	    enable_module("MODULE_RG_IPSEC");
	    enable_module("MODULE_RG_PPTP");
	    enable_module("MODULE_RG_SNMP");
	    enable_module("MODULE_RG_IPV6");
	    token_set_y("CONFIG_RG_IPV6_OVER_IPV4_TUN");
	    enable_module("MODULE_RG_ADVANCED_ROUTING");
	    enable_module("MODULE_RG_L2TP");
	    enable_module("MODULE_RG_URL_FILTERING");
	    enable_module("MODULE_RG_ROUTE_MULTIWAN");
	    enable_module("MODULE_RG_JVM");
	    enable_module("MODULE_RG_BLUETOOTH");
	    enable_module("MODULE_RG_WEB_SERVER");
	    enable_module("MODULE_RG_PRINTSERVER");
	    enable_module("MODULE_RG_MAIL_SERVER");
	    enable_module("MODULE_RG_FTP_SERVER");
	    enable_module("MODULE_RG_SSL_VPN");
	    enable_module("MODULE_RG_ANTIVIRUS_NAC");
	    enable_module("MODULE_RG_ANTIVIRUS_LAN_PROXY");
	    enable_module("MODULE_RG_RADIUS_SERVER");
	}

	enable_module("MODULE_RG_PSE");

	/* ALL OpenRG Available Modules - ALLMODS */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_TR_064");
	enable_module("MODULE_RG_REDUCE_SUPPORT");
	enable_module("MODULE_RG_OSAP_AGENT");
	enable_module("MODULE_RG_VOIP_ASTERISK_SIP");
	enable_module("MODULE_RG_HOME_PBX");
	enable_module("MODULE_RG_FILESERVER");
	enable_module("MODULE_RG_UPNP_AV");
        enable_module("CONFIG_HW_80211G_UML_WLAN");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_UML_LOOP_STORAGE"); /* UML Disk Emulation */
	token_set_y("CONFIG_HW_ETH_WAN");
	enable_module("CONFIG_HW_DSP");
	if (IS_DIST("UML_DUAL_WAN"))
	    token_set_y("CONFIG_HW_ETH_WAN2");
	if (IS_DIST("UML_ATM"))
	{
	    enable_module("MODULE_RG_DSL");
	    token_set_y("CONFIG_HW_DSL_WAN");
	}
	token_set_y("CONFIG_HW_ETH_LAN");
	token_set_y("CONFIG_HW_ETH_LAN2");
	
	/* When B50873 is fixed then the if (MODULE_RG_VLAN) should be
	 * removed.*/
	if (token_get("MODULE_RG_VLAN"))
	    token_set_y("CONFIG_HW_SWITCH_LAN");
	
	token_set("CONFIG_RG_JPKG_DIST", "JPKG_UML");
	token_set_y("CONFIG_RG_DYN_FLASH_LAYOUT");
	token_set_y("CONFIG_RG_MT_PROFILING_FULL_INFO");
	token_set_y("CONFIG_RG_SSL_ROOT_CERTIFICATES");

	/* All devices will be added according to the CONFIGS in HW config.
	 * Note: We assume that we have a lan bridge and at least one device
	 * will be enslaved to it...
	 */
	dev_add("br0", DEV_IF_BRIDGE, DEV_IF_NET_INT);
	token_set_y("CONFIG_DEF_BRIDGE_LANS");

    }
    else if (IS_DIST("UML_VODAFONE") || IS_DIST("UML_VODAFONE_ES"))
    {
	hw = "UML";
	os = "LINUX_26";

	if (IS_DIST("UML_VODAFONE"))
	    token_set_y("CONFIG_RG_VODAFONE_IT");
	else if (IS_DIST("UML_VODAFONE_ES"))
	    token_set_y("CONFIG_RG_VODAFONE_ES");

	token_set_y("CONFIG_RG_SMB");
	enable_module("MODULE_RG_PSE");

	/* ALL OpenRG Available Modules - ALLMODS */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_TR_064");
	enable_module("MODULE_RG_REDUCE_SUPPORT");
	enable_module("MODULE_RG_VOIP_ASTERISK_SIP");
	enable_module("MODULE_RG_HOME_PBX");
	enable_module("MODULE_RG_UPNP_AV");
        enable_module("CONFIG_HW_80211G_UML_WLAN");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	token_set_y("CONFIG_RG_VOIP_CALL_LOG");
        enable_module("CONFIG_HW_USB_STORAGE");
	enable_module("MODULE_RG_PRINTSERVER");
	enable_module("MODULE_RG_MODULAR_UPGRADE");
        enable_module("MODULE_RG_FTP_SERVER");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_UML_LOOP_STORAGE"); /* UML Disk Emulation */
	token_set_y("CONFIG_HW_ETH_WAN");
	enable_module("CONFIG_HW_DSP");
	token_set_y("CONFIG_HW_ETH_LAN");
	token_set_y("CONFIG_HW_ETH_LAN2");
	
	/* When B50873 is fixed then the if (MODULE_RG_VLAN) should be
	 * removed.*/
	if (token_get("MODULE_RG_VLAN"))
	    token_set_y("CONFIG_HW_SWITCH_LAN");
	
	token_set("CONFIG_RG_JPKG_DIST", "JPKG_UML");
	token_set_y("CONFIG_RG_DYN_FLASH_LAYOUT");
	token_set_y("CONFIG_RG_MT_PROFILING_FULL_INFO");
	token_set_y("CONFIG_RG_SSL_ROOT_CERTIFICATES");

	/* All devices will be added according to the CONFIGS in HW config.
	 * Note: We assume that we have a lan bridge and at least one device
	 * will be enslaved to it...
	 */
	dev_add("br0", DEV_IF_BRIDGE, DEV_IF_NET_INT);
	token_set_y("CONFIG_DEF_BRIDGE_LANS");

	/* 3G Support */
	token_set_y("CONFIG_RG_3G_WAN");
	token_set_y("CONFIG_RG_3G_SMS");
        token_set_y("CONFIG_RG_3G_SMS_WHITELIST");

	token_set_y("CONFIG_RG_VODAFONE_CUSTOM_FW_RULES");
	token_set_y("CONFIG_RG_URL_KEYWORD_FILTER");
	set_vodafone_common_configs("hw_switch0", NULL, "002489");
	if (token_get("CONFIG_RG_VODAFONE_IT"))
	    set_vodafone_it_configs("hw_switch0", "hw_switch0", NULL, NULL);
	if (token_get("CONFIG_RG_VODAFONE_ES"))
	    set_vodafone_es_configs("hw_switch0", NULL);

	token_set_y("CONFIG_WBM_VODAFONE");
        enable_module("MODULE_RG_FILESERVER");
	token_set_y("CONFIG_RG_FILES_ON_HOST");
	token_set_y("CONFIG_RG_MULTIWAN");
    }
    else if (IS_DIST("UML_IPPHONE"))
    {
	hw = "UML";

	/* ALL OpenRG Available Modules - ALLMODS */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_VIDEO_SURVEILLANCE");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_SNMP");
	/* For Customer distributions only:
	 * When removing IPV6 you must replace in feature_config.c the line 
	 * if(token_get("MODULE_RG_IPV6")) with if(1) */
	enable_module("MODULE_RG_IPV6");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_VOIP_RV_SIP");
	enable_module("MODULE_RG_VOIP_RV_MGCP");
	enable_module("MODULE_RG_VOIP_RV_H323");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
	enable_module("CONFIG_HW_DSP");

	/* VoIP */
	token_set_y("CONFIG_RG_IPPHONE");
    }
    else if (IS_DIST("UML_IPPHONE_VALGRIND"))
    {
	hw = "UML";

	token_set_y("CONFIG_RG_SMB");

	/* ALL OpenRG Available Modules - ALLMODS */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_VIDEO_SURVEILLANCE");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_SNMP");
	/* For Customer distributions only:
	 * When removing IPV6 you must replace in feature_config.c the line 
	 * if(token_get("MODULE_RG_IPV6")) with if(1) */
	enable_module("MODULE_RG_IPV6");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_VOIP_RV_SIP");
	enable_module("MODULE_RG_VOIP_RV_MGCP");
	enable_module("MODULE_RG_VOIP_RV_H323");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
	enable_module("CONFIG_HW_DSP");

	token_set_y("CONFIG_VALGRIND");

	/* VoIP */
	token_set_y("CONFIG_RG_IPPHONE");
	token_set_y("CONFIG_RG_DYN_FLASH_LAYOUT");
    }
    else if (IS_DIST("UML_ATA_OSIP"))
    {
	hw = "UML";

	token_set_y("CONFIG_RG_SMB");
	token_set("CONFIG_RG_JPKG_DIST", "JPKG_UML");
        
	/* ALL OpenRG Available Modules - ALLMODS */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_VIDEO_SURVEILLANCE");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_SNMP");
	/* For Customer distributions only:
	 * When removing IPV6 you must replace in feature_config.c the line 
	 * if(token_get("MODULE_RG_IPV6")) with if(1) */
	enable_module("MODULE_RG_IPV6");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_VOIP_OSIP");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
	enable_module("CONFIG_HW_DSP");

	/* VoIP */
	enable_module("MODULE_RG_ATA");
	token_set_y("CONFIG_RG_DYN_FLASH_LAYOUT");
	token_set_y("CONFIG_RG_MT_PROFILING_FULL_INFO");
    }
    else if (IS_DIST("686_3X_LSP"))
    {
	hw = "PC_686";
	os = "LINUX_3X";

	token_set_y("CONFIG_LSP_DIST");

	token_set_y("CONFIG_HW_ETH_LAN");

	token_set("LIBC_IN_TOOLCHAIN", "n");
	token_set_y("CONFIG_DYN_LINK");

	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set("CONFIG_RG_INITFS_RAMFS_COMP_METHOD", "none");
	token_set_y("CONFIG_RG_MAINFS_BUILTIN");
	token_set_y("CONFIG_KERNEL_GZIP");
    }        
    else if (IS_DIST("FULL"))
    {
	token_set_y("CONFIG_RG_SMB");

	/* All OpenRG available Modules */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_VIDEO_SURVEILLANCE");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_IPV6");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_VOIP_RV_SIP");
	enable_module("MODULE_RG_VOIP_RV_MGCP");
	enable_module("MODULE_RG_VOIP_RV_H323");
        enable_module("MODULE_RG_DSL");
        enable_module("MODULE_RG_FILESERVER");
	enable_module("MODULE_RG_UPNP_AV");
        enable_module("MODULE_RG_PRINTSERVER");
        enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
        enable_module("MODULE_RG_CABLEHOME");
        enable_module("MODULE_RG_VODSL");
    }
    else if (IS_DIST("TNETC550W_LSP") || IS_DIST("BVW3653_LSP"))
    {
	if (IS_DIST("TNETC550W_LSP"))
	    hw = "TNETC550W";
	else if (IS_DIST("BVW3653_LSP"))
	    hw = "BVW3653";
	os = "LINUX_26";

	token_set_y("CONFIG_LSP_DIST");
	token_set("LIBC_IN_TOOLCHAIN", "n");

	token_set_y("CONFIG_HW_ETH_LAN");

	token_set_y("CONFIG_DYN_LINK");

	token_set_y("CONFIG_RG_INITFS_RAMFS");
    }
    else if (IS_DIST("TNETC550W"))
    {
	hw = "TNETC550W";
	os = "LINUX_26";

	token_set_y("CONFIG_TNETC550W_CM");

	token_set("LIBC_IN_TOOLCHAIN", "n");

	enable_module("MODULE_RG_FOUNDATION");
	/* QOS Conflicts with DOCSIS */
	/* enable_module("MODULE_RG_QOS"); */
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_TR_064");

	token_set_y("CONFIG_HW_ETH_LAN");
	token_set_y("CONFIG_HW_CABLE_WAN");

	token_set_y("CONFIG_RG_DOCSIS");
	token_set_y("CONFIG_RG_PACKET_CABLE");
	token_set_y("CONFIG_DYN_LINK");
    }
    else if (IS_DIST("MVG3420N") || IS_DIST("BVW3653") ||
	IS_DIST("BVW3653_OPENRG") || IS_DIST("BVW3653A1_OPENRG"))
    {
	hw = IS_DIST("MVG3420N") ? "MVG3420N" : "BVW3653";
	os = "LINUX_26";
	int is_integral = IS_DIST("BVW3653_OPENRG") ||
	    IS_DIST("BVW3653A1_OPENRG");

	token_set_y("CONFIG_TNETC550W_CM");
	if (!IS_DIST("MVG3420N"))
	    token_set_y("CONFIG_HITRON_BOARD");

	token_set("LIBC_IN_TOOLCHAIN", "n");
	token_set_y("CONFIG_RG_FOOTPRINT_REDUCTION");

	if (is_integral)
	{
	    token_set_y("CONFIG_RG_INTEGRAL");
	    token_set("DEVICE_MANUFACTURER_STR", "Hitron Technologies");
	}

	/*  RG Priority 1  */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	/* QOS Conflicts with DOCSIS */
	/* enable_module("MODULE_RG_QOS"); */
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	enable_module("CONFIG_RG_WPS");
	enable_module("MODULE_RG_REDUCE_SUPPORT");
	enable_module("MODULE_RG_OSAP_AGENT");
	enable_module("MODULE_RG_PSE");
	if (!is_integral)
	    enable_module("MODULE_RG_VAS_CLIENT");

	/*  RG Priority 2  */
	enable_module("MODULE_RG_UPNP");
	if (is_integral)
	    enable_module("MODULE_RG_FILESERVER");
	
	/*  RG Priority 3  */
	//enable_module("MODULE_RG_PRINTSERVER");
	enable_module("MODULE_RG_UPNP_AV");
	if (!is_integral)
	{
	    enable_module("MODULE_RG_L2TP");
	    enable_module("MODULE_RG_PPTP");
	    token_set_y("CONFIG_RG_SPEED_TEST");
	}

	/* USB Host */
	enable_module("CONFIG_HW_USB_STORAGE");	

	/* General */
	token_set_y("CONFIG_HW_SWITCH_LAN");
	if (IS_DIST("BVW3653A1_OPENRG"))
	    enable_module("CONFIG_HW_80211N_RALINK_RT3052");
	else
	    enable_module("CONFIG_HW_80211N_RALINK_RT2880");
	if (is_integral)
	{
#ifdef CONFIG_RG_DO_DEVICES
	    token_set("CONFIG_RG_DEFAULT_80211N_MODE", "%d", DOT11_MODE_NG);
#endif
	    token_set_y("CONFIG_RG_VAP_SECURED");
	    if (token_get("MODULE_RG_REDUCE_SUPPORT"))
		token_set_y("CONFIG_RG_VAP_HELPLINE");
	}
	token_set_y("CONFIG_HW_CABLE_WAN");
	enable_module("MODULE_RG_VLAN");

	/* Docsis */
	token_set_y("CONFIG_RG_DOCSIS");
	token_set_y("CONFIG_RG_PACKET_CABLE");
	token_set_y("CONFIG_DYN_LINK");

        /* Crash logger */
	token_set_m("CONFIG_RG_KLOG");
	token_set_y("CONFIG_RG_KLOG_RAM_BE");
	token_set_y("CONFIG_RG_KLOG_EMAIL");
	token_set("CONFIG_RG_KLOG_RAMSIZE", "0x100000");

	token_set_m("CONFIG_HW_BUTTONS");
	if (IS_DIST("MVG3420N"))
	    token_set_m("CONFIG_HW_LEDS");

	/* Fast path */
	if (token_get("MODULE_RG_PSE"))
	    token_set_y("CONFIG_RG_PSE_HW");

	if (!is_integral && token_get("CONFIG_HW_SWITCH_PORT_SEPARATION"))
	{
	    /* Switch port 4 is a WAN interface. */
	    switch_virtual_port_dev_add("eth1", "eth0_main", 4, DEV_IF_NET_EXT);
	}
    }
    else if (IS_DIST("ADA00X_LSP"))
    {
	hw = "ADA00X";
	os = "LINUX_26";

	token_set_y("CONFIG_LSP_DIST");
	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MAINFS_IN_PHYS_MAPPED_FLASH");
	token_set("LIBC_IN_TOOLCHAIN", "n");
	
	token_set_y("CONFIG_HW_ETH_LAN");

	token_set_y("CONFIG_RG_MTD_UTILS");

	/* use LSP for UBI writing */
	token_set("CONFIG_RG_UBI_PARTITION", "UBI");
	/* Partion Size ncludes the volume size, 4 PEBs for UBI and 80 
	 * possible bab-blocks (worst case scenario).
	 */
	token_set("CONFIG_RG_UBI_PARTITION_SIZE", "0x1f400000");
	/* 1MiB for u-boot and it's data at the beginning of the flash */
	token_set("CONFIG_RG_UBI_PARTITION_OFFSET", "0x100000");
	token_set_y("CONFIG_MTD_UBI");
    }
    else if (IS_DIST("ADA00X"))
    {
	hw = "ADA00X";
	os = "LINUX_26";

	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MODULES_INITRAMFS");
	token_set_y("CONFIG_RG_MAINFS_IN_PHYS_MAPPED_FLASH");

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	    
	/* Devices */
	token_set_y("CONFIG_HW_ETH_LAN");

	token_set("LIBC_IN_TOOLCHAIN", "n");

	/* Voice */
	enable_module("MODULE_RG_HOME_PBX");
	enable_module("MODULE_RG_VOIP_ASTERISK_SIP");
	enable_module("CONFIG_HW_DSP");

	token_set_y("CONFIG_RG_PERM_STORAGE_UBI");
	/* Volume size for permst - OpenRG Flash layout */
	token_set("CONFIG_RG_PERM_STORAGE_UBI_VOLUME_SIZE", "0x33ae000");
	/* Partion Size ncludes the volume size, 4 PEBs for UBI and 80 
	 * possible bab-blocks (worst case scenario).
	 */
	token_set("CONFIG_RG_UBI_PARTITION_SIZE", "0x3f00000");
	/* 1MiB for u-boot and it's data at the beginning of the flash */
	token_set("CONFIG_RG_UBI_PARTITION_OFFSET", "0x100000");

	enable_module("CONFIG_HW_80211N_RALINK_RT3883");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	token_set_y("CONFIG_RG_VAP_SECURED");
	token_set_y("CONFIG_RG_VAP_GUEST");

	token_set_m("CONFIG_HW_BUTTONS");

	enable_module("MODULE_RG_PSE");
	if (token_get("MODULE_RG_PSE"))
	    token_set_y("CONFIG_RG_PSE_HW");
	
	token_set_y("CONFIG_HW_ISDN");
	token_set_y("CONFIG_RG_VOIP_ASTERISK_CAPI");
	token_set_y("CONFIG_NETBRICKS_ISDN_STACK");
    }
    else if (IS_DIST("VOX_2.0_DE") || IS_DIST("VOX_2.0_DE_VDSL")
	|| IS_DIST("VOX_2.0_INT"))
    {
	static char ubi_vol[255];

	/* VOX_2.0_INT is currently based on VOX_2.0_DE */
	if (IS_DIST("VOX_2.0_DE"))
	    hw = "GRX288_VOX_2.0_DE";
	else if (IS_DIST("VOX_2.0_DE_VDSL"))
	    hw = "VRX288_VOX_2.0_DE";
	else if (IS_DIST("VOX_2.0_INT"))
	    hw = "GRX288_VOX_2.0_INT";

	os = "LINUX_26";

	token_set("CONFIG_RG_JPKG_DIST", "JPKG_MIPSEB_XWAY");

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_VLAN"); 
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT"); 
	enable_module("MODULE_RG_PPP"); 
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_PSE"); 
	enable_module("MODULE_RG_ROUTE_MULTIWAN");
	token_set_y("CONFIG_RG_DSLHOME_USB");
	token_set_y("CONFIG_RG_POWER_MANAGEMENT");
	token_set_y("CONFIG_RG_PSE_HW");
	/* enable_module("MODULE_RG_IPSEC"); Not supported yet */
	enable_module("MODULE_RG_REDUCE_SUPPORT");

	enable_module("MODULE_RG_IPV6");
	token_set_y("CONFIG_RG_IPROUTE2_UTILS");
	token_set_y("CONFIG_RG_DSLHOME_DATAMODEL_TR_181");
	token_set_y("CONFIG_RG_DSLHOME_DATAMODEL_TR_181_ENCAPSULATED");

	enable_module("CONFIG_HW_USB_STORAGE");
	enable_module("MODULE_RG_FILESERVER");

	enable_module("MODULE_RG_MODULAR_UPGRADE");

	token_set_y("CONFIG_RG_URL_KEYWORD_FILTER");
	enable_module("MODULE_RG_FTP_SERVER");

	/* Not supported yet 
	token_set_y("CONFIG_HW_LEDS"); */
	token_set_y("CONFIG_HW_BUTTONS");

	/* XXX: Temporary, until LCD is supported. */
	if (!IS_DIST("VOX_2.0_INT"))
	{
	    token_set_y("CONFIG_HW_LCD");
	    token_set_y("CONFIG_HW_TOUCH_BUTTONS");
	}

	/* Not supported yet 
	 * token_set_y("CONFIG_HW_SWITCH_LAN");
	 */
	token_set_y("CONFIG_HW_ETH_LAN");

	/* build */
	token_set("LIBC_IN_TOOLCHAIN", "n");

	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MODULES_INITRAMFS");
	token_set_y("CONFIG_RG_MAINFS_IN_UBI");

        /* WLAN */
        enable_module("CONFIG_HW_80211N_RALINK_RT3883");
        enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
        token_set_y("CONFIG_RG_VAP_GUEST");
        enable_module("CONFIG_RG_WPS");
        token_set_y("CONFIG_RG_WPS_PHYSICAL_PBC");
        token_set_y("CONFIG_RG_WPS_VIRTUAL_PBC");
        token_set_y("CONFIG_RG_WPS_VIRTUAL_DISPLAY_PIN");
        token_set_y("CONFIG_RG_WPS_KEYPAD");
        token_set_y("CONFIG_RG_WIRELESS_TOOLS"); 		
	/* set_vodafone_common_configs("eth0", "ra0_0"); Not supported yet */
	/* remove once set_vodafone_common_configs is enabled: */
	token_set_y("CONFIG_RG_VODAFONE"); 
	token_set_y("CONFIG_RG_VODAFONE_CCF");
	token_set_y("CONFIG_RG_VODAFONE_CUSTOM_FW_RULES");

	set_vodafone_de_configs("eth0", "eth0", NULL, "ra01_0", NULL);
	if (IS_DIST("VOX_2.0_DE_VDSL"))
	{
	    token_set_y("CONFIG_RG_VODAFONE_DE_VDSL");
	    set_vodafone_de_vdsl_configs("dsl0", "atm0", "ptm0", "eth1");
	}

	if (IS_DIST("VOX_2.0_INT"))
        {
            token_set_y("CONFIG_RALINK_RT5592");
	    token_set_y("CONFIG_RG_VODAFONE_INT");
        }

	/* token_set_y("CONFIG_RG_VODAFONE_WL_RESCAN"); not supported yet */

	/* Vodafone manufacturing tests support */
	token_set_y("CONFIG_RG_VODAFONE_MT_SHELL");

	/* Using 8M of free space before UBI partition */
	token_set("CONFIG_RG_VODAFONE_MT_PARTITION_SIZE", "0x800000");
	token_set("CONFIG_RG_VODAFONE_MT_PARTITION_OFFSET", "0x3e40000");
	token_set("CONFIG_RG_VODAFONE_MT_PARTITION_NAME", "MANUFACTURING_TEST");
	token_set("CONFIG_RG_VODAFONE_MT_DIR_NAME", "/mnt/manufacturing_test");

	token_set_y("CONFIG_RG_PERM_STORAGE_UBI");
	/* Volume size for permst - OpenRG Flash layout (626 LEBs = ~77MB) */
	token_set("CONFIG_RG_PERM_STORAGE_UBI_VOLUME_SIZE", "0x4d07000");
	/* Partition size is at least the volumes' size, 4 PEBs for UBI and 80
	 * possible bab-blocks (worst case scenario).
	 * The user data space available on the UBI partition is:
	 *   S_user = (S_f / S_peb - 4 - MAX-BB) * S_leb =
	 *          = (0x1b6c0000 / 0x20000 - 4 - 80) * 0x1f800 =
	 *          = 0x1a58f000
	 *   S_user = Permst-volume + UBIFS-volume =
	 *          = 0x4d07000 + 0x15888000 = 0x1a58f000
	 */
	token_set("CONFIG_RG_UBI_PARTITION_SIZE", "0x1b6c0000");
	/* 1MiB for u-boot and it's data at the beginning of the flash */
	token_set("CONFIG_RG_UBI_PARTITION_OFFSET", "0x04640000");

	/* JFFS2 Support */
	token_set_m("CONFIG_JFFS2_FS");

	/* UBIFS Support */
	token_set_y("CONFIG_MTD_UBI");
	token_set_y("CONFIG_UBIFS_FS");
	token_set("CONFIG_RG_FFS_PARTITION_NAME", "FFS");
	
	/* Volume size for UBIFS filesystem (~345MB) */
	snprintf(ubi_vol, sizeof(ubi_vol), "0x15888000(%s)",
	    token_get_str("CONFIG_RG_FFS_PARTITION_NAME"));
	token_set("CONFIG_RG_UBIFS_UBI_VOLUMES_STR", ubi_vol);

	/* Mail Notifications Infrastructure */
	enable_module("MODULE_RG_MAIL_SERVER");
	token_set("CONFIG_RG_MAIL_SERVER_UW_IMAP", "n");
	token_set("CONFIG_RG_MAIL_SERVER_DISK_STORAGE", "n");
	token_set_y("CONFIG_RG_MAIL_SERVER_FFS_STORAGE");
	token_set("CONFIG_RG_MAIL_SERVER_FFS_STORAGE_PARTITION_NAME", 
	    token_get_str("CONFIG_RG_FFS_PARTITION_NAME"));

	/* 3G Support */
	token_set_y("CONFIG_RG_3G_WAN"); 
	token_set_y("CONFIG_RG_3G_VOICE");

	/* 3G CDC Ethernet over NCM */
	if (token_get("CONFIG_RG_3G_WAN"))
	{
	    token_set_m("CONFIG_HUAWEI_CDC_ETHER");
	    token_set_y("CONFIG_RG_HUAWEI_CDC_ETHER_NCM");
	    gsm_dev_add("cdc_ether0", DEV_IF_HUAWEI_CDC_ETHER, 1);
	    dev_set_route_group("cdc_ether0", DEV_ROUTE_GROUP_MAIN);
	}

        /* LTE Support */
	if (IS_DIST("VOX_2.0_DE") || IS_DIST("VOX_2.0_INT"))
	{
	    token_set_y("CONFIG_RG_LTE");
	    token_set_y("CONFIG_RG_LTE_FW");
	}

	if (IS_DIST("VOX_2.0_DE_VDSL"))
	{
	    token_set_y("CONFIG_HW_ETH_WAN");
	    token_set_y("CONFIG_HW_VDSL_WAN");
	    token_set_y("CONFIG_HW_DSL_WAN");
	    enable_module("MODULE_RG_DSL");
	    enable_module("MODULE_RG_SNMP");	
	    token_set_y("CONFIG_RG_UCD_SNMP_STATISTICS");
	    token_set_y("CONFIG_VF_WBM_SET_PASSWORD_CAPTCHA");
	}

	/* VOIP */
	enable_module("MODULE_RG_HOME_PBX");
	token_set_y("CONFIG_RG_VOIP_CALL_LOG");
	enable_module("MODULE_RG_VOIP_ASTERISK_SIP");
	enable_module("CONFIG_HW_DSP");
	token_set_y("CONFIG_HW_ISDN");
	token_set_y("CONFIG_RG_VOIP_ASTERISK_CAPI");
	token_set_y("CONFIG_NETBRICKS_ISDN_STACK");
	if (IS_DIST("VOX_2.0_DE_VDSL"))
	    token_set_y("CONFIG_RG_VODAFONE_EXT_VOICE_PORT");
	token_set_y("CONFIG_RG_PHONEBOOK_DB");

	if (IS_DIST("VOX_2.0_DE_VDSL"))
	    token_set_y("CONFIG_HW_FXO");

	token_set_y("CONFIG_RG_MTD_UTILS");
	token_set_y("CONFIG_RG_FFS");

	/* LAN Upgrade */
	token_set_y("CONFIG_RG_TFTP_UPGRADE");

	/* WBM */
	token_set_y("CONFIG_WBM_VODAFONE");
	token_set_y("CONFIG_RG_VODAFONE_3G_ACTIVATION");

	token_set_m("CONFIG_RG_KLOG");

	/* Twonky Media Server */
        token_set_y("CONFIG_RG_TWONKY");
	token_set("CONFIG_RG_TWONKY_FRIENDLYNAME", "EasyBox Media Server");
        token_set_y("CONFIG_INOTIFY");
        token_set_y("CONFIG_INOTIFY_USER");

	/* Printer Support */
	enable_module("MODULE_RG_PRINTSERVER");
	enable_module("CONFIG_RG_LPD");

        token_set_y("CONFIG_RG_HW_WATCHDOG");

	/* Not supported yet 
	token_set_y("CONFIG_RG_MULTIWAN"); */
	token_set_y("CONFIG_BOOTLDR_SIGNED_IMAGES");
	token_set_y("CONFIG_RG_EXTRA_MIME_TYPES");

	token_set_y("CONFIG_RG_PERM_STORAGE_INIT_OPTIMIZATION");

	/* Debugging tools */
#if 0	/* Enable those features only for development compilations */
	token_set_y("CONFIG_RG_DBG_ULIBC_MALLOC");
	/* Redirect malloc error messages from stdout to /dev/console */
	token_set_y("CONFIG_RG_DBG_MALLOC_2_CON");
	/* Crash task in case of found memory error */
	token_set_y("CONFIG_RG_DBG_ULIBC_MALLOC_CRASH");
	/* This feature fills freed memory with 0xeb value. If you found case of
	 * usage released memory - use is_mem_valid(ptr) for memory validation.
	 * Impairs board performance severely!!! */ 
	token_set("CONFIG_RG_DBG_ULIBC_MEM_POISON", "256");
#endif
	token_set_y("CONFIG_RG_SYSSTAT");

	token_set("RG_LAN_IP_SUBNET", "2");

	token_set_y("CONFIG_RG_VODAFONE_MANUFACTURE");

	/* U-Boot */
	if (token_get("CONFIG_LANTIQ_XWAY_UBOOT"))
	    token_set_y("CONFIG_BOOT_FROM_NAND");

	/* Customer version */
	token_set_y("CONFIG_RG_CUSTOMER_VERSION");

	token_set_y("CONFIG_RG_USER_FIRMWARE_UPGRADE");

	token_set("CONFIG_RG_IPV6_SET_DEFAULT_ULA", "n");

	token_set_y("CONFIG_RG_X509");

	/* CONFIG_RG_SYSLOG is defined later. */
	token_set_y("CONFIG_RG_LOG_PROJ");
    }
    else if (IS_DIST("VOX_2.5_DE_VDSL") || IS_DIST("VOX_2.5_DE_VDSL_26") ||
	IS_DIST("VOX_2.5_DE_VDSL_3X"))
    {
	if (IS_DIST("VOX_2.5_DE_VDSL_26"))
	{
	    os = "LINUX_26";
	    token_set("CONFIG_RG_JPKG_DIST",
		"JPKG_MIPSEB_BCM9636X_BSP_4_12_DE");
	}
	else
	{
	    os = "LINUX_3X";
	    token_set("CONFIG_RG_JPKG_DIST", "JPKG_MIPSEB_BCM9636X_3X");
	}
	hw = "BCM963168_VOX_2.5_DE";
	hw_compatible = "BCM963168_AUQ00X";

	token_set_y("CONFIG_RG_FLASH_LAYOUT_VF_DE_2_5");

	token_set_y("CONFIG_RG_VODAFONE_2_5");
	token_set("LIBC_IN_TOOLCHAIN", "n");

	token_set_y("CONFIG_RG_INITFS_RAMFS");

	token_set("CONFIG_RG_TELNETS", "n");

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_DSL");	
	token_set_y("CONFIG_HW_DSL_ANNEX_B");
	enable_module("MODULE_RG_PSE");
	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_MODULAR_UPGRADE");
	token_set_y("CONFIG_RG_DSLHOME_USB");
	token_set_y("CONFIG_RG_POWER_MANAGEMENT");
        token_set_y("CONFIG_RG_DISK_MNG");
	enable_module("MODULE_RG_REDUCE_SUPPORT");
	token_set_y("CONFIG_RG_USER_FIRMWARE_UPGRADE");
	token_set_y("CONFIG_RG_PSE_HW");

	token_set_y("CONFIG_RG_IPROUTE2_UTILS");

	enable_module("CONFIG_HW_USB_STORAGE");
	enable_module("CONFIG_HW_USB_HOST_EHCI");
	enable_module("CONFIG_HW_USB_HOST_OHCI");

	token_set_y("CONFIG_HW_ETH_LAN");
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_SWITCH_LAN");
	token_set_y("CONFIG_HW_DSL_WAN");
	token_set_y("CONFIG_HW_VDSL_WAN");

	token_set_m("CONFIG_HW_BUTTONS");
	token_set_y("CONFIG_HW_LEDS");

	/* WLAN */
	token_set_y("CONFIG_RG_DEV_IF_BCM432X_WLAN");
	enable_module("CONFIG_HW_80211N_BCM432X");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	token_set_y("CONFIG_RG_DUAL_CONCURRENT_WLAN");
	token_set_y("CONFIG_RG_VAP_GUEST");
	enable_module("CONFIG_RG_WPS");
	token_set_y("CONFIG_RG_WPS_PHYSICAL_PBC");
        token_set_y("CONFIG_RG_WPS_VIRTUAL_PBC");
        token_set_y("CONFIG_RG_WPS_VIRTUAL_DISPLAY_PIN");
        token_set_y("CONFIG_RG_WPS_KEYPAD");
        token_set_y("CONFIG_RG_WIRELESS_TOOLS");

	token_set_y("CONFIG_RG_VODAFONE");
	token_set_y("CONFIG_RG_VODAFONE_CCF");
	token_set_y("CONFIG_RG_VODAFONE_CCF_ENCRYPT");
	set_vodafone_de_configs("bcmsw", "br0", "bcmsw", "wl0", "wl1");
	token_set_y("CONFIG_RG_VODAFONE_DE_VDSL");
	token_set_y("CONFIG_RG_VF_VOX_2_5_DE_VDSL");
	if (IS_DIST("VOX_2.5_DE_VDSL") || IS_DIST("VOX_2.5_DE_VDSL_3X"))
	    token_set_y("CONFIG_RG_VF_VOX_2_5_DE_VDSL_3X");
	set_vodafone_de_vdsl_configs("bcm_dsl0", "bcm_atm0", "bcm_ptm0",
	    "eth0");
	token_set("RG_LAN_IP_SUBNET", "2");

	/* VOIP */
	enable_module("MODULE_RG_HOME_PBX");
	token_set_y("CONFIG_RG_VOIP_CALL_LOG");
	enable_module("MODULE_RG_VOIP_ASTERISK_SIP");
	enable_module("CONFIG_HW_DSP");
	token_set_y("BRCM_VRG_COUNTRY_CFG_GERMANY");

	/* LAN Upgrade */
	token_set_y("CONFIG_RG_TFTP_UPGRADE");

	token_set_y("CONFIG_RG_HW_WATCHDOG");

	token_set_y("CONFIG_RG_VODAFONE_MANUFACTURE");

	/* rg_conf obscured values are ecnrypted by AES key provided by
	 * manufacturer */
	token_set_y("CONFIG_RG_RGCONF_CRYPTO_AES");
	token_set_y("CONFIG_RG_RGCONF_CRYPTO_AES_VENDOR_KEY");

	/* WBM */
	token_set_y("CONFIG_WBM_VODAFONE");

	token_set_m("CONFIG_RG_KLOG");

	token_set_y("CONFIG_RG_EXTRA_MIME_TYPES");

	if (!token_get("CONFIG_RG_BSP_UPGRADE"))
	    token_set_y("CONFIG_RG_MAINFS_IN_UBI");
	token_set_y("CONFIG_RG_PERM_STORAGE_UBI");
	token_set_y("CONFIG_UBIFS_FS");
	token_set_y("CONFIG_RG_MTD_UTILS");
	token_set_y("CONFIG_RG_FFS");
	token_set_y("CONFIG_BOOTLDR_SIGNED_IMAGES");
	token_set_y("CONFIG_RG_PERM_STORAGE_INIT_OPTIMIZATION");
	token_set_y("CONFIG_RG_MODULES_INITRAMFS");
	token_set_y("CONFIG_RG_LIBCAPTCHA");
	token_set("CONFIG_RG_FFS_PARTITION_NAME", "FFS");
	/* Auto-sensing doesn't work and it is disabled until Broadcom provides
	 * the solution - B185863 */
	/* token_set_y("CONFIG_RG_VODAFONE_DSL_ANNEX_AUTOSENSING"); */
	token_set_y("CONFIG_RG_UI_BRUTE_FORCE_PROTECTION");

	/* Customer version */
	token_set_y("CONFIG_RG_CUSTOMER_VERSION");

	token_set_y("CONFIG_RG_LOG_PROJ");
	token_set_y("CONFIG_RG_PROXIMITY_SENSOR");
	token_set_y("CONFIG_VF_FFS_CALDATA");

	/* JFFS2 Support */
	token_set_m("CONFIG_JFFS2_FS");

	token_set("CONFIG_RG_HTTP_XFRAME_PROTECTION", "SAMEORIGIN");

	token_set_y("CONFIG_SMP");

	token_set_y("CONFIG_RG_X509");

        token_set("CONFIG_RG_ETH_WAN_DETECT", "n");
	token_set_y("CONFIG_RG_VODAFONE_CGI_POST_ONLY");

	token_set_y("CONFIG_PACKET_MMAP");

	/* common LED features */
	token_set_y("CONFIG_RG_LEDS_BOTTOM_INTENSITY");
	token_set_y("CONFIG_RG_LEDS_PROXIMITY_OVERRIDE");

	/* VSAF */
	token_set_y("CONFIG_RG_VSAF");
	token_set("CONFIG_RG_VSAF_RUNNING_DIR", "/mnt/ffs/ubifs/vsaf");
	token_set_y("CONFIG_RG_VSAF_WBM");

	token_set_y("CONFIG_RG_EXTERNAL_API");
	token_set_y("CONFIG_RG_EXTERNAL_CLIENT");
    }
    else if (IS_DIST("VOX_2.5_DE_VDSL_IN_MAKING"))
    {
	/* XXX These configs should be disabled in command line:
	 *   CONFIG_RG_IGMP_PROXY=n CONFIG_AUQ00X_OBSOLETE_LED_MCU_FW=n
	 */
	os = "LINUX_26";
	hw = "BCM963168_VOX_2.5_DE";

	token_set_y("CONFIG_RG_FLASH_LAYOUT_VF_DE_2_5");

	token_set_y("CONFIG_RG_VODAFONE_SMALL_SET");

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_PSE");
	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_IPV6");
	enable_module("MODULE_RG_VLAN");

	/* WLAN */
	token_set_y("CONFIG_RG_DEV_IF_BCM432X_WLAN");
	enable_module("CONFIG_HW_80211N_BCM432X");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	token_set_y("CONFIG_RG_DUAL_CONCURRENT_WLAN");
	token_set_y("CONFIG_RG_VAP_GUEST");
	enable_module("CONFIG_RG_WPS");
	token_set_y("CONFIG_RG_WPS_PHYSICAL_PBC");
        token_set_y("CONFIG_RG_WPS_VIRTUAL_PBC");
        token_set_y("CONFIG_RG_WPS_VIRTUAL_DISPLAY_PIN");
        token_set_y("CONFIG_RG_WPS_KEYPAD");
        token_set_y("CONFIG_RG_WIRELESS_TOOLS");

	token_set_y("CONFIG_RG_VODAFONE");
	token_set_y("CONFIG_RG_VODAFONE_2_5");
	token_set_y("CONFIG_RG_VF_VOX_2_5_DE_VDSL");
	set_vodafone_de_configs("bcmsw", "br0", "bcmsw", "wl0", "wl1");	
	dev_add_bridge("br0", DEV_IF_NET_INT, "bcmsw", NULL);
	set_vodafone_de_vdsl_configs_short("bcm_dsl0", "bcm_atm0", "bcm_ptm0",
	    "eth0");
	token_set("RG_LAN_IP_SUBNET", "2");

	token_set_m("CONFIG_HW_BUTTONS");
	token_set_y("CONFIG_HW_LEDS");

	/* WBM */
	token_set_y("CONFIG_WBM_VODAFONE");

	token_set_y("CONFIG_RG_IPROUTE2_UTILS");

	token_set_y("CONFIG_HW_ETH_LAN");
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_SWITCH_LAN");
	token_set("CONFIG_BCM9636X_SUPPORT_SWMDK", "n");	

	token_set("LIBC_IN_TOOLCHAIN", "n");

	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MODULES_INITRAMFS");

	token_set_y("CONFIG_RG_MAINFS_IN_UBI");
	token_set_y("CONFIG_RG_PERM_STORAGE_UBI");
	token_set_y("CONFIG_UBIFS_FS");
	token_set_y("CONFIG_RG_MTD_UTILS");
	token_set_y("CONFIG_RG_FFS");
	token_set_y("CONFIG_RG_PERM_STORAGE_INIT_OPTIMIZATION");
	token_set_m("CONFIG_JFFS2_FS");
	token_set("CONFIG_RG_FFS_PARTITION_NAME", "FFS");
    }
    else if (IS_DIST("VOX_2.5_DE_VDSL_SKIM"))
    {
	/* XXX These configs should be disabled in command line:
	 *   CONFIG_RG_IGMP_PROXY=n CONFIG_RG_PPPOE_RELAY=n CONFIG_RG_STP=n
	 */
	os = "LINUX_3X";
	hw = "BCM963168_VOX_2.5_DE";

	token_set_y("CONFIG_RG_FLASH_LAYOUT_VF_DE_2_5");

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_PSE");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_IPV6");

	/* merge B182514 to enable iproute2
	token_set_y("CONFIG_RG_IPROUTE2_UTILS");
	*/

	token_set_y("CONFIG_HW_ETH_LAN");
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_SWITCH_LAN");
	token_set("CONFIG_BCM9636X_SUPPORT_SWMDK", "n");	

	dev_add_bridge("br0", DEV_IF_NET_INT, "bcmsw", NULL);

	token_set("LIBC_IN_TOOLCHAIN", "n");

	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MODULES_INITRAMFS");

	token_set_y("CONFIG_RG_MAINFS_IN_UBI");
	token_set_y("CONFIG_RG_PERM_STORAGE_UBI");
	token_set_y("CONFIG_UBIFS_FS");
	token_set_y("CONFIG_RG_MTD_UTILS");
	token_set_y("CONFIG_RG_FFS");
	token_set_y("CONFIG_RG_PERM_STORAGE_INIT_OPTIMIZATION");
	token_set_m("CONFIG_JFFS2_FS");
	token_set("CONFIG_RG_FFS_PARTITION_NAME", "FFS");

	token_set_y("CONFIG_HW_BUTTONS");
	token_set_y("CONFIG_RG_PROXIMITY_SENSOR");

	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_DSL");
	enable_module("MODULE_RG_VLAN");
	token_set_y("CONFIG_HW_DSL_WAN");
	token_set_y("CONFIG_HW_VDSL_WAN");

	set_vodafone_de_vdsl_configs_short("bcm_dsl0", "bcm_atm0", "bcm_ptm0",
	    "eth0");
    }    
    else
	conf_err("invalid DIST=%s\n", dist);

    if (hw && strcmp(hw, "JPKG") && !(*os))
	os = "LINUX_24";

    token_set("CONFIG_RG_DIST", dist);

    /* Event loop method: We use 'poll' by default since we have some linux-2.4
     * platforms that doesn't support epoll */
    if (!token_get("CONFIG_TARGET_EVENT_EPOLL") &&
	!token_get("CONFIG_TARGET_EVENT_SELECT") &&
	!token_get("CONFIG_TARGET_EVENT_POLL"))
    {
	token_set_y("CONFIG_TARGET_EVENT_POLL");
    }
    /* Currently /usr/local/openrg/i386 doesn't support epoll, and will not
     * support it as long as we support compiling RG on kernel-2.4 Hosts */
    if (!token_get("CONFIG_LOCAL_EVENT_EPOLL") &&
	!token_get("CONFIG_LOCAL_EVENT_POLL") &&
	!token_get("CONFIG_LOCAL_EVENT_SELECT"))
    {
	token_set_y("CONFIG_LOCAL_EVENT_POLL");
    }

    if (token_get("CONFIG_HW_DSL_WAN") && token_get("CONFIG_HW_ETH_WAN"))
	token_set_y("CONFIG_RG_MULTIWAN");

    if (token_get("CONFIG_RG_INSTALLTIME_WPA") &&
	!token_get("CONFIG_RG_VAP_INSTALLTIME"))
    {
	conf_err("Can't set CONFIG_RG_INSTALLTIME_WPA without "
	    "CONFIG_RG_VAP_INSTALLTIME enabled\n");
    }
}
