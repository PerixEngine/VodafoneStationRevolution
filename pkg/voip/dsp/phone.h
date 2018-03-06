/****************************************************************************
 *
 * rg/pkg/voip/dsp/phone.h
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

#ifndef _PHONE_H_
#define _PHONE_H_

#include <linux/ioctl.h>
#include <voip_types.h>
#include <voip/common/voip_common_dsp.h>

#ifndef __KERNEL__
#include <rg_def.h>
#else
#include <kos/kos.h>
#endif

#define VOIP_MAX_PACKET_LEN 1500

#define VOIP_ONHOOK 0
#define VOIP_OFFHOOK 1

#define DYNAMIC_PAYLOAD 96 /* RFC3551 99..127 */

#define AUDIO_PARAMS_SUPPRESS_DTMF (0x1 << 0)
#define AUDIO_PARAMS_DISABLE_VAD (0x1 << 1)
#define AUDIO_PARAMS_DTMF_PAYLOAD_TYPE_SPECIFIED (0x1 << 6)
/* AUDIO_PARAMS bits 2 to 5 reserved for DTMF payload */
#define SET_AUDIO_PARAMS_DTMF_PAYLOAD(audio_params, payload) \
    do{ \
	audio_params = (audio_params | \
	(((0x0F & (payload-DYNAMIC_PAYLOAD))) << 2) | \
	AUDIO_PARAMS_DTMF_PAYLOAD_TYPE_SPECIFIED); \
    }while(0)
#define GET_AUDIO_PARAMS_DTMF_PAYLOAD(audio_params, payload) \
    do{ \
	if (audio_params & AUDIO_PARAMS_DTMF_PAYLOAD_TYPE_SPECIFIED) \
	    payload = (((audio_params >> 2) & 0x0F) + DYNAMIC_PAYLOAD); \
    }while(0)

#define MAX_ALLOC_ID_LEN 128

typedef enum {
    VOIP_DATA_MODE_VOICE = 0,
    VOIP_DATA_MODE_FAX = 1,
    VOIP_DATA_MODE_T38 = 2,
    VOIP_DATA_MODE_MODEM = 3,
} voip_data_mode_t;

typedef struct {
    int  ecMode;              /* Error correction mode */
    u32  maxRemDgrm;          /* Maximum remote end datagram size */      
    u32  maxBitRate;          /* Maximum bit rate for the fax */
    int t38Version;           /* T38 Asn.1 Version */
    /* Facsimile image conversion options */
    udptl_conversion_options_t conversionOption;
    udptl_rate_management_method_t rateManagementMethod;
} voip_dsp_t38_args_t;

typedef struct voip_dsp_stats_t {
    u32 rx_packets;
    u32 tx_packets;
    u32 rx_octets;
    u32 tx_octets;
    u32 rx_packets_lost;
    u32 jb_overruns;
    u32 jb_underruns;
} voip_dsp_stats_t;

typedef struct {
	int channel;
	struct voip_dsp_stats_t stats;
} voip_dsp_stats_args_t;

typedef struct {
    int channel;
    rtp_payload_type_t codec;
    int ptime_ms;
    int suppress_dtmf;
    int dtmf_payload;
    int disable_vad;
    voip_data_mode_t data_mode;
    u32 rtp_context;
    u8 use_t38_args;
    voip_dsp_t38_args_t	t38_args;
} voip_dsp_record_args_t;

typedef struct {
    int line;
    int channel;
} voip_dsp_bind_arg_t;

typedef enum {
    PHONE_KEY_0         = 0, /* MUST BE SET TO 0 */
    PHONE_KEY_1         = 1,
    PHONE_KEY_2         = 2,
    PHONE_KEY_3         = 3,
    PHONE_KEY_4         = 4,
    PHONE_KEY_5         = 5,
    PHONE_KEY_6         = 6,
    PHONE_KEY_7         = 7,
    PHONE_KEY_8         = 8,
    PHONE_KEY_9         = 9,
    PHONE_KEY_ASTERISK  = 10, /* "*" key */
    PHONE_KEY_POUND     = 11, /* "#" key */
    PHONE_KEY_A         = 12,
    PHONE_KEY_B         = 13,
    PHONE_KEY_C         = 14,
    PHONE_KEY_D         = 15,

    PHONE_KEY_HOLD      = 16,
    PHONE_KEY_TRANSFER  = 17,
    PHONE_KEY_CONFERENCE= 18,

    PHONE_KEY_HOOK_ON   = 32,
    PHONE_KEY_HOOK_OFF  = 33,
    PHONE_KEY_FLASH     = 34,
    PHONE_KEY_SETTINGS  = 35, /* internal settings made, read them */
    PHONE_KEY_RING_ON   = 36, /* CID period is off, 3-rd ring started */
    PHONE_KEY_RING_OFF  = 37,
    PHONE_KEY_POLARITY_REVERSAL	= 38,
    PHONE_KEY_FWD_DISCONNECT	= 39,
    PHONE_KEY_DISCONNECT_TONE	= 40,
    PHONE_KEY_RING_TRYING = 41, /* first ring started */
    PHONE_KEY_FAX_CNG   = 42, /* fax CNG tone - calling tone (Local) */
    PHONE_KEY_FAX_NET_CNG = 43, /* *fax CNG tone - calling tone (Network)*/
    PHONE_KEY_FAX_DIS = 44, /* Fax  V.21 DIS Message (Local)*/
    PHONE_KEY_FAX_NET_DIS = 45, /* Fax V.21 DIS Message (Network)*/
    PHONE_KEY_MODEM_CNG = 46, /* Modem CT tone (Local)*/
    PHONE_KEY_MODEM_NET_CNG = 47, /* Modem CT tone (Network) */
    PHONE_KEY_FAXMODEM_NET_CED = 48, /* fax CED - answering tone (Network)*/
    PHONE_KEY_FAXMODEM_CED   = 49, /* fax CED tone - answering tone (Local) */
    PHONE_KEY_FAXMODEM_PR   = 50, /* modem detection virtual key */
    PHONE_KEY_FAXMODEM_AM = 51, /* fax answer tone with amplitue modulation*/
    PHONE_KEY_PORT_ATTACHED = 52, /* the FXS port is connected to a phone */
    PHONE_KEY_PORT_DETACHED = 53, /* the FXS port is NOT connected to a phone*/
} phone_key_t;

