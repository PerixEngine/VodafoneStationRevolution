/***********************************************************************
*
*   Copyright (c) Pegatron Corporation
*   All Rights Reserved
*
*   Purpose: MCU control common define
*
************************************************************************/

#ifndef _MCUCTL_COMMON_H
#define _MCUCTL_COMMON_H

#define msleep(n) usleep(n*1000)
#define UCHAR_0x0 0x0
#define SLAVE_ADDRESS 0x55
#define SLAVE_ADDRESS_LDROM 0x36
#define DELAY_UPDATE_APROM 8050
#define DELAY_UPDATE_APROM_FORMAT2 100
#define DELAY_SYNC_PACKNO 50
#define DELAY_RESET 500
#define DELAY_RUN_LDROM 500
#define DELAY_WRITE_CHECKSUM 200
#define APROM_CRASH "ffffffffffff"

/* APROM Command */
#define CMD_GET_FIRMWARE_VERSION 0xA6

/* LDROM Command */
#define CMD_UPDATE_APROM	0x000000A0
#define CMD_SYNC_PACKNO     0x000000A4
#define CMD_GET_FLASHMODE   0x000000CA
#define CMD_ERASE_ALL		0x000000A3
#define CMD_READ_CHECKSUM   0x000000C8
#define CMD_WRITE_CHECKSUM  0x000000C9
#define CMD_RUN_APROM		0x000000AB
#define CMD_RUN_LDROM		0x000000AC
#define CMD_RESET			0x000000AD
#define CMD_FORMAT2         0x00000000

#endif

