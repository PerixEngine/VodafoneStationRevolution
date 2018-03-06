/*
<:copyright-broadcom 
 
 Copyright (c) 2007 Broadcom Corporation 
 All Rights Reserved 
 No portions of this material may be reproduced in any form without the 
 written permission of: 
          Broadcom Corporation 
          16215 Alton Parkway 
          Irvine, California 92619 
 All information contained in this document is Broadcom Corporation 
 company private, proprietary, and trade secret. 
 
:>
*/
/***********************************************************************/
/*                                                                     */
/*   MODULE:  6362_common.h                                             */
/*   DATE:    02/01/08                                                 */
/*   PURPOSE: Define addresses of major hardware components of         */
/*            BCM6362                                                  */
/*                                                                     */
/***********************************************************************/
#ifndef __BCM6362_MAP_COMMON_H
#define __BCM6362_MAP_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__KERNEL__) || defined (_CFE_) || defined(FAP_4KE)
/* Access to registers from kernelspace */
#define REG_BASE                 0xb0000000
#else
/* Access to registers from userspace, see bcm_mmap.h for api */
#define REG_BASE                 (bcm_mmap_info.mmap_addr)
#define BCM_MMAP_INFO_BASE       0x10000000
#define BCM_MMAP_INFO_SIZE       0x01000000
#endif

#define PERF_BASE                (REG_BASE + 0x00000000)  /* chip control registers */
#define TIMR_BASE                (REG_BASE + 0x00000040)  /* timer registers */
#define NAND_INTR_BASE           (REG_BASE + 0x00000070)  /* nand interrupt control registers */
#define GPIO_BASE                (REG_BASE + 0x00000080)  /* gpio registers */
#define UART_BASE                (REG_BASE + 0x00000100)  /* uart registers */
#define UART1_BASE               (REG_BASE + 0x00000120)  /* uart registers */
#define NAND_REG_BASE            (REG_BASE + 0x00000200)
#define OTP_BASE                 (REG_BASE + 0x00000400)
#define UBUS_STAT_BASE           (REG_BASE + 0x00000500)
#define NAND_CACHE_BASE          (REG_BASE + 0x00000600)
#define SPI_BASE                 (REG_BASE + 0x00000800)  /* SPI master controller registers */
#define HSSPIM_BASE              (REG_BASE + 0x00001000)  /* High-Speed SPI registers */
#define ADSL_CTRL_BASE           (REG_BASE + 0x00001800)
#define MISC_BASE                (REG_BASE + 0x00001800)  /* Miscellaneous Registers */
#define LED_BASE                 (REG_BASE + 0x00001900)  /* LED control registers */
#define USB_CTL_BASE             (REG_BASE + 0x00002400)  /* USB 2.0 device control registers */
#ifdef __KERNEL__
#define USB_EHCI_BASE                        0x10002500   /* USB host registers */
#define USB_OHCI_BASE                        0x10002600   /* USB host registers */
#endif
#define USBH_CFG_BASE            (REG_BASE + 0x00002700)
#define IPSEC_BASE               (REG_BASE + 0x00002800)
#define DDR_BASE                 (REG_BASE + 0x00003000)  /* Memory control registers */
#ifdef __KERNEL__
#define WLAN_CHIPC_BASE                      0x10004000   /* WLAN ChipCommon registers, use 1xxx for ioremap */
#define WLAN_MAC_BASE                        0x10005000   /* WLAN d11mac registers */
#endif
#define WLAN_SHIM_BASE           (REG_BASE + 0x00007000)  /* shim interface to WLAN */
#define SAR_BASE                 (REG_BASE + 0x00007800)
#define PCM_BASE                 (REG_BASE + 0x0000a000)
#define PCM_DMA_BASE             (REG_BASE + 0x0000a800)  /* PCM UIDMA register base */
#define SAR_DMA_BASE             (REG_BASE + 0x0000b800)  /* ATM SAR DMA control registers */
#define USB_DMA_BASE             (REG_BASE + 0x0000c000)  /* USB 2.0 device DMA regiseters */
#define SWITCH_DMA_BASE          (REG_BASE + 0x0000d800)
#define SWITCH_BASE              (REG_BASE + 0x00e00000)
#define PCIE_BASE                (REG_BASE + 0x00e40000)
#define DECT_SHIM_CTRL_BASE      (REG_BASE + 0x0000b000)
#define DECT_SHIM_DMA_CTRL_BASE  (REG_BASE + 0x0000b050)
#define DECT_SHIM_TEST_BASE      (REG_BASE + 0x0000b0f0)
#define DECT_APB_REG_BASE        (REG_BASE + 0x0000e000)
#define DECT_AHB_SHARED_RAM_BASE (REG_BASE + 0x00e50000)
#define DECT_AHB_REG_BASE        (REG_BASE + 0x00e57f80)


