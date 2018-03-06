/*
<:label-BRCM:2011:proprietary:standard

 This program is the proprietary software of Broadcom Corporation and/or its
 licensors, and may only be used, duplicated, modified or distributed pursuant
 to the terms and conditions of a separate, written license agreement executed
 between you and Broadcom (an "Authorized License").  Except as set forth in
 an Authorized License, Broadcom grants no license (express or implied), right
 to use, or waiver of any kind with respect to the Software, and Broadcom
 expressly reserves all rights in and to the Software and all intellectual
 property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU HAVE
 NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY NOTIFY
 BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.

 Except as expressly set forth in the Authorized License,

 1. This program, including its structure, sequence and organization,
    constitutes the valuable trade secrets of Broadcom, and you shall use
    all reasonable efforts to protect the confidentiality thereof, and to
    use this information only in connection with your use of Broadcom
    integrated circuit products.

 2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
    AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR
    WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH
    RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND
    ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY, NONINFRINGEMENT,
    FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES, ACCURACY OR
    COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR CORRESPONDENCE
    TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF USE OR
    PERFORMANCE OF THE SOFTWARE.

 3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR
    ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL,
    INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY
    WAY RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN
    IF BROADCOM HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES;
    OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE
    SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE LIMITATIONS
    SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF ANY
    LIMITED REMEDY.
:> 

*/

#ifndef __ROBOSW_REG_H
#define __ROBOSW_REG_H

void robosw_init(void);
void robosw_configure_ports(void);
void robosw_check_ports(void);

// These macros make offset validation vs. data sheet easier.

#define group(type, name, start_offset, next) type name[(next - start_offset) / sizeof(type)]
#define entry(type, name, start_offset, next) type name

typedef struct RoboSwitch {
  group(byte,     PortCtrl,     0x0000, 0x0009);
  group(byte,     Reserved0009, 0x0009, 0x000b);
  entry(byte,     SwitchMode,   0x000b, 0x000c);
  entry(uint16,   PauseQuanta,  0x000c, 0x000e);
  entry(byte,     ImpOverride,  0x000e, 0x000f);
  entry(byte,     LedRefresh,   0x000f, 0x0010);
  entry(uint16,   LedFunc0,     0x0010, 0x0012);
  entry(uint16,   LedFunc1,     0x0012, 0x0014);
  entry(uint16,   LedFuncMap,   0x0014, 0x0016);
  entry(uint16,   LedEnableMap, 0x0016, 0x0018);
  entry(uint16,   LedMap0,      0x0018, 0x001a);
  entry(uint16,   LedMap1,      0x001a, 0x001c);
  group(byte,     Reserved001c, 0x001c, 0x0020);
  group(byte,     Reserved0020, 0x0020, 0x0021);
  entry(byte,     ForwardCtrl,  0x0021, 0x0022);
  group(byte,     Reserved0022, 0x0022, 0x0024);
  entry(uint16,   ProtSelect,   0x0024, 0x0026);
  entry(uint16,   WanSelect,    0x0026, 0x0028);
  entry(uint32,   PauseCap,     0x0028, 0x002c);
  group(byte,     Reserved002c, 0x002c, 0x002f);
  entry(byte,     MultiCtrl,    0x002f, 0x0030);
  group(byte,     Reserved0030, 0x0030, 0x0031);
  entry(byte,     TxqFlush,     0x0031, 0x0032);
  entry(uint16,   UniFail,      0x0032, 0x0034);
  entry(uint16,   MultiFail,    0x0034, 0x0036);
  entry(uint16,   MlfIpmc,      0x0036, 0x0038);
  entry(uint16,   PausePassRx,  0x0038, 0x003a);
  entry(uint16,   PausePassTx,  0x003a, 0x003c);
  entry(uint16,   DisableLearn, 0x003c, 0x003e);
  group(byte,     Reserved003e, 0x003e, 0x004a);
  entry(uint16,   PllTest,      0x004a, 0x004c);
  group(byte,     Reserved004c, 0x004c, 0x0058);
  group(byte,     PortOverride, 0x0058, 0x0060);
  group(byte,	  Reserved0061, 0x0060, 0x0064);
  entry(byte,     ImpRgmiiCtrlP4, 0x0064, 0x0065);
  entry(byte,	  ImpRgmiiCtrlP5, 0x0065, 0x0066);
  entry(byte,     ImpRgmiiCtrlP6, 0x0066, 0x0067);
  entry(byte,	  ImpRgmiiCtrlP7, 0x0067, 0x0068);
  group(byte,     Reserved0068, 0x0068, 0x006c);
  entry(byte,     ImpRgmiiTimingDelayP4, 0x006c, 0x006d);
  entry(byte,	  ImpRgmiiTimingDelayP5, 0x006d, 0x006e);
  entry(byte,     ImpRgmiiTimingDelayP6, 0x006e, 0x006f);
  entry(byte,	  ImpRgmiiTimingDelayP7, 0x006f, 0x0070);
  group(byte,     Reserved0070, 0x0070, 0x0079);
  entry(byte,     SWResetCtrl,  0x0079, 0x007a);
  group(byte,     Reserved007a, 0x007a, 0x0090);
  entry(uint32,   Rxfilt_Ctl,   0x0090, 0x0094);
  entry(uint32,   Cmf_En_Ctl,   0x0094, 0x0098);
  group(byte,     Reserved0098, 0x0098, 0x00a0);
  entry(uint32,   SwpktCtrl0,   0x00a0, 0x00a4);
  entry(uint32,   SwpktCtrl1,   0x00a4, 0x00a8);
  group(byte,     Reserved00a8, 0x00a8, 0x00b0);
  entry(uint32,   MdioCtrl,     0x00b0, 0x00b4);
  entry(uint16,   MdioData,     0x00b4, 0x00b6);
  group(byte,     Reserved00b4, 0x00b6, 0x0200);
  entry(byte,     GlbMgmt,      0x0200, 0x0201);
  entry(byte,     ChpBoxID,     0x0201, 0x0202);
  entry(byte,     MngPID,       0x0202, 0x0203);
  group(byte,     Reserved0203, 0x0203, 0x1100);
  entry(uint16,   MIICtrl0,     0x1100, 0x1101);

} RoboSwitch;

