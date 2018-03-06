
#ifndef __BPMCTL_COMMON_H_INCLUDED__
#define __BPMCTL_COMMON_H_INCLUDED__

/*
<:copyright-broadcom

 Copyright (c) 2007 Broadcom Corporation
 All Rights Reserved
 No portions of this material may be reproduced in any form without the
 written permission of:
          Broadcom Corporation
          5300 California Avenue
          Irvine, California 92617
 All information contained in this document is Broadcom Corporation
 company private, proprietary, and trade secret.

:>
*/

/*
 *******************************************************************************
 * File Name  : bpmctl_common.h
 * Description: This file is common IOCTL interface between kernel and user
 *              space for BPM.
 *******************************************************************************
 */

#define BPM_DEVNAME             "bpm"

/* BPM Character Device */
#define BPM_DRV_NAME            BPM_DEVNAME
#define BPM_DRV_DEVICE_NAME     "/dev/" BPM_DRV_NAME

#define BPMCTL_ERROR               (-1)
#define BPMCTL_SUCCESS             0

/*
 * Ioctl definitions.
 */
typedef enum {
    BPMCTL_IOCTL_SYS,
    BPMCTL_IOCTL_MAX
} bpmctl_ioctl_t;

typedef enum {
    BPMCTL_SUBSYS_STATUS,
    BPMCTL_SUBSYS_THRESH,
    BPMCTL_SUBSYS_BUFFERS,
    BPMCTL_SUBSYS_MAX
} bpmctl_subsys_t;

typedef enum {
    BPMCTL_OP_GET,
    BPMCTL_OP_SET,
    BPMCTL_OP_ADD,
    BPMCTL_OP_REM,
    BPMCTL_OP_DUMP,
    BPMCTL_OP_MAX
} bpmctl_op_t;

typedef enum {
    BPMCTL_STATUS_DISABLE,
    BPMCTL_STATUS_ENABLE,
    BPMCTL_STATUS_MAX
} bpmctl_status_t;

typedef struct {
    bpmctl_subsys_t subsys;
    bpmctl_op_t     op;
    bpmctl_status_t status;
} bpmctl_data_t;


#endif /*  __BPMCTL_COMMON_H_INCLUDED__ */