/* FAP_TBD: ensure this is in sync with map_part.h */
#define FAP_PSM_BASE                  (REG_BASE + 0x01010000)
#define FAP_QSM_UBUS_BASE             (REG_BASE + 0x01004000)
#define FAP_QSM_SMI_BASE              (REG_BASE + 0x04004000)



/*
#####################################################################
# System PLL Control Register
#####################################################################
*/

/*
#####################################################################
# GPIO Control Registers
#####################################################################
*/
#define GPIO_DATA_HI               (GPIO_BASE + 0x8)
#define GPIO_DATA                  (GPIO_BASE + 0xC)


/*
#####################################################################
# Miscellaneous Registers
#####################################################################
*/
#define MISC_MEMC_CONTROL		0x10
#define MISC_STRAP_BUS			0x14
#define MISC_STRAP_BUS_MIPS_PLL_FVCO_SHIFT      1
#define MISC_STRAP_BUS_MIPS_PLL_FVCO_MASK       (0x1F<<MISC_STRAP_BUS_MIPS_PLL_FVCO_SHIFT)

#define MISC_VREG_CONTROL0		0x1C
#define MISC_VREG_CONTROL0_VREG_ADJ_SHIFT       8
#define MISC_VREG_CONTROL0_VREG_OSC1P2_SHIFT	20
#define MISC_VREG_CONTROL0_VREG_OSC1P8_SHIFT	22
#define MISC_VREG_CONTROL0_VREG_RAMP1P2_SHIFT	26
#define MISC_VREG_CONTROL0_VREG_RAMP1P8_SHIFT	29
    
#define MISC_VREG_CONTROL1		0x20
#define MISC_VREG_CONTROL1_VREG_ISEL2P5_SHIFT   13
#define MISC_VREG_CONTROL1_VREG_ISEL2P5_MASK    0x0001e000
#define MISC_VREG_LDO_2P61       			    1

#define MISC_VREG_CONTROL2		0x24