typedef struct {
    int channel; /* 0 - from local line; >0 - from network peer */
    phone_key_t key;
    int pressed;
} phone_event_t;

typedef enum {
    PHONE_TONE_NONE = 0,
    /* continuous tones */
    PHONE_TONE_DIAL = 1,
    PHONE_TONE_RING = 2,
    PHONE_TONE_BUSY = 3,
    PHONE_TONE_WARN = 4,
    PHONE_TONE_CALLER_WAITING = 5,
    PHONE_TONE_HOOK_OFF = 6,
    PHONE_TONE_REORDER = 7,
    PHONE_TONE_MWI = 8, /* Message waiting indication */
    PHONE_TONE_STUTTER_DIAL = 9,
    PHONE_TONE_CONFIRM = 10,
    PHONE_TONE_HOLD = 11,
    PHONE_TONE_CONGESTION = 12, /* Special information tone */
    PHONE_TONE_UNALLOCATED = 13, /* Dead line / unallocated number tone */
    PHONE_TONE_BUSY_UNOBTAINABLE = 14,
    PHONE_TONE_3G_BACKUP = 15,
    PHONE_TONE_REORDER_SHORT = 16, /* reorder, and then regular dial tone */
    PHONE_TONE_CFWD = 17,
    /* dtmf */
    PHONE_TONE_DTMF0 = 32,
    PHONE_TONE_DTMF1 = 33,
    PHONE_TONE_DTMF2 = 34,
    PHONE_TONE_DTMF3 = 35,
    PHONE_TONE_DTMF4 = 36,
    PHONE_TONE_DTMF5 = 37,
    PHONE_TONE_DTMF6 = 38,
    PHONE_TONE_DTMF7 = 39,
    PHONE_TONE_DTMF8 = 40,
    PHONE_TONE_DTMF9 = 41,
    PHONE_TONE_DTMFA = 42,
    PHONE_TONE_DTMFB = 43,
    PHONE_TONE_DTMFC = 44,
    PHONE_TONE_DTMFD = 45,
    PHONE_TONE_DTMF_ASTERISK = 46,
    PHONE_TONE_DTMF_POUND = 47,
    PHONE_TONE_DTMF_LAST = 48,
} phone_tone_t;

typedef enum {
    VOICE_IO_NONE = 0,
    VOICE_IO_RING = 1,
    VOICE_IO_HANDSET = 2,
    VOICE_IO_HEADSET = 3,
    VOICE_IO_HANDSFREE = 4,
} voice_io_mode_t;

typedef enum {
    PHONE_CONF_VOLUME_SET_SPK = 1, /* followed by a number */
    PHONE_CONF_VOLUME_SET_HS = 2,
    PHONE_CONF_VOLUME_SET_RING = 3,
    PHONE_CONF_VOLUME_SET_RING_MUTE = 4,

    PHONE_CONF_GET_VOLUME = 129, /* followed by io_mode */
    PHONE_CONF_GET_RING_MUTE = 130,
    PHONE_CONF_GET_HF = 131, /* are we in HandsFree mode ? */
} phone_conf_cmd_t;

typedef struct {
    phone_conf_cmd_t cmd;
    int param;
    int ret;
} phone_conf_t;

#define TIME_STRING_LEN 9
#define MAX_CALLER_ID_LEN 32
#define MAX_DIAL_STRING_LEN 32

typedef enum {
    PHONE_CALLER_NUMBER_PRESENT = 0,
    PHONE_CALLER_NUMBER_ABSENT = 1,
    PHONE_CALLER_NUMBER_SUPPRESSED = 2,
} phone_caller_number_status_t;

