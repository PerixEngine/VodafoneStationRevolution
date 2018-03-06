/****************************************************************************
 *
 * rg/pkg/voip/asterisk/channels/dect_cc.c
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
#include <util/sys.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <rg_types.h>
#include <vendor/vtech/dect/vf_dect.h>

#include "asterisk.h"
#include "asterisk/logger.h"
#include "asterisk/channel.h"
#include "dect_cc.h"

#define CC_SOCKET_AST "/tmp/cc_socket_ast"

typedef struct {
	int dect_code;
	int ast_code;
} code2code_t;

static struct {
	struct sockaddr_un send_addr;
	int sock;
	cc_handle_cb_t handle_cb;
} cc_data;

typedef enum {
    CODE_2_AST,
    CODE_2_DECT
} code_mapping_direction_t;

static int code2code(code2code_t *list, int code, code_mapping_direction_t dir)
{
    if(dir == CODE_2_AST)
    {
	for (; list->dect_code != -1 && list->dect_code != code; list++);
	return list->ast_code;
    }
    else
    {
	for (; list->ast_code != -1 && list->ast_code != code; list++);
	return list->dect_code;
    }
}

static char *itoa(int num)
{
	static char ret[16];

	*ret = 0;
	snprintf(ret, sizeof(ret), "%d", num);
	return ret;
}

char *cc_msg_str_get(int msgid)
{
	char *str;

	static code2str_t cc_msg_str[] = {
		{CC_SETUP, "CC_SETUP"},
		{CC_SETUPACK, "CC_SETUPACK"},
		{CC_ALERTING, "CC_ALERTING"},
		{CC_CONNECT, "CC_CONNECT"},
		{CC_CONNECTACK, "CC_CONNECTACK"},
		{CC_DISCONNECT, "CC_DISCONNECT"},
		{CC_RELEASE, "CC_RELEASE"},
		{CC_REJECT, "CC_REJECT"},
		{CC_PROCEEDING, "CC_PROCEEDING"},
		{CC_HOLD, "CC_HOLD"},
		{CC_UNHOLD, "CC_UNHOLD"},
		{CC_CALLSTATUS, "CC_CALLSTATUS"},
		{CC_TRANSFER, "CC_TRANSFER"},
		{CC_3PTY, "CC_3PTY"},
		{CC_DIGIT, "CC_DIGIT"},
		{CC_CHANGE_CODEC, "CC_CHANGE_CODEC"},
		{CC_CODEC_CHANGED, "CC_CODEC_CHANGED"},
		{-1, NULL}
	};

	return (str = code2str(cc_msg_str, msgid)) ? str : itoa(msgid);
}

char *vf_cstat_str_get(cstatus_t status)
{
    char *str;

    static code2str_t cstat_str[] = {
	{DECT_CSTAT_IDLE , "DECT_CSTAT_IDLE"},
	{DECT_CSTAT_SETUP, "DECT_CSTAT_SETUP" },
	{DECT_CSTAT_SETUP_ACK, "DECT_CSTAT_SETUP_ACK"},
	{DECT_CSTAT_PROC, "DECT_CSTAT_PROC"},
	{DECT_CSTAT_ALERTING, "DECT_CSTAT_ALERTING"},
	{DECT_CSTAT_CONNECT, "DECT_CSTAT_CONNECT"},
	{DECT_CSTAT_DISCONNECTING, "DECT_CSTAT_DISCONNECTING"},
	{DECT_CSTAT_HOLD, "DECT_CSTAT_HOLD"},
	{DECT_CSTAT_INTERCEPTED, "DECT_CSTAT_INTERCEPTED"},
	{DECT_CSTAT_CONFERENCE, "DECT_CSTAT_CONFERENCE"},
	{DECT_CSTAT_TRANSFERRED, "DECT_CSTAT_TRANSFERRED"},
	{-1, NULL}
    };

    return (str = code2str(cstat_str, status)) ? str : itoa(status);
}

static char *vf_cc_msg_str_get(int msgid)
{
	char *str;

	static code2str_t vf_cc_msg_str[] = {
		{DECT_CC_INIT, "DECT_CC_INIT"},
		{DECT_CC_SETUP, "DECT_CC_SETUP"},
		{DECT_CC_SETUPACK, "DECT_CC_SETUPACK"},
		{DECT_CC_ALERTING, "DECT_CC_ALERTING"},
		{DECT_CC_CONNECT, "DECT_CC_CONNECT"},
		{DECT_CC_CONNECTACK, "DECT_CC_CONNECTACK"},
		{DECT_CC_DISCONNECT, "DECT_CC_DISCONNECT"},
		{DECT_CC_RELEASE, "DECT_CC_RELEASE"},
		{DECT_CC_REJECT, "DECT_CC_REJECT"},
		{DECT_CC_PROCEEDING, "DECT_CC_PROCEEDING"},
		{DECT_CC_HOLD, "DECT_CC_HOLD"},
		{DECT_CC_UNHOLD, "DECT_CC_UNHOLD"},
		{DECT_CC_CALLSTATUS, "DECT_CC_CALLSTATUS"},
		{DECT_CC_TRANSFER, "DECT_CC_TRANSFER"},
		{DECT_CC_3PTY, "DECT_CC_3PTY"},
		{DECT_CC_DIGIT, "DECT_CC_DIGIT"},
		{DECT_CC_CHANGE_CODEC, "DECT_CC_CHANGE_CODEC"},
		{DECT_CC_CODEC_CHANGED, "DECT_CC_CODEC_CHANGED"},
		{-1, NULL}
	};

	return (str = code2str(vf_cc_msg_str, msgid)) ? str : itoa(msgid);
}

static int msg_code_map(int msgid, code_mapping_direction_t dir)
{
	static code2code_t vf_cc_msg[] = {
		{DECT_CC_SETUP, CC_SETUP},
		{DECT_CC_SETUPACK, CC_SETUPACK},
		{DECT_CC_ALERTING, CC_ALERTING},
		{DECT_CC_CONNECT, CC_CONNECT},
		{DECT_CC_CONNECTACK, CC_CONNECTACK},
		{DECT_CC_DISCONNECT, CC_DISCONNECT},
		{DECT_CC_RELEASE, CC_RELEASE},
		{DECT_CC_REJECT, CC_REJECT},
		{DECT_CC_PROCEEDING, CC_PROCEEDING},
		{DECT_CC_HOLD, CC_HOLD},
		{DECT_CC_UNHOLD, CC_UNHOLD},
		{DECT_CC_CALLSTATUS, CC_CALLSTATUS},
		{DECT_CC_TRANSFER, CC_TRANSFER},
		{DECT_CC_3PTY, CC_3PTY},
		{DECT_CC_DIGIT, CC_DIGIT},
		{DECT_CC_CHANGE_CODEC, CC_CHANGE_CODEC},
		{DECT_CC_CODEC_CHANGED, CC_CODEC_CHANGED},
		{-1, -1}
	};

	return code2code(vf_cc_msg, msgid, dir);
}

static int call_status_map(int cstat, code_mapping_direction_t dir)
{
    static code2code_t vf_cstat_map[] = 
    {
	{DECT_CSTAT_IDLE, CSTAT_IDLE},
	{DECT_CSTAT_SETUP, CSTAT_SETUP},
	{DECT_CSTAT_SETUP_ACK, CSTAT_SETUP_ACK},
	{DECT_CSTAT_PROC, CSTAT_PROC},
	{DECT_CSTAT_ALERTING, CSTAT_ALERTING},
	{DECT_CSTAT_CONNECT, CSTAT_CONNECT},
	{DECT_CSTAT_DISCONNECTING, CSTAT_DISCONNECTING},
	{DECT_CSTAT_HOLD, CSTAT_HOLD},
	{DECT_CSTAT_INTERCEPTED, CSTAT_INTERCEPTED},
	{DECT_CSTAT_CONFERENCE, CSTAT_CONFERENCE},
	{DECT_CSTAT_TRANSFERRED, CSTAT_TRANSFERRED},
	{-1, -1}
    };

    return code2code(vf_cstat_map, cstat, dir);
}

static code2code_t vf_codec[] = {
    {CODEC_G726, DECT_CODEC_G726},
    {CODEC_G722, DECT_CODEC_G722},
    {-1, -1} 
};

static int ast_codec_to_vf(int codec)
{
    return code2code(vf_codec, codec, CODE_2_DECT);
}

static int vf_codec_to_ast(int codec)
{
    int tmp = code2code(vf_codec, codec, CODE_2_AST);

    if (tmp == -1)
	return DECT_CODEC_NONE;

    return tmp;
}

static u32 vf_codec_bm_to_ast(u32 codecs)
{
    u32 tmp = 0;
    code2code_t *list;

    for (list = vf_codec; list->dect_code != -1; list++)
        if (codecs & list->dect_code)
    	    tmp |= list->ast_code;

    return tmp;
}

static u32 ast_codec_bm_to_vf(u32 codecs)
{
    u32 tmp = 0;
    code2code_t *list;

    for (list = vf_codec; list->ast_code != -1; list++)
        if (codecs & list->ast_code)
    	    tmp |= list->dect_code;

    return tmp;
}

static inline void msg_hdr_init(_DECT_FIFO_MSG_T *msg, int id, int len)
{
	static int curr_no = 0;

	msg->transid = curr_no++; 
	msg->msgid = id;    
	msg->tmo = 0;
	msg->datalen = len;
}

static int socket_send(int sock, _DECT_FIFO_MSG_T *msg)
{
	while (sendto(sock, msg, FIFOMSG_SIZE(msg), 0, 
		(struct sockaddr *)&cc_data.send_addr, sizeof(struct sockaddr_un)) < 0)
	{
		if (errno == EAGAIN || errno == EINTR || errno == EWOULDBLOCK)
			continue;
		return -1;
	}

	return 0;
}

static int msg_send(int id, void *msg, int len)
{
	_DECT_FIFO_MSG_T *hdr = malloc(sizeof(_DECT_FIFO_MSG_T) + len - 1);
	int res = -1;

	if (!hdr)
		return -1;

	msg_hdr_init(hdr, id, len);

	if (msg) /* empty messages are acceptable */
		memcpy(hdr->data, msg, len);

	res = socket_send(cc_data.sock, hdr);
	free(hdr);

	return res;
}