/*
#####################################################################
# Memory Control Registers
#####################################################################
*/
#define DDR_CTL_CNFG                             0x000 
#define DDR_CTL_CSST                             0x004 
#define DDR_CTL_CSEND                            0x008 
#define DDR_CTL_ROW00_0                          0x010 
#define DDR_CTL_ROW00_1                          0x014 
#define DDR_CTL_ROW01_0                          0x018 
#define DDR_CTL_ROW01_1                          0x01c 
#define DDR_CTL_ROW20_0                          0x030 
#define DDR_CTL_ROW20_1                          0x034 
#define DDR_CTL_ROW21_0                          0x038 
#define DDR_CTL_ROW21_1                          0x03c 
#define DDR_CTL_COL00_0                          0x050 
#define DDR_CTL_COL00_1                          0x054 
#define DDR_CTL_COL01_0                          0x058 
#define DDR_CTL_COL01_1                          0x05c 
#define DDR_CTL_COL20_0                          0x070 
#define DDR_CTL_COL20_1                          0x074 
#define DDR_CTL_COL21_0                          0x078 
#define DDR_CTL_COL21_1                          0x07c 
#define DDR_CTL_BNK10                            0x090 
#define DDR_CTL_BNK32                            0x094 
#define DDR_CTL_DCMD                             0x100 
#define DDR_CTL_DMODE_0                          0x104 
#define DDR_CTL_DMODE_1                          0x108 
#define DDR_CTL_CLKS                             0x10c 
#define DDR_CTL_ODT                              0x110 
#define DDR_CTL_TIM1_0                           0x114 
#define DDR_CTL_TIM1_1                           0x118 
#define DDR_CTL_TIM2                             0x11c 
#define DDR_CTL_CTL_CRC                          0x120 
#define DDR_CTL_DOUT_CRC                         0x124 
#define DDR_CTL_DIN_CRC                          0x128 
#define PHY_CONTROL_REGS_REVISION                0x200 
#define PHY_CONTROL_REGS_CLK_PM_CTRL             0x204 
#define PHY_CONTROL_REGS_PLL_STATUS              0x210 
#define PHY_CONTROL_REGS_PLL_CONFIG              0x214 
#define PHY_CONTROL_REGS_PLL_PRE_DIVIDER         0x218 
#define PHY_CONTROL_REGS_PLL_DIVIDER             0x21c 
#define PHY_CONTROL_REGS_PLL_CONTROL1            0x220 
#define PHY_CONTROL_REGS_PLL_CONTROL2            0x224 
#define PHY_CONTROL_REGS_PLL_SS_EN               0x228 
#define PHY_CONTROL_REGS_PLL_SS_CFG              0x22c 
#define PHY_CONTROL_REGS_STATIC_VDL_OVERRIDE     0x230 
#define PHY_CONTROL_REGS_DYNAMIC_VDL_OVERRIDE    0x234 
#define PHY_CONTROL_REGS_IDLE_PAD_CONTROL        0x238 
#define PHY_CONTROL_REGS_ZQ_PVT_COMP_CTL         0x23c 
#define PHY_CONTROL_REGS_DRIVE_PAD_CTL           0x240 
#define PHY_BYTE_LANE_0_REVISION                 0x300 
#define PHY_BYTE_LANE_0_VDL_CALIBRATE            0x304 
#define PHY_BYTE_LANE_0_VDL_STATUS               0x308 
    #define PHY_BYTE_LANE_0_VDL_STATUS_CALIB_TOTAL_STEP_MAX  31
    #define PHY_BYTE_LANE_0_VDL_STATUS_CALIB_TOTAL_STEP_STRT 8
    #define PHY_BYTE_LANE_0_VDL_STATUS_CALIB_TOTAL_STEP_MASK (0x1f << PHY_BYTE_LANE_0_VDL_STATUS_CALIB_TOTAL_STEP_STRT)