#define SWITCH ((volatile RoboSwitch * const)SWITCH_BASE)
#define PBVLAN_OFFSET 0x3100
#define PBMAP_MIPS 0x100
#define SWITCH_PBVLAN ((volatile uint16 * const)(SWITCH_BASE + PBVLAN_OFFSET))

#define PortCtrl_Forwarding   0xa0
#define PortCtrl_Learning     0x80
#define PortCtrl_Listening    0x60
#define PortCtrl_Blocking     0x40
#define PortCtrl_Disable      0x20
#define PortCtrl_RxUcstEn     0x10
#define PortCtrl_RxMcstEn     0x08
#define PortCtrl_RxBcstEn     0x04
#define PortCtrl_DisableTx    0x02
#define PortCtrl_DisableRx    0x01

#define SwitchMode_FwdgEn     0x02
#define SwitchMode_ManageMode 0x01

#define ImpOverride_Force     0x80
#define ImpOverride_TxFlow    0x20
#define ImpOverrode_RxFlow    0x10
#define ImpOverride_1000Mbs   0x08
#define ImpOverride_100Mbs    0x04
#define ImpOverride_10Mbs     0x00
#define ImpOverride_Fdx       0x02
#define ImpOverride_Linkup    0x01

#define PortOverride_Enable   0x40
#define PortOverride_TxFlow   0x20
#define PortOverride_RxFlow   0x10
#define PortOverride_1000Mbs  0x08
#define PortOverride_100Mbs   0x04
#define PortOverride_10Mbs    0x00
#define PortOverride_Fdx      0x02
#define PortOverride_Linkup   0x01

#define GlbMgmt_EnableImp     0x80
#define GlbMgmt_IgmpSnooping  0x08
#define GlbMgmt_ReceiveBpdu   0x02
#define GlbMgmt_ResetMib      0x01

#define MdioCtrl_Write        (1 << 31)
#define MdioCtrl_Read         (1 << 30)
#define MdioCtrl_Ext          (1 << 16)
#define MdioCtrl_ID_Shift     25
#define MdioCtrl_ID_Mask      (0x1f << MdioCtrl_ID_Shift)
#define MdioCtrl_Addr_Shift   20
#define MdioCtrl_Addr_Mask    (0x1f << MdioCtrl_Addr_Shift)

#define ImpRgmiiCtrl_GMII_En        0x80
#define ImpRgmiiCtrl_Force_RGMII    0x40
#define ImpRgmiiCtrl_Mode_RGMII     0x00
#define ImpRgmiiCtrl_Mode_RvMII     0x20
#define ImpRgmiiCtrl_Mode_MII       0x10
#define ImpRgmiiCtrl_Mode_GMII      0x30
#define ImpRgmiiCtrl_Mode_Mask      0x30
#define ImpRgmiiCtrl_DLL_IQQD       0x04
#define ImpRgmiiCtrl_DLL_RXC_Bypass 0x02
#define ImpRgmiiCtrl_Timing_Sel     0x01

#define ImpRgmiiTimingDelayDefault  0xF9

#define PORT_4_PORT_ID  4
#define PORT_5_PORT_ID  5
#define PORT_6_PORT_ID  6
#define PORT_7_PORT_ID  7

#if defined (_BCM96328_)
#define EPHY_PORTS      5
#endif
#if defined (_BCM96362_)
#define EPHY_PORTS      6
#endif
#if defined (_BCM963268_)
#define EPHY_PORTS      8
#define GPHY_PORT_ID    3
#endif
#if defined (_BCM96828_)
#define EPHY_PORTS      8
#define GPHY1_PORT_ID   2
#define GPHY2_PORT_ID   3
#endif
#if defined (_BCM96368_)
#define EPHY_PORTS      6
#define USB_PORT_ID     6
#define SAR_PORT_ID     7
#endif
#if defined (_BCM96816_)
#define EPHY_PORTS      4
#define SERDES_PORT_ID  4
#define MOCA_PORT_ID    5
#define USB_PORT_ID     6
#define GPON_PORT_ID    7
#endif
#if defined (_BCM96818_)
#define EPHY_PORTS      7
#define GPHY0_PORT_ID   0
#define GPHY1_PORT_ID   1
#define SERDES_PORT_ID  4
#define USB_PORT_ID     6
#define GPON_PORT_ID    7
#endif
#if defined (_BCM96318_)
#define EPHY_PORTS      5 /* Includes 4 internal ephy ports and 1 rgmii port */
#endif


#endif
