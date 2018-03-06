/****************************************************************************
 *
 * rg/pkg/voip/asterisk/channels/dect_cc.h
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

#ifndef _DECT_CC_H_
#define _DECT_CC_H_

#define CC_DIGIT_R_KEY	21

typedef enum {
	CC_NONE,
	CC_SETUP, 
	CC_SETUPACK,
	CC_ALERTING,
	CC_CONNECT,
	CC_CONNECTACK,
	CC_DISCONNECT,
	CC_RELEASE,
	CC_REJECT,
	CC_PROCEEDING, 
	CC_HOLD, 
	CC_UNHOLD, 
	CC_CALLSTATUS,
	CC_TRANSFER, 
	CC_3PTY,
	CC_DIGIT, 
	CC_CHANGE_CODEC,
	CC_CODEC_CHANGED,
} cc_message_id_t;

typedef enum {
    CSTAT_IDLE,
    CSTAT_SETUP,
    CSTAT_SETUP_ACK,
    CSTAT_PROC,
    CSTAT_ALERTING,
    CSTAT_CONNECT,
    CSTAT_DISCONNECTING,
    CSTAT_HOLD,
    CSTAT_INTERCEPTED,
    CSTAT_CONFERENCE,
    CSTAT_TRANSFERRED
} call_status_t;

#define DECT_CODEC_NONE 0
#define DECT_CODEC_G726 (1 << 0)
#define DECT_CODEC_G722 (1 << 1)

typedef struct {
	cc_message_id_t id;  
	u8 hs_id;            /* Handset ID     */
	u32 cr;              /* Call Reference */
	u32 cr2;             /* Call Reference for second call*/
	u32 codecs;

	union {
		struct {
			char *desc;
			int code;
		} reason;
		char *dialed_digits;
		int call_status;
	} u;
} cc_message_t;

typedef void (*cc_handle_cb_t)(cc_message_t *);

/* Open DECT call control socket */
int dect_cc_open(cc_handle_cb_t cb); 

/* Close DECT call control socket */
void dect_cc_close(void); 

/* Initialize DECT call control - resets all active DECT calls */
void dect_cc_init(void); 

/* Send call control message */
int dect_cc_setup(u8 hs_id, u8 pcm_chan, char *cid_number, char* cid_name,
    u32 codecs, int is_callwaiting);
int dect_cc_setupack(u8 hs_id, u32 cr, u8 pcm_chan, int codec);
int dect_cc_connect(u8 hs_id, u32 cr, int codec);
int dect_cc_disconnect(u8 hs_id, u32 cr, u8 final);
int dect_cc_release(u8 hs_id, u32 cr, u8 final);
int dect_cc_reject(u8 hs_id, u32 cr);
int dect_cc_hold(u8 hs_id, u32 cr);
int dect_cc_unhold(u8 hs_id, u32 cr);
int dect_cc_mwi(u8 msg_count, int line_id);
int dect_cc_callstatus(u8 hs_id, u32 cr, u32 cr2, int status);
int dect_cc_read_and_dispatch(int *id, int fd, short events, void *ignore);

char *cc_msg_str_get(int mgsid);

#endif