#define PHY_BYTE_LANE_0_VDL_OVERRIDE_0           0x310 
#define PHY_BYTE_LANE_0_VDL_OVERRIDE_1           0x314 
#define PHY_BYTE_LANE_0_VDL_OVERRIDE_2           0x318 
#define PHY_BYTE_LANE_0_VDL_OVERRIDE_3           0x31c 
#define PHY_BYTE_LANE_0_VDL_OVERRIDE_4           0x320 
#define PHY_BYTE_LANE_0_VDL_OVERRIDE_5           0x324 
#define PHY_BYTE_LANE_0_VDL_OVERRIDE_6           0x328 
#define PHY_BYTE_LANE_0_VDL_OVERRIDE_7           0x32c 
#define PHY_BYTE_LANE_0_READ_CONTROL             0x330 
#define PHY_BYTE_LANE_0_READ_FIFO_STATUS         0x334 
#define PHY_BYTE_LANE_0_READ_FIFO_CLEAR          0x338 
#define PHY_BYTE_LANE_0_IDLE_PAD_CONTROL         0x33c 
#define PHY_BYTE_LANE_0_DRIVE_PAD_CTL            0x340 
#define PHY_BYTE_LANE_0_CLOCK_PAD_DISABLE        0x344 
#define PHY_BYTE_LANE_0_WR_PREAMBLE_MODE         0x348 
#define PHY_BYTE_LANE_1_REVISION                 0x400 
#define PHY_BYTE_LANE_1_VDL_CALIBRATE            0x404 
#define PHY_BYTE_LANE_1_VDL_STATUS               0x408 
#define PHY_BYTE_LANE_1_VDL_OVERRIDE_0           0x410 
#define PHY_BYTE_LANE_1_VDL_OVERRIDE_1           0x414 
#define PHY_BYTE_LANE_1_VDL_OVERRIDE_2           0x418 
#define PHY_BYTE_LANE_1_VDL_OVERRIDE_3           0x41c 
#define PHY_BYTE_LANE_1_VDL_OVERRIDE_4           0x420 
#define PHY_BYTE_LANE_1_VDL_OVERRIDE_5           0x424 
#define PHY_BYTE_LANE_1_VDL_OVERRIDE_6           0x428 
#define PHY_BYTE_LANE_1_VDL_OVERRIDE_7           0x42c 
#define PHY_BYTE_LANE_1_READ_CONTROL             0x430 
#define PHY_BYTE_LANE_1_READ_FIFO_STATUS         0x434 
#define PHY_BYTE_LANE_1_READ_FIFO_CLEAR          0x438 
#define PHY_BYTE_LANE_1_IDLE_PAD_CONTROL         0x43c 
#define PHY_BYTE_LANE_1_DRIVE_PAD_CTL            0x440 
#define PHY_BYTE_LANE_1_CLOCK_PAD_DISABLE        0x444 
#define PHY_BYTE_LANE_1_WR_PREAMBLE_MODE         0x448 
#define PHY_BYTE_LANE_2_REVISION                 0x500 
#define PHY_BYTE_LANE_2_VDL_CALIBRATE            0x504 
#define PHY_BYTE_LANE_2_VDL_STATUS               0x508 
#define PHY_BYTE_LANE_2_VDL_OVERRIDE_0           0x510 
#define PHY_BYTE_LANE_2_VDL_OVERRIDE_1           0x514 
#define PHY_BYTE_LANE_2_VDL_OVERRIDE_2           0x518 
#define PHY_BYTE_LANE_2_VDL_OVERRIDE_3           0x51c 
#define PHY_BYTE_LANE_2_VDL_OVERRIDE_4           0x520 
#define PHY_BYTE_LANE_2_VDL_OVERRIDE_5           0x524 
#define PHY_BYTE_LANE_2_VDL_OVERRIDE_6           0x528 
#define PHY_BYTE_LANE_2_VDL_OVERRIDE_7           0x52c 
#define PHY_BYTE_LANE_2_READ_CONTROL             0x530 
#define PHY_BYTE_LANE_2_READ_FIFO_STATUS         0x534 
#define PHY_BYTE_LANE_2_READ_FIFO_CLEAR          0x538 
#define PHY_BYTE_LANE_2_IDLE_PAD_CONTROL         0x53c 
#define PHY_BYTE_LANE_2_DRIVE_PAD_CTL            0x540 
#define PHY_BYTE_LANE_2_CLOCK_PAD_DISABLE        0x544 
#define PHY_BYTE_LANE_2_WR_PREAMBLE_MODE         0x548 
#define PHY_BYTE_LANE_3_REVISION                 0x600 
#define PHY_BYTE_LANE_3_VDL_CALIBRATE            0x604 
#define PHY_BYTE_LANE_3_VDL_STATUS               0x608 
#define PHY_BYTE_LANE_3_VDL_OVERRIDE_0           0x610 
#define PHY_BYTE_LANE_3_VDL_OVERRIDE_1           0x614 
#define PHY_BYTE_LANE_3_VDL_OVERRIDE_2           0x618 
#define PHY_BYTE_LANE_3_VDL_OVERRIDE_3           0x61c 
#define PHY_BYTE_LANE_3_VDL_OVERRIDE_4           0x620 
#define PHY_BYTE_LANE_3_VDL_OVERRIDE_5           0x624 
#define PHY_BYTE_LANE_3_VDL_OVERRIDE_6           0x628 
#define PHY_BYTE_LANE_3_VDL_OVERRIDE_7           0x62c 
#define PHY_BYTE_LANE_3_READ_CONTROL             0x630 
#define PHY_BYTE_LANE_3_READ_FIFO_STATUS         0x634 
#define PHY_BYTE_LANE_3_READ_FIFO_CLEAR          0x638 
#define PHY_BYTE_LANE_3_IDLE_PAD_CONTROL         0x63c 
#define PHY_BYTE_LANE_3_DRIVE_PAD_CTL            0x640 
#define PHY_BYTE_LANE_3_CLOCK_PAD_DISABLE        0x644 
#define PHY_BYTE_LANE_3_WR_PREAMBLE_MODE         0x648 
#define DDR_CTL_GCFG                             0x800 
    #define DDR_CTL_GCFG_MEMINITDONE(x)     ((x & 0x1)<<8)
    #define DDR_CTL_GCFG_MEMINITDONE_STRT   (8)
    #define DDR_CTL_GCFG_MEMINITDONE_MSK    (1<<DDR_CTL_GCFG_MEMINITDONE_STRT)
