/****************************************************************************
 *
 * rg/pkg/build/hw_config.c
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
#include <stdarg.h>
#include <string.h>
#include "config_opt.h"
#include "create_config.h"
#include <str.h>

#ifdef CONFIG_RG_DO_DEVICES
static int wireless_vaps_add(char *wl_name, dev_if_type_t wl_type,
    char *vap_name_fmt, int start_idx, wl_net_type_t *vap_order);
static void ralink_rt2xxx_add(dev_if_type_t type, char *wl_name, int start_idx,
    char *config);
static void wireless_phy_dev_add(dev_if_type_t type, char *wl_name,
    wl_net_type_t net_type,int is_enabled, char *br_name);
static void wireless_vap_add(dev_if_type_t type, char *wl_name, char *vap_name,
    wl_net_type_t vap_net_type, int is_enabled, char *br_name);
#else
#define wireless_vaps_add(...)
#define ralink_rt2xxx_add(...)
#define wireless_phy_dev_add(...)
#define wireless_vap_add(...)
#endif

#ifdef CONFIG_RG_DO_DEVICES
typedef struct wl_net_type_properties_t {
    wl_net_type_t wl_net_type;
    char *vap_config;
    int is_scr;
    int is_web_auth;
    char *depend_on_config;
    int enabled_by_default;
    char *br_name;
} wl_net_type_properties_t;

static wl_net_type_properties_t wl_net_types_def[] = {
#if 0
    {WL_NET_TYPE_INSTALLTIME, "CONFIG_RG_VAP_INSTALLTIME", 1, 1, NULL, 1, 1},
    {WL_NET_TYPE_HOME, "CONFIG_RG_VAP_HOME", 0, 1, NULL, 0, 1},
#endif
    {WL_NET_TYPE_HOME_SECURED, "CONFIG_RG_VAP_SECURED", 0, 0, NULL, 0, "br0"},
#if 0
    /* Helpline is created enabled in rg_conf. The actual state of the device
     * is not Senabled in rg_conf, but rather Svolatile_enabled in rg_conf_ram.
     * When VAP is created, Svolatile_enabled is set to 0 and then it is
     * set to 1 when JRMS status becomes GOOD. */
    {WL_NET_TYPE_HELPLINE, "CONFIG_RG_VAP_HELPLINE", 1, 0, "CONFIG_RG_HELPLINE",
	1, 0},
#endif
    {WL_NET_TYPE_GUEST, "CONFIG_RG_VAP_GUEST", 0, 0, NULL, 0, NULL},
    {WL_NET_TYPE_FON, "CONFIG_RG_FON", 0, 0, NULL, 0, 0},
    {WL_NET_TYPE_FON_SECURED, "CONFIG_RG_FON", 0, 0, NULL, 0, 0},
    {-1},
};

static wl_net_type_properties_t wl_net_types_vf_de[] = {
    {WL_NET_TYPE_HOME_SECURED, "CONFIG_RG_VAP_SECURED", 0, 0, NULL, 0, NULL},
    {WL_NET_TYPE_GUEST, "CONFIG_RG_VAP_GUEST", 0, 0, NULL, 0, "br2"},
    {-1},
};

static wl_net_type_properties_t *wl_net_type_properties = wl_net_types_def;

wl_net_type_t default_vap_order[] = {
    WL_NET_TYPE_INSTALLTIME,
    WL_NET_TYPE_HOME,
    WL_NET_TYPE_HOME_SECURED,
    WL_NET_TYPE_HELPLINE,
    -1
};

wl_net_type_t vodafone_vap_order[] = {
    WL_NET_TYPE_HOME_SECURED,
    WL_NET_TYPE_GUEST,
    WL_NET_TYPE_FON,
    WL_NET_TYPE_FON_SECURED,
    -1
};

wl_net_type_t first_vap_secured_order[] = {
    WL_NET_TYPE_HOME_SECURED,
    WL_NET_TYPE_INSTALLTIME,
    WL_NET_TYPE_HOME,
    WL_NET_TYPE_HELPLINE,
    -1
};

static wl_net_type_properties_t *wl_net_type_properties_get(
    wl_net_type_t wl_net_type)
{
    wl_net_type_properties_t *wl;

    for (wl=wl_net_type_properties; wl->wl_net_type !=-1 && 
	wl->wl_net_type!=wl_net_type; wl++);
    if (wl->wl_net_type==-1)
        conf_err("Unknown wl_net_type %d!", wl_net_type);
    return wl;
}

void wireless_features(void)
{
    wl_net_type_properties_t *wl;

    if ((IS_HW("ADA00X") || IS_HW("GRX288_VOX_2.0_DE") ||
	IS_HW("VRX288_VOX_2.0_DE")) && token_get("CONFIG_RG_VODAFONE_DE"))
    {
	wl_net_type_properties = wl_net_types_vf_de;
    }

    /* Add default vaps if none were specified */
    if (token_get("MODULE_RG_WLAN_AND_ADVANCED_WLAN") &&
	!token_get("CONFIG_RG_VAP_HOME") &&
	!token_get("CONFIG_RG_VAP_SECURED") &&
	!token_get("CONFIG_RG_VAP_HELPLINE") &&
	!token_get("CONFIG_RG_VAP_INSTALLTIME"))
    {
	if (token_get("MODULE_RG_REDUCE_SUPPORT"))
	{
	    token_set_y("CONFIG_RG_VAP_INSTALLTIME");
	    if (token_get("CONFIG_RG_JNET_CLIENT"))
		token_set_y("CONFIG_RG_VAP_HELPLINE");
	}
	token_set_y("CONFIG_RG_VAP_HOME"); /* Web-auth */
	token_set_y("CONFIG_RG_VAP_SECURED"); /* WPA */
    }

    if (!token_get("MODULE_RG_WLAN_AND_ADVANCED_WLAN"))
        return;

    for (wl=wl_net_type_properties; wl->wl_net_type !=-1; wl++)
    {
	if (!token_get(wl->vap_config) && !token_get("CONFIG_RG_JPKG"))
	    continue;

	if (wl->depend_on_config)
	    token_set_y(wl->depend_on_config);
	if (wl->is_web_auth)
	    token_set_y("CONFIG_RG_WEB_AUTH");
    }
}
#endif

static void mtd_physmap_add(long start, long len, int width)
{
    token_set("CONFIG_MTD_PHYSMAP_START", "0x%lx", start);
    token_set("CONFIG_MTD_PHYSMAP_LEN", "0x%lx", len);

    /* TODO: We need to fix this genericaly for NOR flash etc. */
    if (!token_get("CONFIG_MTD_COMCERTO_NOR"))
	token_set_y("CONFIG_MTD_PHYSMAP");
    token_set_y("CONFIG_MTD_CMDLINE_PARTS");

    if (token_get("CONFIG_MTD_PHYSMAP"))
	token_set("CONFIG_MTD_PHYSMAP_BANKWIDTH", "%d", width);
}

#ifdef CONFIG_RG_DO_DEVICES
static dev_if_type_t config2type(code2str_t *list)
{
    for (; list->code != -1; list++)
    {
	if (token_get(list->str))
	    return list->code;
    }
    conf_err("Hardware (vap type) is not specified");

    /* not reached */
    return -1;
}

static dev_if_type_t get_vap_type(void)
{
    code2str_t vap_types[] = {
	{ DEV_IF_AR5212_VAP_SLAVE, "CONFIG_RG_ATHEROS_HW_AR5212" },
	{ DEV_IF_AR5416_VAP_SLAVE, "CONFIG_RG_ATHEROS_HW_AR5416" },
	{ DEV_IF_RT2561_VAP, "CONFIG_HW_80211G_RALINK_RT2561" },
	{ DEV_IF_RT2860_VAP, "CONFIG_HW_80211N_RALINK_RT2860" },
	{ DEV_IF_RT2860_VAP, "CONFIG_HW_80211N_RALINK_RT3062" },
	{ DEV_IF_RT2880_VAP, "CONFIG_HW_80211N_RALINK_RT2880" },
	{ DEV_IF_RT2880_VAP, "CONFIG_HW_80211N_RALINK_RT3052" },
	{ DEV_IF_RT3883_VAP, "CONFIG_HW_80211N_RALINK_RT3883" },
	{ DEV_IF_BCM43XX_VAP, "CONFIG_HW_80211G_BCM43XX" },
	{ DEV_IF_BCM9636X_WLAN_VAP, "CONFIG_HW_BCM9636X_WLAN" },
	{ DEV_IF_BCM432X_WLAN_VAP, "CONFIG_HW_80211N_BCM432X" },
	{ DEV_IF_BCM432X_WLAN_VAP_5GHZ, "CONFIG_HW_80211N_BCM432X" },
	{ DEV_IF_UML_WLAN_VAP, "CONFIG_HW_80211G_UML_WLAN" },
	{ -1, NULL }
    };

    return config2type(vap_types);
}

static void wl_net_type_set(char *wl_name, wl_net_type_t wl_net_type)
{
    wl_net_type_properties_t *wl;

    /* Do not set type in conf if no SCR or any net type defined */
    if (token_get("MODULE_RG_REDUCE_SUPPORT") ||
	token_get("CONFIG_RG_VAP_INSTALLTIME") ||
	token_get("CONFIG_RG_VAP_HOME") ||
	token_get("CONFIG_RG_VAP_SECURED") ||
	token_get("CONFIG_RG_VAP_HELPLINE") ||
	token_get("CONFIG_RG_VAP_GUEST"))
    {
	dev_wl_net_type(wl_name, wl_net_type);
    }

    wl = wl_net_type_properties_get(wl_net_type);
    if (wl->is_web_auth && !(wl_net_type == WL_NET_TYPE_INSTALLTIME &&
	token_get("CONFIG_RG_INSTALLTIME_WPA")))
    {
	dev_set_web_auth(wl_name);
    }
}

static void wireless_dev_add(dev_if_type_t type, char *wl_name,
    wl_net_type_t net_type, int is_enabled, char *br_name)
{
    if (token_get("CONFIG_RG_JPKG"))
    {
	token_set_dev_type(type);
	return;
    }

    if (is_enabled)
	dev_add(wl_name, type, DEV_IF_NET_INT);
    else
	dev_add_disabled(wl_name, type, DEV_IF_NET_INT);

    if (br_name)
	dev_add_to_bridge(br_name, wl_name);
    if (net_type != -1)
	wl_net_type_set(wl_name, net_type);
}

static void wireless_phy_dev_add(dev_if_type_t type, char *wl_name,
    wl_net_type_t net_type,int is_enabled, char *br_name)
{
    wireless_dev_add(type, wl_name, net_type, is_enabled, br_name);
    dev_can_be_missing(wl_name);
}

static void wireless_vap_add(dev_if_type_t type, char *wl_name, char *vap_name,
    wl_net_type_t vap_net_type, int is_enabled, char *br_name)
{
    wireless_dev_add(type, vap_name, vap_net_type, is_enabled, br_name);
    dev_set_dependency(vap_name, wl_name);
}

/* Add wireless devices and return the number of added vaps */
static int wireless_vaps_add(char *wl_name, dev_if_type_t wl_type,
    char *vap_name_fmt, int start_idx, wl_net_type_t *vap_order)
{
    char *name = NULL;
    char *br_name;
    wl_net_type_properties_t *wl;
    wl_net_type_t *v;
    int is_first = 1;
    int ret = 0;

    if (!vap_order)
        vap_order = vodafone_vap_order;

    for (v=vap_order; *v!=-1; v++)
    {
        wl = wl_net_type_properties_get(*v);

	/* allow JPKG dists to enable dev types (CONFIG_HW_80211G_xxx) without
	 * specifying vap configs */
	if (!token_get(wl->vap_config) && !token_get("CONFIG_RG_JPKG"))
	    continue;

	if (wl->is_scr && !token_get("MODULE_RG_REDUCE_SUPPORT"))
	    continue;

	br_name = token_get("CONFIG_DEF_BRIDGE_LANS") ? wl->br_name : NULL;

	if (is_first)
	{
	    /* First VAP */
	    wireless_phy_dev_add(wl_type, wl_name, *v, wl->enabled_by_default,
		br_name);
	    is_first = 0;
	}
	else
	{
	    if (!vap_name_fmt)
	        continue;

	    wireless_vap_add(get_vap_type(), wl_name,
		*str_printf(&name, vap_name_fmt, start_idx),
		*v, wl->enabled_by_default, br_name);
	    start_idx++;
	}
	ret++;
    }

    str_free(&name);
    return ret;
}

static void ralink_rt2xxx_add(dev_if_type_t type, char *wl_name, int start_idx,
    char *config)
{
    token_set_m(config); 
    wireless_vaps_add(wl_name, type, "ra%d", start_idx, NULL);
}
#endif

void airgo_agn100_add(void)
{
    wireless_vaps_add("wlan0", DEV_IF_AGN100, NULL, 0, NULL);
}

static void ralink_rt2560_add(char *wl_name)
{
#ifdef CONFIG_RG_DO_DEVICES
    token_set_m("CONFIG_RALINK_RT2560");
#endif
    wireless_vaps_add(wl_name, DEV_IF_RT2560, NULL, 0, NULL);
}

static void ralink_rt2561_add(char *wl_name)
{
    ralink_rt2xxx_add(DEV_IF_RT2561, wl_name, 8, "CONFIG_RALINK_RT2561");
}

static void ralink_rt2860_add(char *wl_name)
{
    ralink_rt2xxx_add(DEV_IF_RT2860, wl_name, 1, "CONFIG_RALINK_RT2860");
}

static void ralink_rt2880_add(char *wl_name)
{
#ifdef CONFIG_RG_DO_DEVICES
    int i, vap_count;
    char *dev = NULL;

    token_set_m("CONFIG_RALINK_RT2880"); 
    vap_count = wireless_vaps_add(wl_name, DEV_IF_RT2880, "ra%d", 1, 
        first_vap_secured_order);
    /* Set Ralink devices separation state (by ap_vlan_id), excluding
     * the first device (ra0) - it must be already non-separated.
     */
    for (i=1; i<vap_count; i++)
    {
	str_printf(&dev, "ra%d", i);
	dev_set_ap_vlan_id(dev, i+1);
    }
    str_free(&dev);
#endif
}