typedef enum {
    PHONE_CALLER_NAME_PRESENT = 0,
    PHONE_CALLER_NAME_ABSENT = 1,
    PHONE_CALLER_NAME_SUPPRESSED = 2,
} phone_caller_name_status_t;

typedef struct {
    char time[TIME_STRING_LEN]; /* System time, as assigned by the
				 * application. The format is MMDDhhmm */
    char number[MAX_DIAL_STRING_LEN]; /* Caller's number, as assigned by the
				       * application */
    char name[MAX_CALLER_ID_LEN]; /* Caller's user-id */
    char addr[DOTTED_IP_LEN]; /* Caller's IP address */
    phone_caller_number_status_t number_status; /* is 'number' valid? */
    phone_caller_name_status_t name_status; /* is 'name' valid? */
} phone_caller_id_t;

typedef struct {
    phone_caller_id_t cid;
    int distinctive_ring;
} call_params_t;

typedef enum {
     TONE_DIRECTION_LOCAL = 0,
     TONE_DIRECTION_NET = 1,
} phone_tone_direction_t;

typedef struct {
    phone_tone_t tone;
    phone_tone_direction_t direction;
} tone_param_t;

#define DC_ALLOC_FLAG_LAME 1

typedef struct {
    char id[MAX_ALLOC_ID_LEN];
    int chan;
    int flags;
} alloc_params_t;

typedef enum {
    PCM_RES_NB_ALAW_8BIT = 0,
    PCM_RES_NB_ULAW_8BIT = 1,
    PCM_RES_NB_LINEAR_16BIT = 2,
    PCM_RES_WB_ALAW_8BIT = 3,
    PCM_RES_WB_ULAW_8BIT = 4,
    PCM_RES_WB_LINEAR_16BIT = 5,
} pcm_res_type_t;

typedef struct {
    pcm_res_type_t pcm_res;
} voip_dsp_line_conf_t;

#define VOIP_LINE_BIND _IOW(RG_IOCTL_PREFIX_VOIP, 1, int)
#define VOIP_LINE_TONE _IOW(RG_IOCTL_PREFIX_VOIP, 2, tone_param_t)
#define VOIP_SLIC_RING _IOW(RG_IOCTL_PREFIX_VOIP, 3, phone_caller_id_t)
#define VOIP_SLIC_CALL_WAITING_ALERT \
    _IOW(RG_IOCTL_PREFIX_VOIP, 4, phone_caller_id_t)
#define VOIP_SLIC_GET_HOOK _IOR(RG_IOCTL_PREFIX_VOIP, 5, int)
#define VOIP_DSP_START _IOW(RG_IOCTL_PREFIX_VOIP, 6, voip_dsp_record_args_t)
#define VOIP_DSP_STOP _IOW(RG_IOCTL_PREFIX_VOIP, 7, int)
#define VOIP_SLIC_CONF _IOWR(RG_IOCTL_PREFIX_VOIP, 8, phone_conf_t)
#define VOIP_DSP_BIND _IOWR(RG_IOCTL_PREFIX_VOIP, 10, voip_dsp_bind_arg_t)
#define VOIP_FXO_SET_HOOK _IOW(RG_IOCTL_PREFIX_VOIP, 11, int)
#define VOIP_SLIC_VMWI _IOW(RG_IOCTL_PREFIX_VOIP, 12, int)
#define VOIP_SLIC_FWD_DISCONNECT _IOR(RG_IOCTL_PREFIX_VOIP, 13, int)
#define VOIP_SLIC_SET_POWER _IOR(RG_IOCTL_PREFIX_VOIP, 14, int)
#define VOIP_FXO_GET_CID _IOW(RG_IOCTL_PREFIX_VOIP, 15, phone_caller_id_t)
#define VOIP_FXO_FLASH_SEND _IOWR(RG_IOCTL_PREFIX_VOIP, 17, int)
#define VOIP_FXO_CID_RX_START _IOWR(RG_IOCTL_PREFIX_VOIP, 18, int)
#define VOIP_FXO_CID_RX_STOP _IOWR(RG_IOCTL_PREFIX_VOIP, 19, int)
#define VOIP_LINE_DC_ALLOC _IOW(RG_IOCTL_PREFIX_VOIP, 20, alloc_params_t)  
#define VOIP_LINE_DC_FREE _IOW(RG_IOCTL_PREFIX_VOIP, 21, int)
#define VOIP_DSP_PRINT_RESOURCES _IOWR(RG_IOCTL_PREFIX_VOIP, 22, int)
#define VOIP_DSP_LINE_CONF _IOWR(RG_IOCTL_PREFIX_VOIP, 23, voip_dsp_line_conf_t)
#define VOIP_DSP_MODIFY _IOW(RG_IOCTL_PREFIX_VOIP, 24, voip_dsp_record_args_t)
#define VOIP_DSP_STATS_GET _IOW(RG_IOCTL_PREFIX_VOIP, 25, voip_dsp_stats_args_t) 
#define VOIP_DSP_PHONE_DETECT _IOW(RG_IOCTL_PREFIX_VOIP, 26, int) 
#endif