#define DDR_CTL_LBIST_CFG                        0x804 
#define DDR_CTL_LBIST_SEED                       0x808 
#define DDR_CTL_ARB                              0x80c 
#define DDR_CTL_PI_GCF                           0x810 
#define DDR_CTL_PI_UBUS_CTL                      0x814 
#define DDR_CTL_PI_MIPS_CTL                      0x818 
#define DDR_CTL_PI_DSL_MIPS_CTL                  0x81c 
#define DDR_CTL_PI_DSL_PHY_CTL                   0x820 
#define DDR_CTL_PI_UBUS_ST                       0x824 
#define DDR_CTL_PI_MIPS_ST                       0x828 
#define DDR_CTL_PI_DSL_MIPS_ST                   0x82c 
#define DDR_CTL_PI_DSL_PHY_ST                    0x830 
#define DDR_CTL_PI_UBUS_SMPL                     0x834 
    #define DDR_CTL_PI_UBUS_SMPL_SAMPLING_PERIOD_STRT  (28)
#define DDR_CTL_TESTMODE                         0x838 
#define DDR_CTL_TEST_CFG1                        0x83c 
#define DDR_CTL_TEST_PAT                         0x840 
#define DDR_CTL_TEST_COUNT                       0x844 
#define DDR_CTL_TEST_CURR_COUNT                  0x848 
#define DDR_CTL_TEST_ADDR_UPDT                   0x84c 
#define DDR_CTL_TEST_ADDR                        0x850 
#define DDR_CTL_TEST_DATA0                       0x854 
#define DDR_CTL_TEST_DATA1                       0x858 
#define DDR_CTL_TEST_DATA2                       0x85c 
#define DDR_CTL_TEST_DATA3                       0x860 


/*
#####################################################################
# UART Control Registers
#####################################################################
*/
#define UART0CONTROL     0x01
#define UART0CONFIG      0x02
#define UART0RXTIMEOUT   0x03
#define UART0BAUD        0x04
#define UART0FIFOCFG     0x0a
#define UART0INTMASK     0x10
#define UART0INTSTAT     0x12
#define UART0DATA        0x17

#define BRGEN            0x80   /* Control register bit defs */
#define TXEN             0x40
#define RXEN             0x20
#define LOOPBK           0x10
#define TXPARITYEN       0x08
#define TXPARITYEVEN     0x04
#define RXPARITYEN       0x02
#define RXPARITYEVEN     0x01

#define XMITBREAK        0x40   /* Config register */
#define BITS5SYM         0x00
#define BITS6SYM         0x10
#define BITS7SYM         0x20
#define BITS8SYM         0x30
#define ONESTOP          0x07
#define TWOSTOP          0x0f

#define DELTAIP         0x0001
#define TXUNDERR        0x0002
#define TXOVFERR        0x0004
#define TXFIFOTHOLD     0x0008
#define TXREADLATCH     0x0010
#define TXFIFOEMT       0x0020
#define RXUNDERR        0x0040
#define RXOVFERR        0x0080
#define RXTIMEOUT       0x0100
#define RXFIFOFULL      0x0200
#define RXFIFOTHOLD     0x0400
#define RXFIFONE        0x0800
#define RXFRAMERR       0x1000
#define RXPARERR        0x2000
#define RXBRK           0x4000


/*
#####################################################################
# DECT IP Control Registers
#####################################################################
*/
#define DECT_STARTCTL   0xb0e50818
#define PCM_BUFF_CTL3   0xb0e5082c
#define PCM_BUFF_CTL7   0xb0e5083c
#define DECT_AHB_CHAN0_RX  0xb0e50a20
#define DECT_AHB_CHAN1_RX  0xb0e50de0
#define DECT_AHB_CHAN2_RX  0xb0e511a0
#define DECT_AHB_CHAN3_RX  0xb0e51560
#define DECT_AHB_CHAN0_TX  0xb0e50840
#define DECT_AHB_CHAN1_TX  0xb0e50c00
#define DECT_AHB_CHAN2_TX  0xb0e50fc0
#define DECT_AHB_CHAN3_TX  0xb0e51380
#define DECT_CLKEN         0x00000040   

#ifdef __cplusplus
}
#endif

#endif