static void ralink_rt3883_add(char *name_format, int is_5g)
{
#ifdef CONFIG_RG_DO_DEVICES
    int i, vap_count;
    char *dev = NULL;
    wl_band_t band = is_5g ? WL_BAND_5G : WL_BAND_2G;

    token_set_m("CONFIG_RALINK_RT3883"); 
    str_printf(&dev, name_format, 0);
    vap_count = wireless_vaps_add(dev, DEV_IF_RT3883, name_format, 1,
	vodafone_vap_order);
    dev_set_wl_band(dev, band);
    /* Set Ralink devices separation state (by ap_vlan_id), excluding
     * the first device (ra0) - it must be already non-separated.
     */
    for (i=1; i<vap_count; i++)
    {
	str_printf(&dev, name_format, i);
	dev_set_wl_band(dev, band);
	dev_set_ap_vlan_id(dev,
	    i + 1 + (is_5g ? token_get("CONFIG_RALINK_DEV_ID_OFFSET") : 0));
    }
    str_free(&dev);
#endif
}

static void bcm43xx_add(char *wl_name)
{
    wireless_vaps_add(wl_name, DEV_IF_BCM43XX, "wl0.%d", 1, NULL);
}

static void bcm9636x_add(char *wl_name)
{
    wireless_vaps_add(wl_name, DEV_IF_BCM9636X_WLAN, "wl0.%d", 1, NULL);
}

void isl_softmac_add(void)
{
    dev_add("eth0", DEV_IF_ISL_SOFTMAC, DEV_IF_NET_INT);
    dev_can_be_missing("eth0");
}

static void atheros_ar5xxx_add(void)
{
#ifdef CONFIG_RG_DO_DEVICES
    code2str_t ath_first_vap_types[] = {
	{ DEV_IF_AR5212_VAP, "CONFIG_RG_ATHEROS_HW_AR5212" },
	{ DEV_IF_AR5416_VAP, "CONFIG_RG_ATHEROS_HW_AR5416" },
	{ -1, NULL }
    };
#endif

    /* 'wifi0' is not an access point */
    wireless_phy_dev_add(DEV_IF_WIFI, "wifi0", -1, 1, NULL);

    wireless_vaps_add("ath0", config2type(ath_first_vap_types),
	"ath%d", 1, NULL);
    dev_set_dependency("ath0", "wifi0");
}

static void lcd_configure(void)
{
    token_set_y("CONFIG_FB");
    token_set_y("CONFIG_FB_CFB_FILLRECT");
    token_set_y("CONFIG_FB_CFB_COPYAREA");
    token_set_y("CONFIG_FB_CFB_IMAGEBLIT");
    token_set_y("CONFIG_FB_BACKLIGHT");
    token_set_y("CONFIG_BACKLIGHT_CLASS_DEVICE");
    token_set_y("CONFIG_LCD_CLASS_DEVICE");

    if (IS_HW("YWZ00B") || IS_HW("BCM96362_VOX_1.5_IT") || IS_HW("ADA00X") ||
	IS_HW("BCM96362_VOX_1.5_NZ"))
    {
	token_set_y("CONFIG_FB_FPGA_LCD");
    }

    if (IS_HW("YWZ00B") || IS_HW("BCM96362_VOX_1.5_IT") ||
	IS_HW("BCM96362_VOX_1.5_NZ"))
    {
        token_set_y("CONFIG_TRULY_LCD");
    }

    if (IS_HW("HG558BZA") || IS_HW("BCM96362_VOX_1.5_IT"))
	token_set_y("CONFIG_HIMAX_HUAWEI_LCD");

    if (token_get("CONFIG_HIMAX_HUAWEI_LCD") ||
	token_get("CONFIG_TRULY_LCD"))
    {
	token_set_y("CONFIG_FB_HIMAX_LCD");
    }

    if (IS_HW("GRX288_VOX_2.0_DE") || IS_HW("VRX288_VOX_2.0_DE"))
    {
	if (token_get("CONFIG_HW_A000J") || token_get("CONFIG_HW_C000J"))
	{
	    token_set_y("CONFIG_FB_ILI9325_LCD");
	    token_set_y("CONFIG_FB_HX8347_LCD");
	    token_set_y("CONFIG_FB_ILI9341_LCD");
	}
	if (token_get("CONFIG_HW_ADA00X") || token_get("CONFIG_HW_AD900X"))
	    token_set_y("CONFIG_FB_FPGA_LCD");
    }
}

static void touch_buttons_configure(void)
{
    token_set_y("CONFIG_INPUT");
    token_set_y("CONFIG_INPUT_EVDEV");

    if (IS_HW("YWZ00B") || IS_HW("BCM96362_VOX_1.5_IT") ||
	IS_HW("BCM96362_VOX_1.5_NZ"))
    {
	token_set_y("CONFIG_SX8646");
    }
    if (IS_HW("HG558BZA") || IS_HW("BCM96362_VOX_1.5_IT"))
	token_set_y("CONFIG_SO380000");

    if (IS_HW("GRX288_VOX_2.0_DE") || IS_HW("VRX288_VOX_2.0_DE"))
    {
	token_set_y("CONFIG_I2C");
	token_set_y("CONFIG_I2C_ALGOBIT");
	token_set_y("CONFIG_I2C_GPIO");
	if (token_get("CONFIG_HW_A000J") || token_get("CONFIG_HW_C000J"))
	    token_set_y("CONFIG_HW_MA86P03");
	if (token_get("CONFIG_HW_ADA00X") || token_get("CONFIG_HW_AD900X"))
	    token_set_y("CONFIG_HW_CAP1166");
    }
}