int dect_cc_open(cc_handle_cb_t cb)
{
	struct sockaddr_un recv_addr;
	int sock;

	if ((sock = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0)
	{
		ast_log(LOG_ERROR, "Failed to open unix socket %m\n");
		return -1;
	}

	if (fcntl(sock, F_SETFL, O_NDELAY) < 0)
	{
		ast_log(LOG_ERROR, "failed setting non-blocking\n");
		goto Error;
	}

	bzero(&recv_addr, sizeof(struct sockaddr_un));
	recv_addr.sun_family = AF_UNIX;
	strcpy(recv_addr.sun_path, CC_SOCKET_AST); 
	unlink(CC_SOCKET_AST);

	if (bind(sock, (struct sockaddr *)&recv_addr, sizeof(recv_addr)) < 0) 
	{
		ast_log(LOG_ERROR, "Failed to bind unix socket %m\n");
		goto Error;
	}

	cc_data.send_addr.sun_family = AF_UNIX;
	strcpy(cc_data.send_addr.sun_path, CC_SOCKET);

	cc_data.handle_cb = cb;
	return (cc_data.sock = sock);

Error:
	close(sock);
	return -1;
}

void dect_cc_close(void)
{
	close(cc_data.sock);
	cc_data.sock = -1;
	cc_data.handle_cb = NULL;
}

void dect_cc_init(void)
{
	if (msg_send(DECT_CC_INIT, NULL, 0))
		ast_log(LOG_ERROR, "dect_cc_send failed with %d\n", errno);
}


int dect_cc_setup(u8 hs_id, u8 pcm_chan, char *cid_number, char* cid_name,
	u32 codecs, int is_callwaiting)
{
    cc_setup_t msg = {};

    ast_log(LOG_DEBUG, "sending CC_SETUP: hs_id %d; pcm_chan %d; cid:%s;"
	"cname=%s; codecs:0x%x\n", hs_id, pcm_chan, cid_number, cid_name,
	   codecs);
    msg.dev = hs_id;
    msg.pcm_chan = pcm_chan >> 1;
    msg.codec_bm = ast_codec_bm_to_vf(codecs);
    msg.ringtone = is_callwaiting? DECT_IE_TONE_TYPE_CALL_WAITING : 
	    DECT_IE_TONE_TYPE_OFF;
    if (cid_number)
	strncpy(msg.cid_number, cid_number, MAX_DECT_NUMBER_LEN);
    if(cid_name)
	strncpy(msg.cid_name, cid_name, MAX_DECT_NAME_LEN);

    return msg_send(DECT_CC_SETUP, &msg, sizeof(cc_setup_t));
}

int dect_cc_setupack(u8 hs_id, u32 cr, u8 pcm_chan, int codec)
{
	cc_setupack_t msg = {};

	ast_log(LOG_DEBUG, "sending CC_SETUPACK: hs_id %d; cr %u; pcm_chan %u;"
	    "codec 0x%x\n",hs_id, cr, pcm_chan, codec);

	msg.dev = hs_id;
	msg.cr = cr;
	msg.pcm_chan = pcm_chan >> 1;
	msg.codec = ast_codec_to_vf(codec);
	
	return msg_send(DECT_CC_SETUPACK, &msg, sizeof(cc_setupack_t));
}

int dect_cc_connect(u8 hs_id, u32 cr, int codec)
{
	cc_connect_t msg = {};

	ast_log(LOG_DEBUG, "sending CC_CONNECT: hs_id %d; cr %u; codec 0x%x\n",
	    hs_id, cr, codec);
	
	msg.dev = hs_id;
	msg.cr = cr;
	msg.codec = ast_codec_to_vf(codec);

	return msg_send(DECT_CC_CONNECT, &msg, sizeof(cc_connect_t));
}

int dect_cc_disconnect(u8 hs_id, u32 cr, u8 final)
{
	cc_disconnect_t msg = {};

	ast_log(LOG_DEBUG, "sending CC_DISCONNECT: hs %d; cr %d, final %d\n",
	    hs_id, cr, final);

	msg.dev = hs_id;
	msg.cr = cr;
	msg.preserve_pcm = final ? 0 : 1;

	return msg_send(DECT_CC_DISCONNECT, &msg, sizeof(cc_disconnect_t));
}

int dect_cc_release(u8 hs_id, u32 cr, u8 final)
{
	cc_release_t msg = {};

	ast_log(LOG_DEBUG, "sending CC_RELEASE: hs_id %d; cr %d\n", hs_id, cr);

	msg.dev = hs_id;
	msg.cr = cr;
	msg.preserve_pcm = final ? 0 : 1;

	return msg_send(DECT_CC_RELEASE, &msg, sizeof(cc_release_t));
}

int dect_cc_reject(u8 hs_id, u32 cr)
{
	cc_reject_t msg = {};

	ast_log(LOG_DEBUG, "sending CC_REJECT: hs_id %d; cr %d\n", hs_id, cr);

	msg.dev = hs_id;
	msg.cr = cr;
	msg.reason = DECT_REASON_SYSTEM;

	return msg_send(DECT_CC_REJECT, &msg, sizeof(cc_reject_t));
}

int dect_cc_hold(u8 hs_id, u32 cr)
{
    cc_callref_t msg = {};

    ast_log(LOG_DEBUG, "sending CC_HOLD: hs_id %d; cr %d\n", hs_id, cr);

    msg.dev = hs_id;
    msg.cr = cr;

    return msg_send(DECT_CC_HOLD, &msg, sizeof(cc_callref_t));
}

int dect_cc_unhold(u8 hs_id, u32 cr)
{
    cc_callref_t msg = {};

    ast_log(LOG_DEBUG, "sending CC_UNHOLD: hs_id %d; cr %d\n", hs_id, cr);

    msg.dev = hs_id;
    msg.cr = cr;

    return msg_send(DECT_CC_UNHOLD, &msg, sizeof(cc_callref_t));
}

int dect_cc_callstatus(u8 hs_id, u32 cr, u32 cr2, int cstat)
{
    cc_callstatus_t msg = {};

    msg.callstatus = call_status_map(cstat, CODE_2_DECT);

    ast_log(LOG_DEBUG, "sending CC_CALLSTATUS: hs_id %d; cr %d; cr2 %d;" \
	"status %s\n", hs_id, cr, cr2, vf_cstat_str_get(msg.callstatus));

    msg.dev = hs_id;
    msg.cr = cr;
    if(cr2)  /*TODO: ask Rolf about this parameter*/
	msg.cr2 = cr2;

    return msg_send(DECT_CC_CALLSTATUS, &msg, sizeof(cc_callstatus_t));	
}

int dect_cc_mwi(u8 msg_count, int line_id)
{
    cc_mwi_t msg = {};

    msg.count = msg_count;
    msg.lineid = line_id;
    msg.type = MWI_TYPE_VOICE; /*only voice supported for now*/
    msg.hs_bitmap = 0x3f; /*all handsets*/

    return msg_send(DECT_CC_MWI, &msg, sizeof(cc_mwi_t));	
}

int dect_cc_read_and_dispatch(int *id, int sock, short events, void *ignore)
{
	static char readbuf[4096];
	_DECT_FIFO_MSG_T *hdr = (_DECT_FIFO_MSG_T *)readbuf;
	int res, handled = 0;
	cc_message_t msg = {};
	char digit_str[2] = {};

	res = recv(sock, readbuf, sizeof(readbuf) - 1, 0);

	if (res <= 0) 
	{
		ast_log(LOG_WARNING, "DECT CC: recv failed! res = %d\n", res);
		return 1;
	}

	switch (hdr->msgid)
	{
	case DECT_CC_SETUP:
		{
			cc_setup_t *data = (cc_setup_t *)hdr->data;

			ast_log(LOG_DEBUG, "CC_SETUP vf_codec_bm:0x%x\n",
			    data->codec_bm);
			msg.cr = data->cr;
			msg.hs_id = data->dev;
			msg.codecs = vf_codec_bm_to_ast(data->codec_bm);
			msg.u.dialed_digits = data->called_number;
			break;
		}
	case DECT_CC_SETUPACK:   
		{
			cc_setupack_t *data = (cc_setupack_t *)hdr->data;

			ast_log(LOG_DEBUG, "CC_SETUPACK vf_codec:0x%x\n",
			    data->codec);
			msg.cr = data->cr;
			msg.hs_id = data->dev;
			msg.codecs = vf_codec_to_ast(data->codec);
			break;
		}
	case DECT_CC_ALERTING:     
		{
			cc_alerting_t *data = (cc_alerting_t *)hdr->data;

			msg.cr = data->cr;
			msg.hs_id = data->dev;
			msg.codecs = vf_codec_to_ast(data->codec);
			break;
		}
	case DECT_CC_CONNECT:      
		{
			cc_connect_t *data = (cc_connect_t *)hdr->data;

                        ast_log(LOG_DEBUG, "CC_CONNECT vf_codec:0x%x\n",
			    data->codec);
			msg.cr = data->cr;
			msg.hs_id = data->dev;
			msg.codecs = vf_codec_to_ast(data->codec);
			break;
		}
	case DECT_CC_CONNECTACK:   
		{
			cc_connectack_t *data = (cc_connectack_t *)hdr->data;

			msg.cr = data->cr;
			msg.hs_id = data->dev;
			break;
		}
	case DECT_CC_DISCONNECT:    
		{
			cc_disconnect_t *data = (cc_disconnect_t *)hdr->data;

			ast_log(LOG_DEBUG, "CC_DISCONNECT reason %d\n", data->reason);

			msg.cr = data->cr;
			msg.hs_id = data->dev;
			msg.u.reason.code = data->reason; /* TODO: convert */
			break;
		}
	case DECT_CC_RELEASE:      
		{
			cc_release_t *data = (cc_release_t *)hdr->data;

			ast_log(LOG_DEBUG, "CC_RELEASE status %d\n", data->status);

			msg.cr = data->cr;
			msg.hs_id = data->dev;
			msg.u.reason.code = data->status; /* TODO: convert */
			break;
		}
	case DECT_CC_REJECT:       
		{
			cc_reject_t *data = (cc_reject_t *)hdr->data;

			ast_log(LOG_DEBUG, "CC_REJECT reason %d; text %s\n", data->reason,
				data->reasontext);

			msg.cr = data->cr;
			msg.hs_id = data->dev;
			msg.u.reason.code = data->reason;
			msg.u.reason.desc = data->reasontext;
			break;
		}
	case DECT_CC_PROCEEDING:  
		{
			cc_callref_t *data = (cc_callref_t *)hdr->data;

			msg.cr = data->cr;
			msg.hs_id = data->dev;
			break;
		}
	case DECT_CC_DIGIT:
		{
			cc_digit_t *data = (cc_digit_t *)hdr->data;

			msg.hs_id = data->dev;
			digit_str[0] = data->val;
			msg.u.dialed_digits = digit_str;
			break;
		}
	case DECT_CC_HOLD:
		{
		    cc_callref_t *data = (cc_callref_t *)hdr->data;
		    
		    msg.cr = data->cr;
		    msg.hs_id = data->dev;
		    break;
		}
	case DECT_CC_UNHOLD:
		{
		    cc_callref_t *data = (cc_callref_t *)hdr->data;
		    
		    msg.cr = data->cr;
		    msg.hs_id = data->dev;
		    break;
		}
        case DECT_CC_3PTY:
		{
		    cc_conference_t *data = (cc_conference_t *)hdr->data;

		    msg.cr = data->cr1;
		    msg.cr2 = data->cr2;
		    msg.hs_id = data->dev;
		    break;
		}

	case DECT_CC_CALLSTATUS:
		{
			cc_callstatus_t *data = (cc_callstatus_t*)hdr->data;

			ast_log(LOG_DEBUG, "Got call status for cr %d - %s\n", 
				data->cr, vf_cstat_str_get(data->callstatus));
		    
			msg.cr = data->cr;
			msg.hs_id = data->dev;
			msg.u.call_status = call_status_map(data->callstatus, CODE_2_AST);
			break;		    
		}
	case DECT_CC_TRANSFER:
	    {
			cc_transfer_t *data = (cc_transfer_t *)hdr->data;
			msg.cr = data->cr1;
			msg.cr2 = data->cr2;
			msg.hs_id = data->dev;
			break;
	    }
	default:
		ast_log(LOG_ERROR, "Don't know how to handle msg \'%s\'(%d)\n", 
			vf_cc_msg_str_get(hdr->msgid), hdr->msgid);
		return 1;
	}

	if (handled)
		return 1;

	ast_log(LOG_DEBUG, "got %s; hs_id %d; cr %d; codec 0x%x\n",
		vf_cc_msg_str_get(hdr->msgid), msg.hs_id, msg.cr, msg.codecs);

	msg.id = msg_code_map(hdr->msgid, CODE_2_AST);
	cc_data.handle_cb(&msg);

	return 1;
}