void hardware_features(void)
{
    option_t *hw_tok;

    if (!hw)
    {
	token_set("CONFIG_RG_HW", "NO_HW");
	token_set("CONFIG_RG_HW_DESC_STR", "No hardware - local targets only");
	token_set_y("CONFIG_RG_BUILD_LOCAL_TARGETS_ONLY");
	return;
    }

    hw_tok = option_token_get(openrg_hw_options, hw);

    if (!hw_tok->value)
	conf_err("No description available for HW=%s\n", hw);

    token_set("CONFIG_RG_HW", hw);
    token_set("CONFIG_RG_HW_DESC_STR", hw_tok->value);

    if (!hw_compatible)
	token_set("CONFIG_RG_HW_HEADERS", hw);
    else
    {
	char *headers = NULL;

	str_printf(&headers, "%s %s", hw, hw_compatible);
        token_set("CONFIG_RG_HW_HEADERS", headers);
	str_free(&headers);
    }

    if (IS_HW("ARX188"))
    {
	token_set("BOARD", "ARX188");
	token_set("FIRM", "Lantiq");

	if (token_get("CONFIG_HW_DSP"))
	{
	    token_set_m("CONFIG_IFX_OS");
	    token_set_m("CONFIG_IFX_TAPI");
	    token_set_m("CONFIG_IFX_VMMC");

	    token_set("CONFIG_HW_NUMBER_OF_FXS_PORTS", "2");
	    token_set_m("CONFIG_RG_XWAY_JDSP");

	    if (token_get("CONFIG_HW_ISDN"))
	    {
		token_set_y("CONFIG_LANTIQ_IPACX");
		token_set("CONFIG_HW_NUMBER_OF_ISDN_NT_LINES", "1");
	    }
	    token_set_y("CONFIG_XRX");
	    token_set_y("CONFIG_IFX_VOICE_CPE_TAPI_FAX");
	}

	token_set_y("CONFIG_AR9");
	token_set_y("CONFIG_AR9_REF_BOARD");

#if 0
	token_set_y("CONFIG_BOOTLDR_UBOOT");
	token_set("CONFIG_BOOTLDR_UBOOT_COMP", "lzma");
#endif

	token_set("CONFIG_SDRAM_SIZE", "32");

	token_set("CONFIG_MTD_PHYSMAP_START", "0x10000000");
	token_set("CONFIG_MTD_PHYSMAP_LEN", "0x00800000");
	token_set("CONFIG_MTD_IFX_NOR_FLASH_SIZE", "8");
#if 0
	token_set("CONFIG_RG_FLASH_LAYOUT_SIZE", "8");
#endif
	token_set("CONFIG_RG_CONSOLE_DEVICE", "ttyS1");
	token_set("CONFIG_CMDLINE", "console=ttyS1,115200");

	if (token_get("CONFIG_RG_PSE_HW"))
	{
	    token_set_y("CONFIG_IFX_PPA");
	    token_set("CONFIG_RG_PSE_HW_PLAT_PATH", 
		"vendor/lantiq/xway/rg/modules");
	}
	
	if (token_get("CONFIG_IFX_PPA"))
	{
	    token_set_y("CONFIG_IFX_PPA_A5");
	    token_set_y("CONFIG_IFX_PPA_DATAPATH");
	    /* AR9-A5 datapath driver demands ATM */
	    token_set_y("CONFIG_ATM");
	}
	else 
	{
	    if (token_get("CONFIG_HW_ETH_LAN") || 
		token_get("CONFIG_HW_ETH_WAN"))
	    {
		/* Note:
		 * CONFIG_IFX_3PORT_SWITCH is the driver for the AR9 internal 
		 * MAC. It MUST NOT be used concurrently with the AR9 datapath 
		 * driver (CONFIG_IFX_PPA_DATAPATH).
		 */
		token_set_y("CONFIG_IFX_ETH_FRAMEWORK");
		token_set_y("CONFIG_IFX_3PORT_SWITCH");
		if (token_get("CONFIG_HW_ETH_WAN"))
		    token_set_y("CONFIG_SW_ROUTING_MODE");
	    }
	    if (token_get("CONFIG_HW_DSL_WAN"))
	    {
		/* Note:
		 * CONFIG_IFX_ATM is the driver for the A1 ATM firmware.
		 * It MUST NOT be used concurrently with the AR9 datapath 
		 * driver (CONFIG_IFX_PPA_DATAPATH).
		 */
		token_set_y("CONFIG_IFX_ATM");
	    }
	}
	if (!token_get("CONFIG_LSP_DIST"))
	{
	    if (token_get("CONFIG_HW_ETH_LAN"))
            {
		dev_add("eth0", DEV_IF_PSB6973_HW_SWITCH, DEV_IF_NET_INT);
                token_set_y("CONFIG_PSB6973_HAVE_EXTERNAL_PHY");
            }
	    if (token_get("CONFIG_HW_ETH_WAN"))
		dev_add("eth1", DEV_IF_LANTIQ_AR9_ETH, DEV_IF_NET_EXT);
            if (token_get("CONFIG_HW_DSL_WAN"))
            {
                token_set_y("CONFIG_IFX_ATM_OAM");
                token_set_y("CONFIG_RG_ATM_QOS");
		dev_add("dsl0", DEV_IF_LANTIQ_XWAY_DSL, DEV_IF_NET_EXT);
		dsl_atm_dev_add("atm0", DEV_IF_LANTIQ_XWAY_ATM, "dsl0");

		/* xDSL daemon application requires threads support. */
		token_set_y("CONFIG_RG_LIBC_CUSTOM_STREAMS");
            }
	}
	
        if (token_get("CONFIG_RG_3G_WAN"))
            gsm_dev_add("gsm0", DEV_IF_3G_USB_SERIAL, 1);

	if (token_get("CONFIG_HW_USB_STORAGE"))
	    token_set_y("CONFIG_USB");

	if (token_get("CONFIG_RG_ATHEROS_HW_AR5416"))
	{
	    token_set_m("CONFIG_RG_ATHEROS");
	    token_set("CONFIG_ATHEROS_AR5008_PCI_SWAP", "1");
	    atheros_ar5xxx_add();

	    /* TODO: Check if we need it... */
	    token_set_y("CONFIG_RG_WSEC_DAEMON");
	}

	token_set_y("CONFIG_PCI");
	if (token_get("CONFIG_HW_ENCRYPTION"))
	{
	    token_set_y("CONFIG_CRYPTO_DEV_DEU");
	    token_set_y("CONFIG_CRYPTO_DEV_AR9");
	}
    }

    if (IS_HW("VRX288") || IS_HW("GRX288") || IS_HW("ADA00X") || 
	IS_HW("GRX288_VOX_2.0_DE") || IS_HW("VRX288_VOX_2.0_DE") ||
	IS_HW("GRX288_VOX_2.0_INT"))
    {
	if (IS_HW("ADA00X"))
	{
	    token_set("BOARD", "ADA00X");
	    token_set("FIRM", "Sercomm");
            token_set_y("CONFIG_HW_ADA00X");
	}
	else if (IS_HW("GRX288_VOX_2.0_DE"))
	{
	    token_set("BOARD", "Vox2.0 DE");
	    token_set("FIRM", "Vodafone");
            token_set_y("CONFIG_HW_ADA00X");
	    token_set_y("CONFIG_HW_A000J");
	}
	else if (IS_HW("VRX288_VOX_2.0_DE"))
	{
	    token_set("BOARD", "Vox2.0 DE");
	    token_set("FIRM", "Vodafone");
            token_set_y("CONFIG_HW_AD900X");
	    token_set_y("CONFIG_HW_C000J");
	}
	else if (IS_HW("GRX288_VOX_2.0_INT"))
	{
	    token_set("BOARD", "Vox2.0 INT");
	    token_set("FIRM", "Vodafone");
            token_set_y("CONFIG_HW_ADA00X_INT");
	}
	else
	{
	    token_set("BOARD", "VRX288");
	    token_set("FIRM", "Lantiq");
	}

	if (token_get("CONFIG_HW_DSP"))
	{
	    token_set_m("CONFIG_IFX_OS");
	    token_set_m("CONFIG_IFX_TAPI");
	    token_set_m("CONFIG_IFX_VMMC");

	    token_set("CONFIG_HW_NUMBER_OF_FXS_PORTS", "2");
	    token_set_m("CONFIG_RG_XWAY_JDSP");

	    if (token_get("CONFIG_HW_ISDN"))
	    {
		if (IS_HW("GRX288_VOX_2.0_DE") || IS_HW("GRX288") ||
		    IS_HW("ADA00X") || IS_HW("GRX288_VOX_2.0_INT"))
		{
		    token_set_y("CONFIG_LANTIQ_IPACX");
		}
		if (IS_HW("VRX288_VOX_2.0_DE") || IS_HW("VRX288"))
		{
		    token_set_y("CONFIG_LANTIQ_TSMINT");
		    token_set("CONFIG_HW_NUMBER_OF_ISDN_TE_LINES", "1");
		}
		token_set("CONFIG_HW_NUMBER_OF_ISDN_NT_LINES", "1");
	    }
	    token_set_y("CONFIG_XRX");
	    token_set_y("CONFIG_IFX_VOICE_CPE_TAPI_FAX");
	    if (token_get("CONFIG_RG_VOIP_DSP_TRANSCODING"))
	    {
		if (IS_HW("GRX288_VOX_2.0_DE") || IS_HW("GRX288_VOX_2.0_INT"))
		    token_set("CONFIG_RG_VOIP_DSP_TRANSCODING_LINE", "4");
		else if (IS_HW("VRX288_VOX_2.0_DE"))
		    token_set("CONFIG_RG_VOIP_DSP_TRANSCODING_LINE", "9");
	    }
	    if (token_get("CONFIG_HW_FXO"))
	    {
		token_set("CONFIG_HW_NUMBER_OF_FXO_PORTS", "1");
		if (IS_HW("VRX288_VOX_2.0_DE"))
		{
		    token_set_y("CONFIG_SPI_SI3050");
		    token_set("CONFIG_RG_FXO_TIMESLOT", "8");
		}
	    }
	}

	token_set_y("CONFIG_VR9");
	token_set_y("CONFIG_BOOTLDR_UBOOT");
	
	if (IS_HW("VRX288") || IS_HW("GRX288"))
	    token_set_y("CONFIG_VR9_REF_BOARD");
	if (IS_HW("GRX288_VOX_2.0_DE") || IS_HW("VRX288_VOX_2.0_DE") ||
	    IS_HW("GRX288_VOX_2.0_INT"))
	{
	    token_set_y("CONFIG_VR9_VOX_2_0_BOARD");
	}
	if (token_get("CONFIG_HW_ADA00X"))
	    token_set_y("CONFIG_VR9_ADA00X_BOARD");
	if (token_get("CONFIG_HW_ADA00X_INT"))
	    token_set_y("CONFIG_VR9_ADA00X_INT_BOARD");
	if (token_get("CONFIG_HW_AD900X"))
	    token_set_y("CONFIG_VR9_AD900X_BOARD");
	if (token_get("CONFIG_HW_A000J"))
	    token_set_y("CONFIG_VR9_A000J_BOARD");
	if (token_get("CONFIG_HW_C000J"))
	    token_set_y("CONFIG_VR9_C000J_BOARD");

	if (IS_HW("ADA00X") || IS_HW("GRX288_VOX_2.0_DE") ||
	    IS_HW("VRX288_VOX_2.0_DE") || IS_HW("GRX288_VOX_2.0_INT"))
	{
	    token_set("CONFIG_SDRAM_SIZE", "128");
	}
	else
	    token_set("CONFIG_SDRAM_SIZE", "64");

	if (IS_HW("ADA00X") || IS_HW("GRX288_VOX_2.0_DE") ||
	    IS_HW("VRX288_VOX_2.0_DE") || IS_HW("GRX288_VOX_2.0_INT"))
	{
	    token_set("CONFIG_HW_FLASH_NOR", "n");
	}
	else
	    token_set("CONFIG_MTD_IFX_NOR_FLASH_SIZE", "8");

	if (token_get("CONFIG_HW_FLASH_NOR"))
	{
	    token_set("CONFIG_MTD_PHYSMAP_START", "0x10000000");
	    token_set("CONFIG_MTD_PHYSMAP_LEN", "0x00800000");
	}

	token_set("CONFIG_RG_CONSOLE_DEVICE", "ttyS0");
	token_set("CONFIG_CMDLINE", "console=ttyS0,115200");

	token_set_y("CONFIG_HW_FLASH_NAND");
	
	if (IS_HW("VRX288") || IS_HW("ADA00X") || IS_HW("GRX288_VOX_2.0_DE") ||
	    IS_HW("VRX288_VOX_2.0_DE") || IS_HW("GRX288_VOX_2.0_INT"))
	{
	    token_set("CONFIG_HW_NAND_PEB_SIZE", "0x20000");
	    token_set("CONFIG_HW_NAND_LEB_SIZE", "0x1F800");
	    token_set("CONFIG_HW_NAND_MIN_IO_SIZE", "2048");
	    token_set("CONFIG_HW_NAND_SUB_PAGE_SIZE", "512");

	    /* uboot_env partition's size and offset must correlate to the
	     * sizes in u-boot.
	     * (vendor/lantiq/xway/u-boot/include/configs/vr9_cfg.h)
	     */
	    token_set("CONFIG_RG_UBOOT_ENV_PARTITION_NAME", "uboot_env");
	    token_set("CONFIG_RG_UBOOT_ENV_PARTITION_SIZE", "0x60000"); 
	    token_set("CONFIG_RG_UBOOT_ENV_PARTITION_OFFSET", "0x000a0000");
	}
	else if (IS_HW("GRX288"))
	{
	    /* We do not fully support the NAND variant on the GRX reference
	     * board (MLC technology) but these physical parameters are correct.
	     */
	    token_set("CONFIG_HW_NAND_PEB_SIZE", "0x40000");
	    token_set("CONFIG_HW_NAND_MIN_IO_SIZE", "2048");
	    token_set("CONFIG_HW_NAND_SUB_PAGE_SIZE", "2048");
	}
	
	if (token_get("CONFIG_RG_PSE_HW"))
	{
	    token_set_y("CONFIG_IFX_PPA");
	    token_set("CONFIG_RG_PSE_HW_PLAT_PATH", 
		"vendor/lantiq/xway/rg/modules");

	    if (IS_HW("GRX288_VOX_2.0_DE"))
		token_set_y("CONFIG_LANTIQ_VR9_PORT_SEP");
	}

	if (token_get("CONFIG_HW_ETH_WAN"))
	    token_set_y("CONFIG_LANTIQ_VR9_PORT_SEP");

	if (token_get("CONFIG_HW_DSL_WAN") || token_get("CONFIG_HW_VDSL_WAN"))
	{
	    token_set_m("CONFIG_IFX_OS");
	    dev_add("dsl0", DEV_IF_LANTIQ_XWAY_DSL, DEV_IF_NET_EXT);

	    /* xDSL daemon application requires threads support. */
	    token_set_y("CONFIG_RG_LIBC_CUSTOM_STREAMS");
	}

	if (token_get("CONFIG_HW_DSL_WAN"))
	{
	    token_set_y("CONFIG_IFX_ATM_OAM");
	    token_set_y("CONFIG_RG_ATM_QOS");
	    dsl_atm_dev_add("atm0", DEV_IF_LANTIQ_XWAY_ATM, "dsl0");
	    if (!token_get("CONFIG_IFX_PPA"))
		token_set_m("CONFIG_IFX_ATM");
	}

	if (token_get("CONFIG_HW_VDSL_WAN"))
	{
	    dsl_ptm_dev_add("ptm0", DEV_IF_LANTIQ_XWAY_PTM, "dsl0");
	    if (!token_get("CONFIG_IFX_PPA"))
		token_set_m("CONFIG_IFX_PTM");
	}

	if (token_get("CONFIG_HW_ETH_LAN") || token_get("CONFIG_HW_ETH_WAN"))
	{
	    token_set_y("CONFIG_GE_MODE");
	    token_set_y("CONFIG_GPHY_DRIVER");
	    token_set_y("CONFIG_IFX_ETH_FRAMEWORK");
	    token_set_y("CONFIG_IFX_ETHSW_API");

	    if (token_get("CONFIG_IFX_PPA"))
	    {
		int drv_count = 0;

		if (token_get("CONFIG_HW_VDSL_WAN"))
		{
		    token_set_m("CONFIG_IFX_PPA_E5");
		    drv_count++;
		}
		if (token_get("CONFIG_HW_DSL_WAN"))
		{
		    token_set_m("CONFIG_IFX_PPA_A5");
		    drv_count++;
		}

		/* In case A5 & E5 are loaded, we decide which driver to load 
		 * at runtime.
		 */
		if (drv_count > 1)
		    token_set_y("CONFIG_RG_LANTIQ_XWAY_DYN_DATAPATH");
		else if (!drv_count)
		    token_set_m("CONFIG_IFX_PPA_D5");


		token_set_y("CONFIG_IFX_PPA_DATAPATH");
		token_set_y("CONFIG_IFX_PPA_API_PROC");
	    }
	    else 
	    {
		/* Note:
		 * CONFIG_IFX_7PORT_SWITCH is the driver for the VR9 internal 
		 * MAC. It MUST NOT be used if CONFIG_IFX_PPA (fastpath) is set.
		 */
		token_set_y("CONFIG_IFX_7PORT_SWITCH");

		if (token_get("CONFIG_HW_ETH_WAN"))
		    token_set_y("CONFIG_SW_ROUTING_MODE");
	    }

	    if (token_get("CONFIG_LANTIQ_VR9_PORT_SEP"))
	    {
		if (!token_get("CONFIG_HW_ETH_WAN") &&
		    IS_HW("GRX288_VOX_2.0_DE"))
		{
		    /* Physical port 3 is not connected, but use it for dummy
		     * WAN separation because of directpath requirement.
		     */
		    token_set("CONFIG_LANTIQ_VR9_PORT_WAN_ETH", "3");
		}
		else if (IS_HW("ADA00X") || IS_HW("GRX288_VOX_2.0_DE") ||
		    IS_HW("VRX288_VOX_2.0_DE") || IS_HW("GRX288_VOX_2.0_INT"))
		{
                    token_set("CONFIG_LANTIQ_VR9_PORT_WAN_ETH", "4");
		}
                else
                    token_set("CONFIG_LANTIQ_VR9_PORT_WAN_ETH", "5");
		token_set("CONFIG_LANTIQ_VR9_VLAN_CPU", "4001");
		token_set("CONFIG_LANTIQ_VR9_VLAN_WAN_ETH", "4002");
		if (token_get("CONFIG_HW_DSL_WAN") ||
 	            token_get("CONFIG_HW_VDSL_WAN"))
 	        {
		    token_set("CONFIG_LANTIQ_VR9_VLAN_WAN_DSL", "4003");
		}
	    }
	}

	if (!token_get("CONFIG_LSP_DIST"))
	{
	    if (token_get("CONFIG_HW_ETH_LAN"))
	    {
		token_set("CONFIG_HW_SWITCH_FIRST_PHY", "1");
		if (!IS_HW("VRX288_VOX_2.0_DE"))
		{
		    switch_dev_add("eth0", NULL, DEV_IF_LANTIQ_VR9_HW_SWITCH, 
			DEV_IF_NET_INT, 6);
		}
		else
		{
#ifdef CONFIG_RG_DO_DEVICES
		    cascaded_sw_port_map_t rtl836x_ports[] = {
			{1, 1},
			{2, 2},
			{3, 3},
			{4, 4},
			{-1}
		    };
		    cascaded_sw_port_map_t vr9_ports[] = {
			{5, 2}, /* rtl836x rgmii. vr9's port is virtual */
			{6, 6}, /* vr9 cpu port. vr9's port is physical */
			{7, 0}, /* wireless port. vr9's port is physical */
                        {8, 5}, /* wireless port. vr9's port is physical */
			{-1}
		    };
#endif

		    token_set_m("CONFIG_RG_HW_SWITCH_RTL836X");
		    token_set_y("CONFIG_RG_REALTEK_RTL8365MB");
		    token_set_y("CONFIG_RG_REALTEK_RTL8367RB");

		    switch_dev_add("eth0", NULL, DEV_IF_CASCADED_HW_SWITCH,
			DEV_IF_NET_INT, 6);

		    switch_dev_add("lantiq_sw", NULL,
			DEV_IF_LANTIQ_VR9_HW_SWITCH_NO_ETH, DEV_IF_NET_INT, 6);
#ifdef CONFIG_RG_DO_DEVICES
		    dev_add_to_cascader("eth0", "lantiq_sw",
			CASCADER_ENSLAVED_IS_MASTER | CASCADER_IS_MII, 0,
			vr9_ports);
#endif

		    switch_dev_add("realtek_sw", NULL, DEV_IF_RTL836X_HW_SWITCH,
			DEV_IF_NET_INT, 1);
#ifdef CONFIG_RG_DO_DEVICES
		    dev_add_to_cascader("eth0", "realtek_sw", CASCADER_IS_MII,
			2, rtl836x_ports);
#endif
		    token_set("CONFIG_VF_SWITCH_SNOOPING_DEV", "realtek_sw");
		}
	    }
	    if (token_get("CONFIG_HW_ETH_WAN"))
		dev_add("eth1", DEV_IF_LANTIQ_VR9_ETH, DEV_IF_NET_EXT);
	}

	if (IS_HW("GRX288") || IS_HW("VRX288"))
	{
	    token_set_y("CONFIG_PCI");
	    token_set_y("CONFIG_PCIEPORTBUS");
	}
	else if (token_get("CONFIG_HW_A000J"))
	    token_set_y("CONFIG_PCI");

	if (IS_HW("VRX288") || IS_HW("VRX288_VOX_2.0_DE"))
	    token_set_y("CONFIG_IFX_PCIE_PHY_36MHZ_MODE");
	else if (IS_HW("GRX288") || IS_HW("ADA00X") || 
	    IS_HW("GRX288_VOX_2.0_DE") || IS_HW("GRX288_VOX_2.0_INT"))
	{
	    token_set_y("CONFIG_IFX_PCIE_PHY_25MHZ_MODE");
	}

	if (token_get("CONFIG_HW_80211N_LANTIQ_WAVE300"))
        {
            token_set("CONFIG_HW_WAVE300_HW_TYPE", "30");
            token_set("CONFIG_HW_WAVE300_HW_REVISION", "B");
	    wireless_phy_dev_add(DEV_IF_WAVE300, "wlan0", -1, 1, "br0");
        }

	if (IS_HW("ADA00X") || IS_HW("GRX288_VOX_2.0_DE") ||
	    IS_HW("VRX288_VOX_2.0_DE") || IS_HW("GRX288_VOX_2.0_INT"))
        {
            if (token_get("CONFIG_HW_80211N_RALINK_RT3883"))
            {
		token_set("CONFIG_RALINK_DEV_ID_OFFSET", "4");
                /* In RT5592 ra00 is expected to be 2.4GHz and ra01 5GHz */
                if (token_get("CONFIG_RALINK_RT5592"))
                {
                    ralink_rt3883_add("ra01_%d", 1);
                    ralink_rt3883_add("ra00_%d", 0);
                }
                else
                {
                    ralink_rt3883_add("ra01_%d", 0);
                    ralink_rt3883_add("ra00_%d", 1);
                }
                dev_set_dependency("ra00_0", "eth0");
                dev_set_dependency("ra01_0", "eth0");
		token_set("CONFIG_RALINK_MII_DEF_RX_DEV", "\"eth0\"");
            }
	    if (token_get("CONFIG_HW_AD900X") || token_get("CONFIG_HW_C000J"))
		token_set("CONFIG_LANTIQ_VR9_MAX_LAN_PORTS", "2");
	    else
		token_set("CONFIG_LANTIQ_VR9_MAX_LAN_PORTS", "4");
	}
	else
	    token_set("CONFIG_LANTIQ_VR9_MAX_LAN_PORTS", "5");


	if (token_get("CONFIG_HW_USB_STORAGE"))
	{
	    token_set_y("CONFIG_USB");
	    if (token_get("CONFIG_HW_A000J"))
	    {
		token_set_y("CONFIG_USB_EHCI_HCD");
		token_set_y("CONFIG_USB_EHCI_BIG_ENDIAN_MMIO");
		token_set_y("CONFIG_USB_UHCI_HCD");
		token_set_y("CONFIG_USB_UHCI_BIG_ENDIAN_MMIO");
	    }
	}

	if (token_get("CONFIG_HW_ENCRYPTION"))
	{
	    token_set_y("CONFIG_CRYPTO_DEV_DEU");
	    token_set_y("CONFIG_CRYPTO_DEV_VR9");
	}

        if (token_get("CONFIG_RG_3G_WAN"))
            gsm_dev_add("gsm0", DEV_IF_3G_USB_SERIAL, 1);

	if (token_get("CONFIG_RG_LTE"))
	{
	    if (token_get("CONFIG_HW_ADA00X") ||
		token_get("CONFIG_HW_ADA00X_INT"))
	    {
		token_set_y("CONFIG_HW_GCT_LTE_GDM7240");
		dev_add("lte0", DEV_IF_GDM7240_LTE, DEV_IF_NET_EXT);
		dev_can_be_missing("lte0");
	    }
	    if (token_get("CONFIG_HW_A000J"))
	    {
		token_set_y("CONFIG_HW_QUANTA_LTE_1KG");
		dev_add("qlte0", DEV_IF_QLTE_PHYSICAL, DEV_IF_NET_EXT);
		dev_can_be_missing("qlte0");
		dev_add("usb0", DEV_IF_QLTE, DEV_IF_NET_EXT);
		dev_can_be_missing("usb0");
		dev_set_route_group("usb0", DEV_ROUTE_GROUP_MAIN);
		dev_set_dependency("usb0", "qlte0");
		dev_add("usb1", DEV_IF_QLTE, DEV_IF_NET_EXT);
		dev_can_be_missing("usb1");
		dev_set_route_group("usb1", DEV_ROUTE_GROUP_VOIP);
		dev_set_dependency("usb1", "qlte0");
	    }
	}

	if (token_get("CONFIG_HW_LCD"))
	    lcd_configure();

	if (token_get("CONFIG_HW_TOUCH_BUTTONS"))
	    touch_buttons_configure();
	
	if (token_get("CONFIG_RG_HW_WATCHDOG"))
	{
	    token_set_m("CONFIG_IFX_WDT");
	    token_set_y("CONFIG_RG_HW_WATCHDOG_IOCTL");
	    token_set_y("CONFIG_RG_HW_WATCHDOG_ARCH_LINUX_GENERIC");
	}
    }

    if (IS_HW("WADB100G"))
    {
	token_set_y("CONFIG_BCM963XX_COMMON");
	token_set_y("CONFIG_BCM96348");
	
	token_set_y("CONFIG_SERIAL_CORE");
	token_set_y("CONFIG_SERIAL_CORE_CONSOLE");
	if (token_get("CONFIG_HW_ETH_WAN"))
	{
	    token_set_m("CONFIG_BCM963XX_ETH"); 
	    dev_add("bcm0", DEV_IF_BCM963XX_ETH, DEV_IF_NET_EXT); 
	}
	
	token_set_y("CONFIG_BCM963XX_SERIAL");

        token_set_y("CONFIG_MTD_CFI_AMDSTD");
	
	token_set("CONFIG_BCM963XX_BOARD_ID", "96348GW-10");
	token_set("CONFIG_BCM963XX_CHIP", "6348");
	token_set("CONFIG_BCM963XX_SDRAM_SIZE", "16");
	token_set("CONFIG_RG_FLASH_LAYOUT_SIZE", "4");
	/* this value is taken from 
	 * vendor/broadcom/bcm963xx/linux-2.6/bcmdrivers/opensource/include/bcm963xx/board.h*/
        mtd_physmap_add(0x1FC00000,
	     token_get("CONFIG_RG_FLASH_LAYOUT_SIZE") * 1024 * 1024, 2);

	token_set_y("CONFIG_PCI");

	if (token_get("CONFIG_HW_DSL_WAN"))
	{
	    token_set_y("CONFIG_ATM");
	    token_set_m("CONFIG_BCM963XX_ADSL");
	    token_set_m("CONFIG_BCM963XX_ATM");
	    token_set_y("CONFIG_RG_ATM_QOS");
	    dsl_atm_dev_add("bcm_atm0", DEV_IF_BCM963XX_ADSL, NULL);
	}

	if (token_get("CONFIG_HW_ETH_LAN"))
	{
	    dev_add("bcm0", DEV_IF_BCM963XX_ETH, DEV_IF_NET_INT);

	    if (!token_get("CONFIG_HW_SWITCH_LAN"))
	    {
		token_set_m("CONFIG_BCM963XX_ETH");
		switch_dev_add("bcm1", NULL, DEV_IF_BCM963XX_ETH,
		    DEV_IF_NET_INT, -1);
	    }
	}

	if (token_get("CONFIG_HW_SWITCH_LAN"))
	{
	    token_set_m("CONFIG_HW_SWITCH_BCM53XX");
	    token_set_m("CONFIG_BCM963XX_ETH");
	    switch_dev_add("bcm1", NULL, DEV_IF_BCM5325A_HW_SWITCH,
		DEV_IF_NET_INT, 5);
	}
	
	if (token_get("CONFIG_HW_80211G_BCM43XX"))
	{
	    token_set_y("CONFIG_BCM4318");

	    bcm43xx_add("wl0");
	}

	if (token_get("CONFIG_HW_USB_RNDIS"))
	{
	    token_set_y("CONFIG_USB_RNDIS");
	    token_set_m("CONFIG_BCM963XX_USB");
	    dev_add("usb0", DEV_IF_USB_RNDIS, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_LEDS"))
	    token_set_y("CONFIG_RG_UIEVENTS");

	token_set("BOARD", "WADB100G");
	token_set("FIRM", "Broadcom");
    }

    if (IS_HW("ASUS6020VI"))
    {
	token_set_y("CONFIG_BCM963XX_COMMON");
	token_set_y("CONFIG_BCM96348");

	token_set_y("CONFIG_SERIAL_CORE");
	token_set_y("CONFIG_SERIAL_CORE_CONSOLE");
	if (token_get("CONFIG_HW_ETH_WAN"))
	{
	    token_set_m("CONFIG_BCM963XX_ETH"); 
	    dev_add("bcm0", DEV_IF_BCM963XX_ETH, DEV_IF_NET_EXT); 
	}
	token_set_y("CONFIG_BCM963XX_SERIAL");

        token_set_y("CONFIG_MTD_CFI_AMDSTD");
	
	token_set("CONFIG_BCM963XX_BOARD_ID", "AAM6020VI");
	token_set("CONFIG_BCM963XX_CHIP", "6348");
	token_set("CONFIG_BCM963XX_SDRAM_SIZE", "16");
	token_set("CONFIG_RG_FLASH_LAYOUT_SIZE", "4");

	/* taken from vendor/broadcom/bcm963xx/kernel/bcm963xx/board.h */
        mtd_physmap_add(0x1FC00000,
	     token_get("CONFIG_RG_FLASH_LAYOUT_SIZE") * 1024 * 1024, 2);

	token_set_y("CONFIG_PCI");

	if (token_get("CONFIG_HW_DSL_WAN"))
	{
	    token_set_y("CONFIG_ATM");
	    token_set_m("CONFIG_BCM963XX_ADSL");
	    token_set_m("CONFIG_BCM963XX_ATM");
	    token_set_y("CONFIG_RG_ATM_QOS");
	    dsl_atm_dev_add("bcm_atm0", DEV_IF_BCM963XX_ADSL, NULL);
	}

	if (token_get("CONFIG_HW_SWITCH_LAN"))
	{
	    token_set_m("CONFIG_HW_SWITCH_BCM53XX");
	    token_set_m("CONFIG_BCM963XX_ETH");
	    switch_dev_add("bcm1", NULL, DEV_IF_BCM5325E_HW_SWITCH,
		DEV_IF_NET_INT, 5);
	}

	if (token_get("CONFIG_HW_ETH_LAN"))
	{
	    token_set_m("CONFIG_BCM963XX_ETH");
	    dev_add("bcm1", DEV_IF_BCM963XX_ETH, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_80211G_BCM43XX"))
	{
	    token_set_y("CONFIG_BCM4318");

	    bcm43xx_add("wl0");
	}

	if (token_get("CONFIG_HW_LEDS"))
	    token_set_y("CONFIG_RG_UIEVENTS");

	token_set("BOARD", "ASUS6020VI");
	token_set("FIRM", "Asus");
    }

    if (IS_HW("CN3XXX"))
    {
	token_set_y("CONFIG_CAVIUM_OCTEON");

	/* Ethernet */
	token_set_m("CONFIG_CAVIUM_ETHERNET");
	token_set_y("CONFIG_MII");
	if (token_get("CONFIG_HW_ETH_LAN"))
	    dev_add("eth1", DEV_IF_CN3XXX_ETH, DEV_IF_NET_INT);
	if (token_get("CONFIG_HW_ETH_WAN"))
	    dev_add("eth0", DEV_IF_CN3XXX_ETH, DEV_IF_NET_EXT);
	else if (token_get("CONFIG_HW_ETH_LAN2"))
	    dev_add("eth0", DEV_IF_CN3XXX_ETH, DEV_IF_NET_INT);

	/* USB */
	if (token_get("CONFIG_HW_USB_STORAGE"))
	{
	    token_set_y("CONFIG_USB");
	    token_set_m("CONFIG_USB_DWC_OTG");
	    token_set_y("CONFIG_USB_DEVICEFS");
	}

	/* IPSec */
	if (token_get("CONFIG_HW_ENCRYPTION"))
	{
	    token_set_y("CONFIG_IPSEC_USE_OCTEON_CRYPTO");
	    token_set_m("CONFIG_IPSEC_ALG_JOINT");
	}

    	/* Flash/MTD */
	token_set_y("CONFIG_MTD");
	token_set_y("CONFIG_MTD_CFI_ADV_OPTIONS");
	token_set_y("CONFIG_MTD_CFI_NOSWAP");
	token_set_y("CONFIG_MTD_CFI_GEOMETRY");
	token_set_y("CONFIG_MTD_MAP_BANK_WIDTH_1");
	token_set_y("CONFIG_MTD_CFI_I1");
	token_set_y("CONFIG_MTD_CFI_AMDSTD");
	token_set("CONFIG_MTD_CFI_AMDSTD_RETRY", "0");
	token_set_y("CONFIG_MTD_CFI_UTIL");
	token_set_y("CONFIG_MTD_SLRAM");

	mtd_physmap_add(0x1f400000, 0x800000, 1);

	if (token_get("CONFIG_RG_PSE_HW"))
	{
	    token_set_y("CONFIG_CAVIUM_FASTPATH");
	    token_set("CONFIG_RG_PSE_HW_PLAT_PATH", 
		"vendor/cavium/octeon/fastpath");
	}
    }    

    if (IS_HW("BCM94702"))
    {
	token_set_y("CONFIG_BCM947_COMMON");
	token_set_y("CONFIG_BCM4702");

	/* In order to make an root cramfs based dist use the following 
	 * instead of SIMPLE_RAMDISK
	 *  token_set_y("CONFIG_CRAMFS");
	 *  token_set_y("CONFIG_SIMPLE_CRAMDISK");
	 *  token_set("CONFIG_CMDLINE", 
	 *      "\"root=/dev/mtdblock2 noinitrd console=ttyS0,115200\"");
	 */

	if (token_get("CONFIG_HW_ETH_WAN") || token_get("CONFIG_HW_ETH_LAN"))
	{
	    token_set_m("CONFIG_ET");
	    token_set_y("CONFIG_ET_47XX");
	}

	if (token_get("CONFIG_HW_ETH_WAN"))
	    dev_add("bcm1", DEV_IF_BCM4710_ETH, DEV_IF_NET_EXT);

	if (token_get("CONFIG_HW_ETH_LAN"))
	    dev_add("bcm0", DEV_IF_BCM4710_ETH, DEV_IF_NET_INT);

	token_set("BOARD", "BCM94702");
	token_set("FIRM", "Broadcom");
    }

    if (IS_HW("BCM94704"))
    {
	token_set_y("CONFIG_BCM947_COMMON");
	token_set_y("CONFIG_BCM4704");

	if (token_get("CONFIG_HW_ETH_WAN") || token_get("CONFIG_HW_ETH_LAN"))
	{
	    token_set_m("CONFIG_ET");
	    token_set_y("CONFIG_ET_47XX");
	}

	if (token_get("CONFIG_HW_ETH_WAN"))
	    dev_add("bcm1", DEV_IF_BCM4710_ETH, DEV_IF_NET_EXT);

	if (token_get("CONFIG_HW_ETH_LAN"))
	    dev_add("bcm0", DEV_IF_BCM4710_ETH, DEV_IF_NET_INT);

	if (token_get("CONFIG_HW_80211G_BCM43XX"))
	    bcm43xx_add("eth0");

	token_set("BOARD", "BCM94704");
	token_set("FIRM", "Broadcom");
    }

    if (IS_HW("BCM94712"))
    {
	/* This means (among others) copy CRAMFS to RAM, which is much
	 * safer, but PMON/CFE currently has a limit of ~3MB when uncompressing.
	 * If your image is larger than that, either reduce image size or
	 * remove CONFIG_COPY_CRAMFS_TO_RAM for this platform. */
	/* TODO: CONFIG_COPY_CRAMFS_TO_RAM does not exist any longer... Check
	 * what should be done here... */
	token_set_y("CONFIG_BCM947_COMMON");
	token_set_y("CONFIG_BCM4710");

	if (token_get("CONFIG_HW_ETH_WAN") || token_get("CONFIG_HW_ETH_LAN"))
	{
	    token_set_m("CONFIG_ET");
	    token_set_y("CONFIG_ET_47XX");
	    token_set_y("CONFIG_RG_VLAN_8021Q");
	}

	if (token_get("CONFIG_HW_ETH_WAN"))
	    dev_add("bcm0.1", DEV_IF_BCM4710_ETH, DEV_IF_NET_EXT);

	if (token_get("CONFIG_HW_ETH_LAN"))
	    dev_add("bcm0.0", DEV_IF_BCM4710_ETH, DEV_IF_NET_INT);

	if (token_get("CONFIG_HW_80211G_BCM43XX"))
	    bcm43xx_add("eth0");

	token_set("BOARD", "BCM94712");
	token_set("FIRM", "Broadcom");
    }

    if (IS_HW("WRT54G"))
    {
	token_set_y("CONFIG_BCM947_COMMON");
	token_set_y("CONFIG_BCM947_HW_BUG_HACK");

	if (token_get("CONFIG_HW_ETH_WAN") || token_get("CONFIG_HW_ETH_LAN"))
	{
	    token_set_m("CONFIG_ET");
	    token_set_y("CONFIG_ET_47XX");
	    token_set_y("CONFIG_RG_VLAN_8021Q");
	    token_set_y("CONFIG_VLAN_8021Q_FAST");
	    dev_add("bcm0", DEV_IF_BCM4710_ETH, DEV_IF_NET_EXT);
	}

	if (token_get("CONFIG_HW_ETH_WAN"))
	    dev_add("bcm0.1", DEV_IF_VLAN, DEV_IF_NET_EXT);

	if (token_get("CONFIG_HW_ETH_LAN"))
	    dev_add("bcm0.2", DEV_IF_VLAN, DEV_IF_NET_INT);

	if (token_get("CONFIG_HW_80211G_BCM43XX"))
	    bcm43xx_add("eth0");

	token_set("BOARD", "Cybertan");
	token_set("FIRM", "Cybertan");
    }

    if (IS_HW("BCM963168AC5") || IS_HW("BCM963168_VOX_2.5_IT") ||
	IS_HW("BCM963168_ADB") || IS_HW("BCM963168_AUQ00X") ||
	IS_HW("BCM963168_HUAWEI") || IS_HW("BCM963168_VOX_2.5_UK") ||
	IS_HW("BCM963168_VOX_2.5_DE"))
    {
	token_set_y("CONFIG_BCM963268");
	token_set("BRCM_CHIP", "63268");
	if (token_get("CONFIG_RG_OS_LINUX_3X"))
	    token_set_y("CONFIG_BROADCOM_9636X_3X");
	else
	    token_set_y("CONFIG_BROADCOM_9636X");
	token_set("BRCM_BOARD", "bcm963xx");

	if (IS_HW("BCM963168AC5"))
	{
	    token_set("BOARD", "BCM963168AC5");
	    token_set("BRCM_BOARD_ID", "963168AC5");
	    token_set("BRCM_VOICE_BOARD_ID", "SI3217X");
	    token_set("BRCM_FLASHBLK_SIZE", "64");
	    token_set_y("CONFIG_HW_BCM963168AC5");
	    token_set("FIRM", "Broadcom");
	}
	else if (IS_HW("BCM963168_VOX_2.5_IT") || IS_HW("BCM963168_VOX_2.5_UK") ||
	    IS_HW("BCM963168_ADB") || IS_HW("BCM963168_HUAWEI") ||
	    IS_HW("BCM963168_VOX_2.5_IT"))
	{
	    if (IS_HW("BCM963168_ADB"))
	    {
		token_set("BOARD", "BCM963168_ADB");
		token_set("BRCM_BOARD_ID", "963168_ADBVOX25");
		token_set("BRCM_VOICE_BOARD_ID", "ZL88601");
		token_set_y("CONFIG_HW_BCM963168_ADB");
	    }
	    else if (IS_HW("BCM963168_HUAWEI"))
	    {
		token_set("BOARD", "BCM963168_HUAWEI");
		token_set("BRCM_BOARD_ID", "963168VX");
		token_set("BRCM_VOICE_BOARD_ID", "LE9662_ZSI");
		token_set_y("CONFIG_HW_BCM963168_HUAWEI");
	    }
	    else /* BCM963168_VOX_2.5_IT and BCM963168_VOX_2.5_UK */
	    {
		token_set_y("CONFIG_HW_BCM963168_HUAWEI");
		token_set_y("CONFIG_HW_BCM963168_ADB");
		if (IS_HW("BCM963168_VOX_2.5_IT"))
		    token_set_y("CONFIG_HW_BCM963168_AUQ00X");
	    }
	    token_set("BRCM_FLASHBLK_SIZE", "128");

	    token_set("CONFIG_HW_FLASH_NOR", "n");
	    token_set_y("CONFIG_HW_FLASH_NAND");
	    token_set("FIRM", "Broadcom");
	}
	else if (IS_HW("BCM963168_AUQ00X") || IS_HW("BCM963168_VOX_2.5_DE"))
	{
	    if (IS_HW("BCM963168_AUQ00X"))
	    {
		token_set("BOARD", "BCM963168_SERCOMM");
		token_set("FIRM", "Broadcom");
		token_set("BRCM_BOARD_ID", "963168MBV3");
		if (!token_get_str("BRCM_VOICE_BOARD_ID"))
		    token_set("BRCM_VOICE_BOARD_ID", "LE9672_ZSI");
		token_set_y("CONFIG_HW_BCM963168_AUQ00X");
	    }
	    else /* BCM963168_VOX_2.5_DE */
	    {
		token_set("BOARD", "Vox2.5 DE");
		token_set("FIRM", "Vodafone");
		token_set_y("CONFIG_HW_BCM963168_AUQ00X");
	    }
	    token_set("BRCM_FLASHBLK_SIZE", "128");

	    token_set("CONFIG_HW_FLASH_NOR", "n");
	    token_set_y("CONFIG_HW_FLASH_NAND");
	}

	token_set("BRCM_NUM_MAC_ADDRESSES", "11");
	token_set("BRCM_BASE_MAC_ADDRESS", "02:10:18:01:00:01");
	token_set("BRCM_MAIN_TP_NUM", "0");
	token_set("BRCM_PSI_SIZE", "24");
	if (!token_get("CONFIG_SDRAM_SIZE"))
	    token_set("CONFIG_SDRAM_SIZE", "128");

	token_set_y("CONFIG_BOOTLDR_CFE");

	if (!token_get("CONFIG_LSP_DIST"))
	{
	    if (token_get("CONFIG_HW_SWITCH_LAN"))
	    {
		token_set_m("CONFIG_HW_SWITCH_BCM53125");
		token_set_m("CONFIG_HW_SWITCH_BCM9636X");
		token_set("CONFIG_HW_SWITCH_FIRST_PHY", "1");
		switch_dev_add("bcmsw", NULL, DEV_IF_BCM53125_HW_SWITCH,
		    DEV_IF_NET_INT, 8);	    
		if (IS_HW("BCM963168_VOX_2.5_DE"))
		{
		    token_set_y("CONFIG_HW_SWITCH_BCM53125_CPU_TRAP_IGMP");
		    token_set("CONFIG_VF_SWITCH_SNOOPING_DEV", "bcmsw");
		}
	    }
	    else if (token_get("CONFIG_HW_ETH_LAN"))
	    {
		dev_add("bcmsw", DEV_IF_BCM9636X_ETH, DEV_IF_NET_INT);
	    }
	}

	if (token_get("CONFIG_HW_ETH_LAN") || token_get("CONFIG_HW_ETH_WAN"))
	{
	    token_set_m("CONFIG_BCM_ENET");
	    token_set_y("CONFIG_BCM_EXT_SWITCH");
	    /* The external switch is 53125 but the code refers to all of the
	     * family as 53115 */
	    token_set("CONFIG_BCM_EXT_SWITCH_TYPE", "53115");

	    if (token_get("CONFIG_BROADCOM_9636X_3X"))
		token_set_y("CONFIG_BCM_PORTS_ON_INT_EXT_SW");
	}

	if (token_get("CONFIG_HW_80211N_BCM432X"))
	{
	    /* WLAN chip on SoC */
	    wireless_vaps_add("wl0", DEV_IF_BCM432X_WLAN_5GHZ, "wl0.%d", 1,
		NULL);
	    
	    /* External WLAN 4360 with 802.11AC support */
	    wireless_vaps_add("wl1", DEV_IF_BCM_WLAN_11AC, "wl1.%d", 1, NULL);
	}

	if (token_get("CONFIG_HW_DSL_WAN"))
	{
	    token_set_y("CONFIG_ATM");
	    token_set_y("CONFIG_RG_ATM_QOS");
	    dev_add("bcm_dsl0", DEV_IF_BCM9636X_DSL, DEV_IF_NET_EXT);
	    dsl_atm_dev_add("bcm_atm0", DEV_IF_BCM9636X_ATM, "bcm_dsl0");
	    dsl_ptm_dev_add("bcm_ptm0", DEV_IF_BCM9636X_PTM, "bcm_dsl0");
	}

	if (token_get("CONFIG_HW_ETH_WAN"))
	{
	    token_set_y("CONFIG_RG_DEV_IF_BCM963XX_ETH");
	    dev_add("eth0", DEV_IF_BCM963XX_ETH, DEV_IF_NET_EXT);
	    if (IS_HW("BCM963168_VOX_2.5_IT") ||
		IS_HW("BCM963168_VOX_2.5_UK"))
	    {
		token_set_y("CONFIG_BCM_GMAC");
		token_set("CONFIG_BCM_GMAC_DEV", "eth0");
	    }
	}

	if (token_get("CONFIG_HW_USB_STORAGE"))
	    token_set_m("CONFIG_BCM_USB");

	if (token_get("CONFIG_HW_DSP"))
	{
	    if (token_get("CONFIG_SMP"))
		token_set_y("CONFIG_BCM_BRCM_VOICE_NONDIST");
	    else
		token_set_m("CONFIG_BCM_BCMDSP");

	    token_set_m("CONFIG_BCM_ENDPOINT");

	    if (token_get("CONFIG_BROADCOM_9636X_3X"))
		token_set_y("BRCM_63268_UNI");
	    else
		token_set_y("BRCM_6362_UNI");

	    token_set("CONFIG_HW_NUMBER_OF_FXS_PORTS", "2");
	    token_set_y("CONFIG_RG_DSP_THREAD");
	}

        if (token_get("CONFIG_RG_3G_WAN"))
	{
	    gsm_dev_add("gsm0", DEV_IF_3G_USB_SERIAL, 1);
	    dev_add("ppp400", DEV_IF_PPPOS_CONN, DEV_IF_NET_EXT);
	    dev_set_dependency("ppp400", "gsm0");
	    dev_set_logical_dependency("ppp400", "gsm0");
	    token_set_m("CONFIG_HUAWEI_CDC_ETHER");
	    token_set_y("CONFIG_RG_HUAWEI_CDC_ETHER_NCM");
	    dev_add("cdc_ether0", DEV_IF_HUAWEI_CDC_ETHER, DEV_IF_NET_EXT);
	    dev_set_route_group("cdc_ether0", DEV_ROUTE_GROUP_MAIN);
	    if (token_get("CONFIG_RG_VODAFONE_3G_VOICE_OVER_PS"))
	    {
		gsm_dev_add("gsm1", DEV_IF_3G_USB_SERIAL, 2);
		dev_add("ppp401", DEV_IF_PPPOS_CONN, DEV_IF_NET_EXT);
		dev_set_dependency("ppp401", "gsm1");
		dev_set_logical_dependency("ppp401", "gsm1");
	    }
	}

	if (token_get("CONFIG_HW_FLASH_NAND"))
	{
	    char *buf = NULL;

	    /* Tightly coupled with CONFIG_SDRAM_SIZE.
	     * RGloader needs private mem to place the openrg image prior 
	     * booting; it is located right after the memory seen by the 
	     * rgloader kernel. Must also defined at the openrg dist (the 
	     * openrg image opener executabe should be linked to this address).
	     *
	     *  - Assuming SDRAM is more than 32M
	     *  - The last 32mb of memory are reserved for extracting the image.
	     */
	    str_printf(&buf, "0x%08x",
		(token_get("CONFIG_SDRAM_SIZE") - 32) * 1024 * 1024);
	    token_set("CONFIG_RGLOADER_RESERVED_RAM_START", buf);
	    str_free(&buf);

	    token_set("CONFIG_HW_NAND_PEB_SIZE", "0x20000"); /* 128KiB */
	    token_set("CONFIG_HW_NAND_LEB_SIZE", "0x1F000"); /* 124KiB*/
	    token_set("CONFIG_HW_NAND_MIN_IO_SIZE", "2048");
	    token_set("CONFIG_HW_NAND_SUB_PAGE_SIZE", "2048");
	    token_set("CONFIG_MTD_UBI_BEB_RESERVE", "3");
	}

	if (token_get("CONFIG_RG_HW_WATCHDOG"))
	{
	    token_set_m("CONFIG_RG_BCM9636X_WATCHDOG");
	    token_set_y("CONFIG_RG_HW_WATCHDOG_ARCH_LINUX_GENERIC");
	}

        if (token_get("CONFIG_RG_PSE_HW"))
        {
            token_set("CONFIG_RG_PSE_HW_PLAT_PATH",
                "vendor/broadcom/rg/bcm9636x/rg_modules");
	    /* Do not set FAP by default, but use HW PSE for PKTC shortcut */
	    if (IS_HW("BCM963168_VOX_2.5_IT") ||
		IS_HW("BCM963168_VOX_2.5_UK"))
	    {
		token_set_m("CONFIG_BCM_FAP");
		token_set("CONFIG_BCM_FAP_IMPL", "1");
	    }
        }

	if (token_get("CONFIG_HW_LEDS"))
	{
	    token_set_y("CONFIG_RG_UIEVENTS");
	    token_set_m("CONFIG_RG_KLEDS");
	}

	if (token_get("CONFIG_RG_KLEDS"))
	{
	    if (token_get("CONFIG_HW_BCM963168_ADB"))
		token_set_y("CONFIG_ADB_MCU_LEDS");

	    if (token_get("CONFIG_HW_BCM963168_AUQ00X"))
		token_set_m("CONFIG_SER_AUQ00X_MCU_BCM_LEDS");
	}

	if (token_get("CONFIG_RG_PROXIMITY_SENSOR"))
	{
	    if (token_get("CONFIG_HW_BCM963168_ADB"))
	    {
		token_set_y("CONFIG_PX3220");
		token_set_m("CONFIG_PX3220_ADB");
	    }

	    if (token_get("CONFIG_HW_BCM963168_AUQ00X"))
	    {
		token_set_y("CONFIG_PX3220");
		token_set_m("CONFIG_PX3220_SER");
	    }

	    if (token_get("CONFIG_HW_BCM963168_HUAWEI"))
	        token_set_m("CONFIG_TSL2672");
	}
    }

    if (IS_HW("BCM96362") || IS_HW("WVDB113G") || IS_HW("YWZ00A") ||
	IS_HW("YWZ00B") || IS_HW("HG558BZA") || IS_HW("BCM96362_VOX_1.5_IT")
	|| IS_HW("AGC00A") || IS_HW("BCM96362_VOX_1.5_ES") ||
	IS_HW("BCM96362_VOX_1.5_PT") || IS_HW("BCM96362_VOX_1.5_NZ"))
    {
	if (IS_HW("WVDB113G"))
	{
	    token_set("BOARD", "BCM96368");
	    token_set_y("CONFIG_BCM96368");
	    token_set("BRCM_CHIP", "6368");
	}
	else
	{
	    token_set("BOARD", "BCM96362");
	    token_set_y("CONFIG_BCM96362");
	    token_set("BRCM_CHIP", "6362");
	}

	/* BRCM_BOARD_ID and BRCM_VOICE_BOARD_ID are relevant ONLY for CFE rom
	 * default environment */
	if (IS_HW("WVDB113G"))
	    token_set("BRCM_BOARD_ID", "96368VVW");
	else if (IS_HW("BCM96362"))
	{
	    token_set("BRCM_BOARD_ID", "96362ADVNgr");
	    token_set("BRCM_VOICE_BOARD_ID", "SI3217X");
	}
	else if (IS_HW("YWZ00A") || IS_HW("YWZ00B"))
	{
	    token_set("BRCM_BOARD_ID", "96362ADVNgr");
	    token_set("BRCM_VOICE_BOARD_ID", "ADVNGR_SI32176");
	}
	else if (IS_HW("HG558BZA"))
	{
	    token_set("BRCM_BOARD_ID", "6362hhb3");
	    token_set("BRCM_VOICE_BOARD_ID", "LE88276");
	}
	else if (IS_HW("AGC00A"))
	{
	    token_set("BRCM_BOARD_ID", "96362AGC00A");
	    token_set("BRCM_VOICE_BOARD_ID", "ADVNGR_SI32176");
	}

	token_set_y("CONFIG_BROADCOM_9636X");

	if (IS_HW("YWZ00B") || IS_HW("HG558BZA"))
	    token_set_y("CONFIG_BROADCOM_BCM9636X_BSP_4_06");

	token_set("BRCM_BOARD", "bcm963xx");

	if (IS_HW("BCM96362"))
	{
	    token_set_y("CONFIG_HW_BCM96362");
	    if (!token_get("CONFIG_SDRAM_SIZE"))
		token_set("CONFIG_SDRAM_SIZE", "256");
	    token_set("CONFIG_HW_FLASH_NOR", "n");
	    token_set_y("CONFIG_HW_FLASH_NAND");
	    token_set_y("CONFIG_BOOTLDR_CFE");
	}
	else if (IS_HW("WVDB113G"))
	{
	    if (!token_get("CONFIG_SDRAM_SIZE"))
		token_set("CONFIG_SDRAM_SIZE", "64");
	    token_set_y("CONFIG_HW_FLASH_NOR");
	    mtd_physmap_add(0x18000000, 8 * 1024 * 1024, 2);
	}
	else if (IS_HW("YWZ00A"))
	{
	    token_set_y("CONFIG_HW_YWZ00A");
	    token_set("CONFIG_SDRAM_SIZE", "128");
	    token_set("CONFIG_HW_FLASH_NOR", "n");
	    token_set_y("CONFIG_HW_FLASH_SPI");
	    token_set_y("CONFIG_MTD_M25P80");
	}
	else if (IS_HW("YWZ00B") || IS_HW("HG558BZA") ||
	    IS_HW("BCM96362_VOX_1.5_IT") || IS_HW("BCM96362_VOX_1.5_ES") ||
	    IS_HW("BCM96362_VOX_1.5_PT") || IS_HW("AGC00A") ||
	    IS_HW("BCM96362_VOX_1.5_NZ"))
	{
	    if (IS_HW("YWZ00B") || IS_HW("BCM96362_VOX_1.5_IT") ||
	        IS_HW("AGC00A") || IS_HW("BCM96362_VOX_1.5_ES") ||
		IS_HW("BCM96362_VOX_1.5_PT") || IS_HW("BCM96362_VOX_1.5_NZ"))
	    {
		token_set_y("CONFIG_HW_YWZ00B");
	    }
	    if (IS_HW("HG558BZA") || IS_HW("BCM96362_VOX_1.5_IT"))
		token_set_y("CONFIG_HW_HG558");

	    token_set("CONFIG_SDRAM_SIZE", "128");
	    token_set("CONFIG_HW_FLASH_NOR", "n");
	    token_set_y("CONFIG_HW_FLASH_NAND");
	    token_set("CONFIG_HW_SWITCH_FIRST_PHY", "1");
	    token_set_y("CONFIG_BOOTLDR_CFE");

	    if (token_get("CONFIG_RG_RGLOADER"))
	    {
		/* CFE NVRAM partition table setup */
		token_set("CONFIG_CFE_NVRAM_NAND_BOOT_OFFSET_KB", "0x0");
		token_set("CONFIG_CFE_NVRAM_NAND_BOOT_SIZE_KB", "0x10");
		token_set("CONFIG_CFE_NVRAM_NAND_ROOTFS_1_OFFSET_KB", "0x10");
		token_set("CONFIG_CFE_NVRAM_NAND_ROOTFS_1_SIZE_KB", "0x7f0");
		token_set("CONFIG_CFE_NVRAM_NAND_ROOTFS_2_OFFSET_KB", "0x800");
		token_set("CONFIG_CFE_NVRAM_NAND_ROOTFS_2_SIZE_KB", "0x4c00");
		token_set("CONFIG_CFE_NVRAM_NAND_DATA_OFFSET_KB", "0x5400");
		token_set("CONFIG_CFE_NVRAM_NAND_DATA_SIZE_KB", "0x400");
		token_set("CONFIG_CFE_NVRAM_NAND_BBT_OFFSET_KB", "0xfc00");
		token_set("CONFIG_CFE_NVRAM_NAND_BBT_SIZE_KB", "0x400");
	    }
	}

	if (!token_get("CONFIG_HW_FLASH_NOR") && 
	    token_get("CONFIG_RG_MAINFS_IN_PHYS_MAPPED_FLASH"))
	{
	    rg_error(LEXIT, "CONFIG_RG_MAINFS_IN_PHYS_MAPPED_FLASH is not"
	        " possible on this board.\n");
	}

	if (!token_get("CONFIG_LSP_DIST"))
	{
	    if (token_get("CONFIG_HW_SWITCH_LAN"))
	    {
		token_set_m("CONFIG_HW_SWITCH_BCM9636X");
		if (IS_HW("BCM96362_VOX_1.5_PT"))
		{
		    token_set_y("CONFIG_BCM9636X_CASCADED_HW_SWITCH");
		    token_set_m("CONFIG_RG_HW_SWITCH_RTL836X");
		    token_set_y("CONFIG_RG_REALTEK_RTL8363SB");
		    switch_dev_add("bcmsw", NULL, 
			DEV_IF_RTL8363_BCM9636X_CASCADED_HW_SWITCH,
			DEV_IF_NET_INT, 8);
		}
		else
		{
		    switch_dev_add("bcmsw", NULL, DEV_IF_BCM9636X_HW_SWITCH,
			DEV_IF_NET_INT, 8);
		}
	    }
	    else if (token_get("CONFIG_HW_ETH_LAN"))
	    {
		if (!token_get("CONFIG_RG_RGLOADER"))
		    dev_add("bcmsw", DEV_IF_BCM9636X_ETH, DEV_IF_NET_INT);
		else
		{
		    dev_add_disabled("bcmsw", DEV_IF_BCM9636X_ETH,
			DEV_IF_NET_INT);
		}
	    }
	}

	if (token_get("CONFIG_HW_ETH_LAN") || token_get("CONFIG_HW_ETH_WAN"))
	    token_set_m("CONFIG_BCM_ENET");

	if (token_get("CONFIG_HW_LEDS"))
	    token_set_m("CONFIG_RG_KLEDS");

        if (token_get("CONFIG_RG_3G_WAN"))
	{
            gsm_dev_add("gsm0", DEV_IF_3G_USB_SERIAL, 1);
	    token_set_m("CONFIG_HUAWEI_CDC_ETHER");
	    token_set_y("CONFIG_RG_HUAWEI_CDC_ETHER_NCM");
	    dev_add("cdc_ether0", DEV_IF_HUAWEI_CDC_ETHER, DEV_IF_NET_EXT);
	    dev_set_route_group("cdc_ether0", DEV_ROUTE_GROUP_MAIN);
	    if (token_get("CONFIG_RG_VODAFONE_3G_VOICE_OVER_PS"))
		gsm_dev_add("gsm1", DEV_IF_3G_USB_SERIAL, 2);
	}

	if (token_get("CONFIG_HW_FLASH_NAND"))
	{
	    char *buf = NULL;

	    /* Tightly coupled with CONFIG_SDRAM_SIZE.
	     * RGloader needs private mem to place the openrg image prior 
	     * booting; it is located right after the memory seen by the 
	     * rgloader kernel. Must also defined at the openrg dist (the 
	     * openrg image opener executabe should be linked to this address).
	     *
	     *  - Assuming SDRAM is more than 32M
	     *  - The last 32mb of memory are reserved for extracting the image.
	     */
	    str_printf(&buf, "0x%08x",
		(token_get("CONFIG_SDRAM_SIZE") - 32) * 1024 * 1024);
	    token_set("CONFIG_RGLOADER_RESERVED_RAM_START", buf);
	    str_free(&buf);

	    if (token_get("CONFIG_RG_PERM_STORAGE_UBI"))
	    {
		/* Use 3% spare for bad eraseblocks */
		token_set("CONFIG_MTD_UBI_BEB_RESERVE", "3");
	    }

	    token_set("CONFIG_HW_NAND_PEB_SIZE", "0x4000"); /* 16KiB */
	    token_set("CONFIG_HW_NAND_LEB_SIZE", "0x3C00"); /* 15KiB*/
	    token_set("CONFIG_HW_NAND_MAX_BAD_BLOCKS", "80");
	    token_set("CONFIG_HW_NAND_MIN_IO_SIZE", "512");
	    token_set("CONFIG_HW_NAND_SUB_PAGE_SIZE", "512");
	}

	if (token_get("CONFIG_HW_TOUCH_BUTTONS"))
	    touch_buttons_configure();

	if (token_get("CONFIG_HW_USB_STORAGE"))
        {
	    token_set_y("CONFIG_USB");
	    token_set_y("CONFIG_USB_DEVICEFS");
        }

	if (token_get("CONFIG_HW_LEDS") || token_get("CONFIG_RG_VODAFONE_LCD"))
	    token_set_y("CONFIG_RG_UIEVENTS");

	if (token_get("CONFIG_HW_LCD"))
	    lcd_configure();

	if (token_get("CONFIG_HW_DSP"))
	{
	    if (token_get("CONFIG_SMP"))
		token_set_y("CONFIG_BCM_BRCM_VOICE_NONDIST");
	    else
		token_set_m("CONFIG_BCM_BCMDSP");

	    token_set_m("CONFIG_BCM_ENDPOINT");
	    token_set_y("CONFIG_RG_DSP_THREAD");
	    if (token_get("CONFIG_BCM96362"))
	    {
		token_set_y("BRCM_6362_UNI");
		token_set("CONFIG_HW_NUMBER_OF_FXS_PORTS", "2");
	    }
	}

	if (token_get("CONFIG_HW_DSL_WAN"))
	{
	    token_set_y("CONFIG_ATM");
	    token_set_y("CONFIG_RG_ATM_QOS");
	    dev_add("bcm_dsl0", DEV_IF_BCM9636X_DSL, DEV_IF_NET_EXT);
	    dsl_atm_dev_add("bcm_atm0", DEV_IF_BCM9636X_ATM, "bcm_dsl0");
	}

	if (token_get("CONFIG_HW_BCM9636X_WLAN"))
	    bcm9636x_add("wl0");
    
	if (token_get("CONFIG_HW_ENCRYPTION"))
	    token_set_m("CONFIG_BCM_SPU");

	if (token_get("CONFIG_RG_HW_WATCHDOG"))
	{
	    token_set_m("CONFIG_RG_BCM9636X_WATCHDOG");
	    token_set_y("CONFIG_RG_HW_WATCHDOG_ARCH_LINUX_GENERIC");
	}

	if (IS_HW("BCM96362_VOX_1.5_ES"))
	{
	    token_set_y("CONFIG_RG_VOICE_REINJECTION");
            token_set("CONFIG_RG_VOICE_REINJECTION_RELAY_GPIO", "5");
	}

	if (IS_HW("BCM96362_VOX_1.5_ES") || IS_HW("AGC00A"))
	    token_set_y("CONFIG_RG_MCU_LEDS");
    }

    if (IS_HW("BCM6358"))
    {
	token_set_y("CONFIG_BCM63XX");
	token_set_y("CONFIG_BCM63XX_CPU_6358");
	token_set_y("CONFIG_BOARD_BCM963XX");
	token_set_y("CONFIG_SERIAL_CORE");
	token_set_y("CONFIG_SERIAL_CORE_CONSOLE");
	token_set_y("CONFIG_SERIAL_BCM63XX");
	token_set_y("CONFIG_SERIAL_BCM63XX_CONSOLE");	

	token_set("CONFIG_SDRAM_SIZE", "32");
	token_set("CONFIG_RG_FLASH_LAYOUT_SIZE", "8");

	mtd_physmap_add(0x1e000000,
	    token_get("CONFIG_RG_FLASH_LAYOUT_SIZE") * 1024 * 1024, 2);

	if (token_get("CONFIG_HW_ETH_LAN"))
	{
	    token_set_y("CONFIG_ETHERNET");
	    token_set_y("CONFIG_MII");
	    token_set_y("CONFIG_PHYLIB");
	    token_set_y("CONFIG_NET_VENDOR_BROADCOM");	    
	    token_set_y("CONFIG_BCM63XX_ENET");
	    token_set_y("CONFIG_BCM63XX_PHY");	    

	    /* MAC1 is connected to LAN Eth (switch ) */
	    dev_add("eth1", DEV_IF_BCM6358_ETH, DEV_IF_NET_INT);
	    if (token_get("CONFIG_HW_ETH_WAN"))
		dev_add("eth0", DEV_IF_BCM6358_ETH, DEV_IF_NET_EXT);
	}
    }

    if (IS_HW("BCM96358") || IS_HW("DWV_BCM96358") || IS_HW("HH1620") ||
	IS_HW("CT6382D") || IS_HW("HG55VDFA") || IS_HW("HG55MAGV"))
    {
	long start;
	token_set_y("CONFIG_BCM963XX_COMMON");
	token_set_y("CONFIG_BCM96358");
	token_set_y("CONFIG_SERIAL_CORE");
	token_set_y("CONFIG_SERIAL_CORE_CONSOLE");
	token_set_y("CONFIG_BCM963XX_SERIAL");
	token_set_y("CONFIG_BCM96358_BOARD");

	if (IS_HW("BCM96358"))
	    token_set("CONFIG_BCM963XX_BOARD_ID", "96358GW");
	else if (IS_HW("DWV_BCM96358") || IS_HW("HG55VDFA") )
	{
	    token_set_y("CONFIG_HW_DWV_96358");
	    token_set("CONFIG_BCM963XX_BOARD_ID", "DWV-S0");
	}
	else if (IS_HW("HG55MAGV"))
	{
	    token_set_y("CONFIG_HW_HG55MAGV");
	    token_set("CONFIG_BCM963XX_BOARD_ID", "HW553");
	}
	else if (IS_HW("HH1620"))
	{
	    token_set_y("CONFIG_HW_HH1620");
	    token_set("CONFIG_BCM963XX_BOARD_ID", "96358VW");
	}
	else if (IS_HW("CT6382D"))
	{
	    token_set_y("CONFIG_HW_CT6382D");
	    token_set("CONFIG_BCM963XX_BOARD_ID", "CT6382D");
	}

	token_set("CONFIG_BCM963XX_SDRAM_SIZE", "32");

	if (IS_HW("HG55VDFA") || IS_HW("HG55MAGV"))
	{
	    token_set("CONFIG_BCM963XX_SDRAM_SIZE", "64");
	    token_set("CONFIG_RG_FLASH_LAYOUT_SIZE", "16");
//	    token_set("CONFIG_HW_NUMBER_OF_FXS_PORTS", "2");
	    start = 0x1e000000;
	}
	else if (IS_HW("DWV_BCM96358") || IS_HW("HH1620") || IS_HW("CT6382D"))
	{
	    token_set("CONFIG_RG_FLASH_LAYOUT_SIZE", "8");
	    token_set("CONFIG_HW_NUMBER_OF_FXS_PORTS", "2");
	    /* this value is taken from 
	     * vendor/broadcom/bcm963xx/linux-2.6/bcmdrivers/opensource/include/bcm963xx/board.h*/
	    start = 0x1e000000;
	}
	else
	{
	    /* this value is taken from 
	     * vendor/broadcom/bcm963xx/linux-2.6/bcmdrivers/opensource/include/bcm963xx/board.h*/
	    token_set("CONFIG_RG_FLASH_LAYOUT_SIZE", "4");
	    start = 0x1fc00000;
	}

	mtd_physmap_add(start, 
	    token_get("CONFIG_RG_FLASH_LAYOUT_SIZE") * 1024 * 1024, 2);

	token_set("CONFIG_BCM963XX_CHIP", "6358");

	if (token_get("CONFIG_HW_DSL_WAN"))
	{
	    token_set_y("CONFIG_ATM");
	    token_set_m("CONFIG_BCM963XX_ADSL");
	    token_set_m("CONFIG_BCM963XX_ATM");
	    token_set_y("CONFIG_RG_ATM_QOS");
	    dsl_atm_dev_add("bcm_atm0", DEV_IF_BCM963XX_ADSL, NULL);
	}

	if (token_get("CONFIG_HW_ETH_WAN") &&
	    !token_get("CONFIG_HW_SWITCH_PORT_SEPARATION"))
	{
	    if (token_get("CONFIG_HW_ETH_LAN"))
		conf_err("Ethernet device 'bcm0' cannot be WAN and LAN device");
	    token_set_m("CONFIG_BCM963XX_ETH");
	    dev_add("bcm0", DEV_IF_BCM963XX_ETH, DEV_IF_NET_EXT);
	}

	if (token_get("CONFIG_HW_ETH_LAN"))
	{
	    dev_add("bcm0", DEV_IF_BCM963XX_ETH, DEV_IF_NET_INT);

	    if (!token_get("CONFIG_HW_SWITCH_LAN"))
	    {
		token_set_m("CONFIG_BCM963XX_ETH");
		switch_dev_add("bcm1", NULL, DEV_IF_BCM963XX_ETH,
		    DEV_IF_NET_INT, -1);
	    }
	}

	if (token_get("CONFIG_HW_SWITCH_LAN"))
	{
	    token_set_m("CONFIG_HW_SWITCH_BCM53XX");
	    token_set_m("CONFIG_BCM963XX_ETH");
	    if (token_get("CONFIG_HW_SWITCH_PORT_SEPARATION"))
	    {
		dev_add("bcm1", DEV_IF_BCM963XX_ETH, DEV_IF_NET_INT);
		switch_dev_add("bcm1_main", "bcm1", DEV_IF_BCM5325E_HW_SWITCH,
		    DEV_IF_NET_INT, 5);
	    }
	    else
	    {
		switch_dev_add("bcm1", NULL, DEV_IF_BCM5325E_HW_SWITCH,
		    DEV_IF_NET_INT, 5);
	    }
	}

	if (token_get("CONFIG_HW_80211G_BCM43XX"))
	{
	    token_set_y("CONFIG_BCM4318");
	    bcm43xx_add("wl0");
	}

	if (token_get("CONFIG_RG_ATHEROS_HW_AR5212") ||
	    token_get("CONFIG_RG_ATHEROS_HW_AR5416"))
	{
	    token_set("CONFIG_ATHEROS_AR5008_PCI_SWAP", "0");
	    atheros_ar5xxx_add();
	}

        if (token_get("CONFIG_RG_3G_WAN"))
            gsm_dev_add("gsm0", DEV_IF_3G_USB_SERIAL, 1);

	if (token_get("CONFIG_HW_USB_RNDIS"))
	{
	    token_set_y("CONFIG_USB_RNDIS");
	    token_set_m("CONFIG_BCM963XX_USB");
	    dev_add("usb0", DEV_IF_USB_RNDIS, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_DSP"))
	{
	    token_set_m("CONFIG_BCM963XX_DSP");
	    token_set_m("CONFIG_BCM_ENDPOINT");
	    token_set("CONFIG_HW_NUMBER_OF_FXS_PORTS", "2");
	    token_set("CONFIG_BCM963XX_SLICTYPE", (IS_HW("CT6382D") ||
		IS_HW("HG55VDFA") || IS_HW("HG55MAGV")) ? "LE88221" :
		"SI3215");
	}
	if (token_get("CONFIG_HW_FXO"))
	{
	    token_set_y("CONFIG_BCM963XX_FXO");
	    token_set("CONFIG_HW_NUMBER_OF_FXO_PORTS", "1");
	}

	token_set_y("CONFIG_PCI");
	token_set_y("CONFIG_NEW_PCI");
	token_set_y("CONFIG_PCI_AUTO");
	if (token_get("CONFIG_HW_LEDS"))
	{
	    token_set_y("CONFIG_RG_UIEVENTS");
	    token_set_m("CONFIG_RG_KLEDS");
	}

	if (IS_HW("HH1620"))
	{
	    token_set("BOARD", "HH1620");
	    token_set("FIRM", "Motorola/Netopia");
	}
	else
	{
	    token_set("BOARD", "BCM96358");
	    token_set("FIRM", "Broadcom");
	}

    }

    if (IS_HW("BB-ROUTER") || IS_HW("FE-ROUTER") || 
	IS_HW("PACKET-IAD"))
    {

	if (token_get("CONFIG_HW_ETH_LAN")) 
	    dev_add("eth2", DEV_IF_COMCERTO_ETH, DEV_IF_NET_INT); 
	if (token_get("CONFIG_HW_ETH_WAN")) 
	    dev_add("eth0", DEV_IF_COMCERTO_ETH, DEV_IF_NET_EXT); 
	if (token_get("CONFIG_COMCERTO_VED_M821"))
	    dev_add("eth1", DEV_IF_COMCERTO_ETH_VED, DEV_IF_NET_INT);

	token_set_y("CONFIG_RG_ARCHINIT");
	token_set_y("CONFIG_NETFILTER");
	token_set("FIRM", "Mindspeed");

	token_set_y("CONFIG_CPU_CP15");
	token_set_y("CONFIG_CPU_CP15_MMU");

	if (IS_HW("PACKET-IAD"))
	{
	    token_set_y("CONFIG_COMCERTO_PACKET_IAD");
	    token_set_y("CONFIG_EVM_PACKET_IAD");
	    token_set("CONFIG_LOCALVERSION","Packet-IAD");
	    token_set("BOARD", "Packet-iad");
	    token_set("CONFIG_SDRAM_SIZE", "120");
	    if (token_get("CONFIG_HW_DSP"))
	    {
		if (token_get("CONFIG_RG_PSE_HW"))
		    token_set("CONFIG_HW_NUMBER_OF_FXS_PORTS", "2");
		else
		{
		    token_set_y("CONFIG_HW_FXO"); 
		    token_set("CONFIG_HW_NUMBER_OF_FXS_PORTS", "4");
		    token_set("CONFIG_HW_NUMBER_OF_FXO_PORTS", "1");
		}
	    }
	}
	else if (IS_HW("BB-ROUTER"))
	{
	    token_set_y("CONFIG_COMCERTO_BB_ROUTER");
	    token_set_y("CONFIG_EVM_ROUTER");
	    token_set("CONFIG_LOCALVERSION", "Giga-Router");
	    token_set("BOARD", "Giga-Router"); 
	    token_set("CONFIG_SDRAM_SIZE", "56");
	    if (token_get("CONFIG_HW_DSP"))
	    {
		if (token_get("CONFIG_RG_PSE_HW"))
		    token_set("CONFIG_HW_NUMBER_OF_FXS_PORTS", "2");
		else
		{
		    token_set_y("CONFIG_HW_FXO"); 
		    token_set("CONFIG_HW_NUMBER_OF_FXS_PORTS", "4");
		    token_set("CONFIG_HW_NUMBER_OF_FXO_PORTS", "1");
		}
	    }
	    /* CONFIG_MTD_PHYSMAP_START taken from NORFLASH_MEMORY_PHY1 in
	     * vendor/mindspeed/comcerto-100/kernel/arch-comcerto/boards/
	     * board-ferouter.h
	     */
	    mtd_physmap_add(0x20000000, 16 * 1024 * 1024, 2);
	}
	else if (IS_HW("FE-ROUTER"))
	{
	    token_set_y("CONFIG_COMCERTO_FE_ROUTER");
	    token_set_y("CONFIG_EVM_FEROUTER");
	    token_set("CONFIG_LOCALVERSION", "FE-Router");
	    token_set("BOARD", "FE-Router");
	    token_set("CONFIG_SDRAM_SIZE", "56");
	    if (token_get("CONFIG_RG_PSE_HW"))
		token_set("CONFIG_HW_NUMBER_OF_FXS_PORTS", "2");
	    else
	    {
		token_set_y("CONFIG_HW_FXO"); 
		token_set("CONFIG_HW_NUMBER_OF_FXS_PORTS", "2");
		token_set("CONFIG_HW_NUMBER_OF_FXO_PORTS", "1");   
	    }

	    /* CONFIG_MTD_PHYSMAP_START taken from NORFLASH_MEMORY_PHY1 in
	     * vendor/mindspeed/comcerto-100/kernel/arch-comcerto/boards/
	     * board-ferouter.h
	     */
	    mtd_physmap_add(0x20000000, 16 * 1024 * 1024, 2);
	}

	if (token_get("CONFIG_RG_PSE_HW"))
	{
	    token_set("CONFIG_RG_PSE_HW_PLAT_PATH",
		"vendor/mindspeed/comcerto-100/modules/fastpath");

	    token_set_m("CONFIG_COMCERTO_FPP");
	    token_set_y("CONFIG_COMCERTO_FPP_LOADER");
	    token_set("FPP_CODE_LOCATION", "/lib/modules/fpp.axf");
	    token_set("FPP_NODE_LOCATION", "/dev/fpp");
	    token_set_y("CONFIG_COMCERTO_FPP_TX_ZERO_COPY");
	    token_set_y("CONFIG_COMCERTO_FPP_RX_ZERO_COPY");
	}
	if (token_get("CONFIG_HW_ENCRYPTION"))
 	{
	    token_set_y("CONFIG_ELLIPTIC_IPSEC");
	    token_set_m("CONFIG_IPSEC_ALG_JOINT");
	    token_set_y("CONFIG_IPSEC_ALG_JOINT_ASYNC");
 	}

	if (token_get("CONFIG_HW_DSP"))
	{
	    token_set_y("CONFIG_SPI_LEGERITY");
	    token_set_y("CONFIG_COMCERTO_MATISSE");
	    token_set_y("CONFIG_COMCERTO_CAL");
	}

	if (token_get("CONFIG_HW_LEDS"))
	{
	    token_set_y("CONFIG_RG_UIEVENTS");
	    token_set_m("CONFIG_RG_KLEDS");
	}

	/* Atheros WiFi card */ 
	if (token_get("CONFIG_RG_ATHEROS_HW_AR5212") ||
	    token_get("CONFIG_RG_ATHEROS_HW_AR5416"))
	{
	    token_set("CONFIG_ATHEROS_AR5008_PCI_SWAP", "0");
	    atheros_ar5xxx_add();
	}

	/* Ralink WiFi card */ 
 	if (token_get("CONFIG_HW_80211G_RALINK_RT2560"))
	    ralink_rt2560_add("ra0"); 
 
	if (token_get("CONFIG_HW_80211G_RALINK_RT2561"))	   	
	    ralink_rt2561_add("ra0"); 

	if (token_get("CONFIG_HW_80211N_RALINK_RT2860"))	   	
	    ralink_rt2860_add("ra0"); 
    }

    if (IS_HW("PC_686"))
    {
	token_set("BOARD", "PC");
	token_set_y("CONFIG_RG_686");

	token_set("CONFIG_RG_FLASH_LAYOUT_SIZE", "64"); 

	token_set("CONFIG_RG_CONSOLE_DEVICE", "console");

	if (token_get("CONFIG_HW_ETH_LAN") || token_get("CONFIG_HW_ETH_WAN"))
	    token_set_y("CONFIG_E1000");

	if (!token_get("CONFIG_LSP_DIST"))
	{
	    if (token_get("CONFIG_HW_ETH_LAN"))
		dev_add("eth0", DEV_IF_EEPRO1000, DEV_IF_NET_INT);
	    if (token_get("CONFIG_HW_ETH_WAN"))
		dev_add("eth1", DEV_IF_EEPRO1000, DEV_IF_NET_EXT);
	}
    }

    if (IS_HW("UML"))
    {
	token_set("ARCH", "um");

	token_set("CONFIG_RG_FLASH_LAYOUT_SIZE", "64"); 
	if (token_get("CONFIG_HW_ETH_WAN"))
	    dev_add("eth0", DEV_IF_UML, DEV_IF_NET_EXT);
	if (token_get("CONFIG_HW_ETH_WAN2"))
	    dev_add("eth3", DEV_IF_UML, DEV_IF_NET_EXT);

	if (token_get("CONFIG_HW_DSL_WAN"))
	{
	    token_set_y("CONFIG_ATM_NULL");
	    dsl_atm_dev_add("atmnull0", DEV_IF_ATM_NULL, NULL);
	}

	if (token_get("CONFIG_HW_HSS_WAN"))
	    token_set_y("CONFIG_RG_HSS");

	if (token_get("CONFIG_HW_SWITCH_LAN"))
	{
	    dev_add_uml_hw_switch("hw_switch0", DEV_IF_NET_INT, -1, "eth4",
		"eth5", "eth6", "eth7", NULL);
	    /* Yep, a bridge under the bridge... */
	    dev_add_to_bridge("br0", "hw_switch0");
	}

	if (token_get("CONFIG_HW_ETH_LAN2"))
	{
	    dev_add("eth2", DEV_IF_UML, DEV_IF_NET_INT);
	    if (token_get("CONFIG_DEF_BRIDGE_LANS"))
		dev_add_to_bridge("br0", "eth2");
	}

	if (token_get("CONFIG_HW_ETH_LAN"))
	{
	    dev_add("eth1", DEV_IF_UML, DEV_IF_NET_INT);
	    if (token_get("CONFIG_DEF_BRIDGE_LANS"))
		dev_add_to_bridge("br0", "eth1");
	}

	if (token_get("CONFIG_HW_80211G_UML_WLAN"))
	{
	    wireless_vaps_add("uml_wlan0", DEV_IF_UML_WLAN, "uml_wlan%d", 8,
		NULL);
	}

	if (token_get("CONFIG_RG_TUTORIAL_ADVANCED"))
	{
	    dev_add("mydev0", DEV_IF_MYDEV, DEV_IF_NET_INT);
	    dev_can_be_missing("mydev0");
	    dev_add_to_bridge("br0", "mydev0");
	}

	if (token_get("CONFIG_UML_ROUTE_GROUPS"))
	{
	    dev_add("eth8", DEV_IF_UML, DEV_IF_NET_EXT); 
	    dev_add("eth9", DEV_IF_UML, DEV_IF_NET_EXT); 
	    dev_add("eth10", DEV_IF_UML, DEV_IF_NET_INT);
	    dev_add("eth11", DEV_IF_UML, DEV_IF_NET_INT);
	    dev_add("eth12", DEV_IF_UML, DEV_IF_NET_INT);
	    dev_add("eth13", DEV_IF_UML, DEV_IF_NET_INT);
	    dev_add("eth14", DEV_IF_UML, DEV_IF_NET_INT);
	    dev_add("eth15", DEV_IF_UML, DEV_IF_NET_EXT);
	    dev_add("eth16", DEV_IF_UML, DEV_IF_NET_EXT);
	    dev_add("eth17", DEV_IF_UML, DEV_IF_NET_EXT);
	    dev_add("eth18", DEV_IF_UML, DEV_IF_NET_EXT);

	    /* Setting three routing groups non-forwarding, demo1, demo2 */
	    dev_set_route_group("eth8", DEV_ROUTE_GROUP_NON_FORWARDING);
	    dev_set_route_group("eth9", DEV_ROUTE_GROUP_NON_FORWARDING);
	    dev_set_route_group("eth12", DEV_ROUTE_GROUP_DEMO1);
	    dev_set_route_group("eth13", DEV_ROUTE_GROUP_DEMO1);
	    dev_set_route_group("eth17", DEV_ROUTE_GROUP_DEMO1);
	    dev_set_route_group("eth14", DEV_ROUTE_GROUP_DEMO2);
	    dev_set_route_group("eth18", DEV_ROUTE_GROUP_DEMO2);
	}

	token_set_y("CONFIG_RG_UML");

	token_set_y("CONFIG_DEBUGSYM"); /* UML is always for debug ;-) */
	
	token_set("CONFIG_RG_CONSOLE_DEVICE", "console");

	token_set_y("CONFIG_RAMFS");
	token_set("CONFIG_KERNEL_STACK_ORDER", "2");
	token_set_y("CONFIG_MODE_SKAS");
	token_set("CONFIG_NEST_LEVEL", "0");
	token_set("CONFIG_CON_ZERO_CHAN", "fd:0,fd:1");
	token_set("CONFIG_CON_CHAN", "xterm");
	token_set("CONFIG_SSL_CHAN", "pty");
	token_set_y("CONFIG_STDIO_CONSOLE");
	token_set_y("CONFIG_SSL");
	token_set_y("CONFIG_FD_CHAN");
	token_set_y("CONFIG_NULL_CHAN");
	token_set_y("CONFIG_PORT_CHAN");
	token_set_y("CONFIG_PTY_CHAN");
	token_set_y("CONFIG_TTY_CHAN");
	token_set_y("CONFIG_XTERM_CHAN");
	token_set_y("CONFIG_BLK_DEV_NBD");

	token_set_y("CONFIG_UML_NET");
	token_set_y("CONFIG_UML_NET_ETHERTAP");
	token_set_y("CONFIG_UML_NET_TUNTAP");
	token_set_y("CONFIG_UML_NET_SLIP");
	token_set_y("CONFIG_UML_NET_SLIRP");
	token_set_y("CONFIG_UML_NET_DAEMON");
	token_set_y("CONFIG_UML_NET_MCAST");
	token_set_y("CONFIG_DUMMY");
	token_set_y("CONFIG_TUN");

	token_set("CONFIG_UML_RAM_SIZE",
	    token_get("CONFIG_VALGRIND") ? "192M" : "64M");

	token_set_y("CONFIG_USERMODE");
	token_set_y("CONFIG_UID16");
	token_set_y("CONFIG_EXPERIMENTAL");
	token_set_y("CONFIG_BSD_PROCESS_ACCT");
	token_set_y("CONFIG_HOSTFS");
	token_set_y("CONFIG_HPPFS");
	token_set_y("CONFIG_MCONSOLE");
	token_set_y("CONFIG_MAGIC_SYSRQ");

	token_set_y("CONFIG_PACKET_MMAP");
	token_set_y("CONFIG_QUOTA");
	token_set_y("CONFIG_AUTOFS_FS");
	token_set_y("CONFIG_AUTOFS4_FS");
	token_set_y("CONFIG_REISERFS_FS");

	if (token_get("CONFIG_HW_DSP"))
	{
	    token_set_y("CONFIG_HW_FXO");
	    token_set_y("CONFIG_RG_VOIP_HW_EMULATION");
	    token_set("CONFIG_HW_NUMBER_OF_FXS_PORTS", "8");
	    token_set("CONFIG_HW_NUMBER_OF_FXO_PORTS", "1");
	}
    }
    
    if (IS_HW("JPKG"))
    {
	if (token_get("CONFIG_HW_80211G_RALINK_RT2560"))
	    ralink_rt2560_add("ra0");

	if (token_get("CONFIG_HW_80211G_RALINK_RT2561"))
	    ralink_rt2561_add("ra0");

	if (token_get("CONFIG_HW_80211N_RALINK_RT2860"))
	    ralink_rt2860_add("ra0");

	if (token_get("CONFIG_HW_80211G_BCM43XX"))
	    bcm43xx_add(NULL);

	if (token_get("CONFIG_RG_ATHEROS_HW_AR5212") ||
	    token_get("CONFIG_RG_ATHEROS_HW_AR5416"))
	{
	    atheros_ar5xxx_add();
	}

	if (token_get("CONFIG_HW_80211N_AIRGO_AGN100"))
	    airgo_agn100_add();
    }
    
    if (token_get("CONFIG_RG_IPV6_OVER_IPV4_TUN"))
	dev_add("sit0", DEV_IF_IPV6_OVER_IPV4_TUN, DEV_IF_NET_INT);

    if (IS_HW("TNETC550W") || IS_HW("BVW3653") || IS_HW("MVG3420N"))
    {
	token_set_y("CONFIG_BOOTLDR_UBOOT");
	token_set("CONFIG_BOOTLDR_UBOOT_COMP", "gzip");
	token_set_y("CONFIG_AVALANCHE_U_BOOT");	

	token_set_y("CONFIG_AVALANCHE_COMMON");
	token_set_y("CONFIG_MACH_PUMA5");
	token_set_y("CONFIG_RG_ARM_FIXED_MACH_TYPE");
	if (IS_HW("TNETC550W"))
	    token_set_y("CONFIG_MACH_PUMA5EVM");
	else if(IS_HW("BVW3653"))
	    token_set_y("CONFIG_MACH_PUMA5_BVW3653");
	else if(IS_HW("MVG3420N"))
	    token_set_y("CONFIG_MACH_PUMA5_MVG3420N");

	if (token_get("CONFIG_HW_ETH_LAN"))
	{
	    token_set_y("CONFIG_MII");
	    token_set_y("CONFIG_ARM_AVALANCHE_CPGMAC_F");
	    token_set("CONFIG_ARM_CPMAC_PORTS", "1");
	    token_set_y("CONFIG_AVALANCHE_LOW_CPMAC");
	    token_set_y("CONFIG_ARM_CPMAC_DS_TRAFFIC_PRIORITY");
	    
	    dev_add("eth0", DEV_IF_AVALANCHE_CPGMAC_F_ETH, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_SWITCH_LAN"))
	{
	    token_set_y("CONFIG_MII");
	    token_set_y("CONFIG_ARM_AVALANCHE_CPGMAC_F");
	    token_set("CONFIG_ARM_CPMAC_PORTS", "1");
	    token_set_y("CONFIG_AVALANCHE_LOW_CPMAC");
	    if(IS_HW("BVW3653"))
	    {
		token_set("CONFIG_AR8316_CPU_PORT", "0");
		token_set("CONFIG_AR8316_WIRELESS_PORT", "5");
	    }
	    else if(IS_HW("MVG3420N"))
	    {
		token_set("CONFIG_AR8316_CPU_PORT", "5");
		token_set("CONFIG_AR8316_WIRELESS_PORT", "0");
	    }
	    token_set_y("CONFIG_AVALANCHE_SWITCH_PROMISCOUS");
	    if (token_get("CONFIG_HW_SWITCH_PORT_SEPARATION"))
	    {
		dev_add("eth0", DEV_IF_TI550_ETH, DEV_IF_NET_INT);
		switch_dev_add("eth0_main", "eth0", DEV_IF_AR8316_HW_SWITCH,
		    DEV_IF_NET_INT, token_get("CONFIG_AR8316_CPU_PORT"));
	    }
	    else
		dev_add("eth0", DEV_IF_AR8316_HW_SWITCH, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_80211N_RALINK_RT2880") ||
	    token_get("CONFIG_HW_80211N_RALINK_RT3052"))
	{
	    ralink_rt2880_add("ra0");
	    if (token_get("CONFIG_HW_80211N_RALINK_RT3052"))
		token_set_y("CONFIG_RALINK_RT3052"); 
	}
	if (token_get("CONFIG_RALINK_RT2880"))
	{
	    if (token_get("CONFIG_HW_SWITCH_PORT_SEPARATION"))
	    {
		dev_set_dependency("ra0", "eth0_main");
		token_set("CONFIG_RALINK_MII_DEF_RX_DEV", "\"eth0_main\"");
	    }
	    else
	    {
		dev_set_dependency("ra0", "eth0");
		token_set("CONFIG_RALINK_MII_DEF_RX_DEV", "\"eth0\"");
	    }
	}

	if (token_get("CONFIG_HW_CABLE_WAN"))
	{
	    dev_add("lbr0", DEV_IF_TI550_ETH_DOCSIS3, DEV_IF_NET_EXT);
	    dev_add("cni0", DEV_IF_TI550_CNI_DOCSIS3, DEV_IF_NET_EXT);
	    dev_set_dependency("lbr0", "cni0");
	}

	if (token_get("CONFIG_RG_PSE_HW"))
	{
	    if (token_get("CONFIG_HW_SWITCH_PORT_SEPARATION"))
	    {
		conf_err("Fastpath can't be enabled when switch port "
		    "separation is used (see B85161)\n");
	    }
	    token_set_y("CONFIG_TI_PACKET_PROCESSOR");
	    token_set("CONFIG_RG_PSE_HW_PLAT_PATH",
		"vendor/ti/avalanche/rg/modules");
	}
	
	if (token_get("CONFIG_HW_USB_STORAGE"))
	{
	    token_set_y("CONFIG_USB");
	    token_set_y("CONFIG_USB_DEVICEFS");
	    token_set_m("CONFIG_USB_MUSB_HDRC");
	    token_set_y("CONFIG_USB_MUSB_SOC");
	    token_set_y("CONFIG_USB_MUSB_HOST");
	    token_set_y("CONFIG_USB_MUSB_HDRC_HCD");
	    token_set_y("CONFIG_USB_INVENTRA_FIFO");

	    /* debug flag */
	    token_set("CONFIG_USB_INVENTRA_HCD_LOGGING", "0");
	}

	if (token_get("CONFIG_HW_LEDS"))
	    token_set_y("CONFIG_RG_UIEVENTS");

	token_set("CONFIG_SDRAM_SIZE", "64");

	if (IS_HW("TNETC550W"))
	{
	    token_set("BOARD", "TNETC550W");
	    token_set("FIRM", "Texas Instruments");
	}
	else if(IS_HW("BVW3653"))
	{
	    token_set("BOARD", "BVW3653");
	    token_set("FIRM", "Hitron");
	    token_set_y("CONFIG_RG_UIEVENTS");
	}
	else if(IS_HW("MVG3420N"))
	{
	    token_set("BOARD", "MVG3420N");
	    token_set("FIRM", "Mototech");
	}
    }
    

    if (!token_get_str("CONFIG_RG_MAINFS_MTDPART_NAME"))
	token_set("CONFIG_RG_MAINFS_MTDPART_NAME", "\"mainfs\"");
}

