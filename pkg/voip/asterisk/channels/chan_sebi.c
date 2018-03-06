/*
 * chan_sebi 
 * channel module for usb 3g modems
 * Jose A. Deniz <odicha@hotmail.com>
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

/*! \file
 *
 * \brief "Sebi" Modem Device channel driver
 *
 * \author Jose A. Deniz <odicha@hotmail.com>
 *
 * \Portions taken from chan_mobile
 *
 * \Portions taken from OpenSer
 *
 * \ingroup channel_drivers
 */

/*** MODULEINFO
        <depend></depend>
 ***/

#include <asterisk.h>

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <termios.h>
#include <dirent.h>

#include <asterisk/lock.h>
#include <asterisk/channel.h>
#include <asterisk/config.h>
#include <asterisk/logger.h>
#include <asterisk/module.h>
#include <asterisk/pbx.h>
#include <asterisk/options.h>
#include <asterisk/utils.h>
#include <asterisk/linkedlists.h>
#include <asterisk/cli.h>
#include <asterisk/devicestate.h>
#include <asterisk/causes.h>
#include <asterisk/app.h>
#include <asterisk/manager.h>
#include <asterisk/callerid.h>
#include "chan_sebi.h"
#ifdef DSP_ENABLED
#include "asterisk/jdsp_common.h"
#endif
#include "asterisk/incall_announcement.h"

#define MODEM_CONFIG "sebi.conf"

#define DEVICE_SRATE 8000

/* From 3GPP 27.007 */
#define CLI_VALID 0
#define CLI_WITHHELD 1
#define CLI_UNAVAILABLE 2

static const char desc[] = "Sebi Modem Device Channel Driver";
static int instant_dial_enabled = 0;

static unsigned char conf_file_md5[MD5_DIGEST_LEN];

enum modem_state {
        MODEM_STATE_PREIDLE,
        MODEM_STATE_IDLE,
        MODEM_STATE_DIAL,
        MODEM_STATE_DIAL1,
        MODEM_STATE_OUTGOING,
        MODEM_STATE_RING,
        MODEM_STATE_RING2,
        MODEM_STATE_RING3,
        MODEM_STATE_HANGUP,
};

typedef struct {
	unsigned int tx_frames;
	unsigned int tx_octets;
	unsigned int rx_frames;
	unsigned int rx_octets;
} audio_stats_t;

struct modem_pvt {
        struct ast_channel *owner;                      /* Channel we belong to, possibly NULL */
        struct ast_frame fr;                            /* "null" frame */
        char id[31];                                    /* The id from sebi.conf */
        int group;                                      /* group number for group dialling */
        char voice_port[15];                    /* voice serial port of device */
        char context[AST_MAX_CONTEXT];                  /* the context for incoming calls */
        char connected;                                 /* is it connected? */
        char data_buf[256];
        char arg_buf[256];                               /* Buffer for notification arguments */
        char *io_buf;
        char *io_save_buf;
        int io_save_len;
        int io_pipe[2];
        int voice_socket;                                       /* voice socket descriptor */
        enum modem_state state;                         /* monitor thread current state */
        pthread_t monitor_thread;                       /* monitor thread handle */
		int monitor_enable;
        pthread_t read_thread;                          /* voice reading thread handle */
        char *read_buf;
        char dial_number[AST_MAX_EXTENSION];            /* number for the monitor thread to dial */
        int dial_timeout;
        unsigned int sent_answer:1;
        unsigned int do_hangup:1;
	unsigned int has_session:1;
        char hangup_count;
        ast_mutex_t lock;
        int mark;
		int format;
		int ptime;
		int rx_gain_shift;
		audio_stats_t stats;
        AST_LIST_ENTRY(modem_pvt) entry;
		int instant_dial_added;
};

static AST_LIST_HEAD_STATIC(devices, modem_pvt);

/* CLI stuff */
static char show_usage[] =
"Usage: sebi show devices\n" 
"       Shows the state of Modem devices.\n";

static int handle_cli_modem_show_devices(int fd, int argc, char **argv);

static int voice_write(int s, char *buf, int len);
static int voice_read(int s, char *buf, int len); 
static int voice_connect(struct modem_pvt *pvt);
static void stop_voice(struct modem_pvt *pvt);

static struct ast_cli_entry modem_cli[] = {
        {{"sebi", "show", "devices", NULL}, handle_cli_modem_show_devices, "Show Modem devices", show_usage},
};

/* App stuff */
static char *app_sebistatus = "SebiStatus";
static char *sebistatus_synopsis = "SebiStatus(Device,Variable)";
static char *sebistatus_desc =
"SebiStatus(Device,Variable)\n"
"  Device - Id of sebi device from sebi.conf\n"
"  Variable - Variable to store status in will be 1-3.\n" 
"             In order, Disconnected, Connected & Free, Connected & Busy.\n";

static char *app_ussd = "USSD";
static char *ussd_synopsis = "USSD(String)";
static char *ussd_desc = 
"USSD(String)\n"
"  String = string to send as USSD\n";

static struct ast_channel *modem_new(int state, struct modem_pvt *pvt, char *cid_num, char *cid_name, int cid_validity);
static struct ast_channel *modem_request(const char *type, const struct ast_codec_pref *format, void *data, int *cause);
static int modem_call(struct ast_channel *ast, char *dest, int timeout);
static int modem_hangup(struct ast_channel *ast);
static int modem_answer(struct ast_channel *ast);
static int modem_digit_begin(struct ast_channel *ast, char digit);
static int modem_digit_end(struct ast_channel *ast, char digit);
static struct ast_frame *modem_read(struct ast_channel *ast);
static int modem_write(struct ast_channel *ast, struct ast_frame *frame);
static int modem_fixup(struct ast_channel *oldchan, struct ast_channel *newchan);
static int modem_devicestate(void *data);
static int modem_indicate(struct ast_channel *ast, int condition,
    const void *data, size_t datalen);
static int do_state(struct modem_pvt *pvt);

#define NATIVE_FORMATS ((AST_FORMAT_MAX_AUDIO << 1) - 1)

static const struct ast_channel_tech modem_tech = {
        .type = "Sebi",
        .description = "Sebi Device Channel Driver",
        .capabilities = NATIVE_FORMATS,
        .requester = modem_request,
        .call = modem_call,
        .hangup = modem_hangup,
        .answer = modem_answer,
        .send_digit_begin = modem_digit_begin,
        .send_digit_end = modem_digit_end,
        .read = modem_read,
        .write = modem_write,
        .fixup = modem_fixup,
        .devicestate = modem_devicestate,
	.indicate = modem_indicate,
};

static int device_frame_size(struct modem_pvt *pvt)
{
	return ast_codec_get_len(pvt->format, DEVICE_SRATE * pvt->ptime / 1000);
}

/* CLI Commands implementation */

static int handle_cli_modem_show_devices(int fd, int argc, char **argv)
{
        struct modem_pvt *pvt;
        
        char group[6];

#define FORMAT1 "%-15.15s %-15.15s %-9.9s %-5.5s %-6.6s\n"

        if (argc != 3)
                return RESULT_SHOWUSAGE;

        ast_cli(fd, FORMAT1, "ID", "Group", "Connected", "State", "GainSh");
        AST_LIST_LOCK(&devices);
        AST_LIST_TRAVERSE(&devices, pvt, entry) {
                
                snprintf(group, 5, "%d", pvt->group);
                ast_cli(fd, FORMAT1, pvt->id, group, pvt->connected ? "Yes" : "No",
                        (pvt->state == MODEM_STATE_IDLE) ? "Free" : (pvt->state
						< MODEM_STATE_IDLE) ? "Init" : "Busy", pvt->rx_gain_shift ? "Yes" : "No");
        }
        AST_LIST_UNLOCK(&devices);

#undef FORMAT1

        return RESULT_SUCCESS;
}

static int send_3g_request(char *type, char *data)
{
        return manager_event(EVENT_FLAG_CALL, "3GRequest", "Type: %s\r\n"
                "Data: %s\r\n", type, data ?: "");
}

static int manager_handle_action(char *type, char *data)
{
        struct modem_pvt *pvt;

        /* Since we support only one device, get pvt from first one */
        pvt = devices.first;
        if (!pvt)
                return -1;

        ast_mutex_lock(&pvt->lock);
        strncpy(pvt->data_buf, type, sizeof(pvt->data_buf));
        strncpy(pvt->arg_buf, data ?: "", sizeof(pvt->arg_buf));
        do_state(pvt);
        ast_mutex_unlock(&pvt->lock);
        /* Wake monitor loop */
        pthread_kill(pvt->monitor_thread, SIGURG);

        return 0;
}

static int manager_action_3g_response(struct mansession *s, struct message *m)
{
        char *reply = astman_get_header(m, "Reply");

        ast_log(LOG_DEBUG, "3G Response - Reply: %s\n", reply);

        manager_handle_action(reply, NULL);

        return 0;
}

static int manager_action_3g_notify(struct mansession *s, struct message *m)
{
        char *type = astman_get_header(m, "Type");
        char *data = astman_get_header(m, "Data");

        ast_log(LOG_DEBUG, "3G Notify - Type: %s, Data: %s\n", astman_get_header(m,
                "Type"), astman_get_header(m, "Data"));

        manager_handle_action(type, data);

        return 0;
}

/*

        Dialplan applications implementation

*/

static int modem_status_exec(struct ast_channel *ast, void *data)
{

        struct modem_pvt *pvt;
        char *parse;
        int stat;
        char status[2];

        AST_DECLARE_APP_ARGS(args,
                AST_APP_ARG(device);
                AST_APP_ARG(variable);
        );

        if (ast_strlen_zero(data))
                return -1;

        parse = ast_strdupa(data);

        AST_STANDARD_APP_ARGS(args, parse);

        if (ast_strlen_zero(args.device) || ast_strlen_zero(args.variable))
                return -1;

        stat = 1;

        AST_LIST_LOCK(&devices);
        AST_LIST_TRAVERSE(&devices, pvt, entry) {
                if (!strcmp(pvt->id, args.device))
                        break;
        }
        AST_LIST_UNLOCK(&devices);

        if (pvt->connected)
                stat = 2;
        if (pvt->owner)
                stat = 3;

        snprintf(status, sizeof(status), "%d", stat);
        pbx_builtin_setvar_helper(ast, args.variable, status);

        return 0;

}

static int modem_ussd_exec(struct ast_channel *ast, void *data)
{
	char *parse;

	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(string);
		);

	if (ast_strlen_zero(data))
		return -1;

	parse = ast_strdupa(data);

	AST_STANDARD_APP_ARGS(args, parse);

	if (ast_strlen_zero(args.string))
		return -1;

	send_3g_request(REQUEST_USSD, args.string);

	return 0;
}

/*

        Channel Driver callbacks

*/

static int cli_validity_to_ast(int validity)
{
        switch (validity)
        {
        case CLI_VALID:
                return AST_PRES_ALLOWED;
        case CLI_WITHHELD:
                return AST_PRES_RESTRICTED;
        case CLI_UNAVAILABLE:
                return AST_PRES_UNAVAILABLE;
        default:
                return AST_PRES_ALLOWED;
        }
}

static struct ast_channel *modem_new(int state, struct modem_pvt *pvt, char *cid_num, char *cid_name, int validity)
{
        struct ast_channel *chn;

        if (pipe(pvt->io_pipe) == -1) {
                ast_log(LOG_ERROR, "Failed to create io_pipe.\n");
                return NULL;
        }

        pvt->io_save_len = 0;
        pvt->sent_answer = 0;
        pvt->do_hangup = 1;
	pvt->has_session = 0;
        chn = ast_channel_alloc(1);
        if (chn) {
                chn->tech = &modem_tech;
		ast_codec_pref_init(&chn->nativeformats);
		ast_codec_pref_append(&chn->nativeformats, pvt->format);
                chn->rawreadformat = pvt->format;
                chn->rawwriteformat = pvt->format;
                chn->writeformat = pvt->format;
                chn->readformat = pvt->format;
                chn->tech_pvt = pvt;
                chn->fds[0] = pvt->io_pipe[0];
                if (state == AST_STATE_RING)
                        chn->rings = 1;
                pvt->owner = chn;
		ast_set_callerid(chn, cid_num, cid_name, cid_num);
		chn->cid.cid_pres = cli_validity_to_ast(validity);
		chn->_state = state;
		chn->ptime = pvt->ptime;
#ifdef DSP_ENABLED
		jdsp_build_callid(chn->shared_callid, AST_MAX_SHARED_CALLID);
#endif
		if (!ast_strlen_zero(pvt->context))
			ast_copy_string(chn->context, pvt->context, sizeof(chn->context));
		snprintf(chn->name, sizeof(chn->name), "Sebi/%s-%04lx", pvt->id, random() & 0xffff);
                pbx_builtin_setvar_helper(chn, "DISABLE_VAD", "YES");
                pbx_builtin_setvar_helper(chn, "SUPPRESS_DTMF", "NO");
                return chn;
        }

        return NULL;

}

static struct ast_channel *modem_request(const char *type, const struct ast_codec_pref *format, void *data, int *cause)
{

        struct ast_channel *chn = NULL;
        struct modem_pvt *pvt;
        char *dest_dev = NULL;
        char *dest_num = NULL;
        int group = -1;

        if (!data) {
                ast_log(LOG_WARNING, "Channel requested with no data\n");
                *cause = AST_CAUSE_INCOMPATIBLE_DESTINATION;
                return NULL;
        }

        dest_dev = ast_strdupa((char *)data);

        dest_num = strchr(dest_dev, '/');
        if (dest_num)
                *dest_num++ = 0x00;

        if (((dest_dev[0] == 'g') || (dest_dev[0] == 'G')) && ((dest_dev[1] >= '0') && (dest_dev[1] <= '9'))) {
                group = atoi(&dest_dev[1]);
        }

        AST_LIST_LOCK(&devices);
        AST_LIST_TRAVERSE(&devices, pvt, entry) {
                if (group > -1 && pvt->group == group && pvt->connected && !pvt->owner) {
                        break;
                } else if (!strcmp(pvt->id, dest_dev)) {
                        break;
                }
        }
        AST_LIST_UNLOCK(&devices);
        if (!pvt || !pvt->connected || pvt->owner) {
                ast_log(LOG_WARNING, "Request to call on device %s which is not connected / already in use.\n", dest_dev);
                *cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
                return NULL;
        }

        if (!(ast_codec_pref_bits(format) & pvt->format)) {
                ast_log(LOG_WARNING, "Asked to get a channel of unsupported format '%d'\n", ast_codec_pref_bits(format));
                *cause = AST_CAUSE_FACILITY_NOT_IMPLEMENTED;
                return NULL;
        }

        if (!dest_num) {
                ast_log(LOG_WARNING, "Can't determine destination number.\n");
                *cause = AST_CAUSE_INCOMPATIBLE_DESTINATION;
                return NULL;
        }

        chn = modem_new(AST_STATE_DOWN, pvt, NULL, NULL, CLI_VALID);
        if (!chn) {
                ast_log(LOG_WARNING, "Unable to allocate channel structure.\n");
                *cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
                return NULL;
        }

        return chn;

}

static int modem_call(struct ast_channel *ast, char *dest, int timeout)
{
        
        struct modem_pvt *pvt;
        char *dest_dev = NULL;
        char *dest_num = NULL;

        

        dest_dev = ast_strdupa((char *)dest);

        pvt = ast->tech_pvt;

        dest_num = strchr(dest_dev, '/');
        if (!dest_num) {
                ast_log(LOG_WARNING, "Cant determine destination number.\n");
                return -1;
        }
        *dest_num++ = 0x00;

        if ((ast->_state != AST_STATE_DOWN) && (ast->_state != AST_STATE_RESERVED)) {
                ast_log(LOG_WARNING, "modem_call called on %s, neither down nor reserved\n", ast->name);
                return -1;
        }

        ast_log(LOG_DEBUG, "Calling %s on %s\n", dest, ast->name);

        ast_mutex_lock(&pvt->lock);
        ast_copy_string(pvt->dial_number, dest_num, sizeof(pvt->dial_number));
        pvt->state = MODEM_STATE_DIAL;
        pvt->dial_timeout = (timeout == 0) ? 30 : timeout;
        ast_mutex_unlock(&pvt->lock);

        return 0;

}

static void print_debug_stats(struct modem_pvt *p)
{
	char msg[256];

	snprintf(msg, sizeof(msg), "RxPackets: %d, TxPackets: %d, "
		"RxOctets: %d, TxOctets: %d",
		p->stats.rx_frames, p->stats.tx_frames, p->stats.rx_octets,
		p->stats.tx_octets);

	ast_log(LOG_DEBUG, "Stats for sebi - %s\n", msg);
}

static int modem_hangup(struct ast_channel *ast)
{

        struct modem_pvt *pvt;

        if (!ast->tech_pvt) {
                ast_log(LOG_WARNING, "Asked to hangup channel not connected\n");
                return 0;
        }
        pvt = ast->tech_pvt;

        ast_log(LOG_DEBUG, "Hanging up device %s.\n", pvt->id);

        ast->fds[0] = -1;

		print_debug_stats(pvt);
        
        ast_mutex_lock(&pvt->lock);

        stop_voice(pvt);

        close(pvt->io_pipe[0]);
        close(pvt->io_pipe[1]);

        if (pvt->state == MODEM_STATE_OUTGOING || pvt->state == MODEM_STATE_DIAL1 || pvt->state == MODEM_STATE_RING3) {
                if (pvt->do_hangup) {
                        send_3g_request(REQUEST_HANGUP, NULL);
                }
                pvt->state = MODEM_STATE_HANGUP;
                pvt->hangup_count = 0;
        } else
                pvt->state = MODEM_STATE_IDLE;

	if (pvt->has_session)
	{
	    manager_event(EVENT_FLAG_SYSTEM, "SessionChanged",
		"Established: 0\r\n"
		"Peer: gsm\r\n");
	    pvt->has_session = 0;
	}
        pvt->owner = NULL;
        ast->tech_pvt = NULL;
        ast_mutex_unlock(&pvt->lock);

        ast_setstate(ast, AST_STATE_DOWN);

        return 0;

}

static int modem_answer(struct ast_channel *ast)
{

        struct modem_pvt *pvt;

        pvt = ast->tech_pvt;
        ast_mutex_lock(&pvt->lock);
        send_3g_request(REQUEST_ANSWER, NULL);
        ast_setstate(ast, AST_STATE_UP);
        pvt->sent_answer = 1;
	pvt->has_session = 1;
	manager_event(EVENT_FLAG_SYSTEM, "SessionChanged",
	    "Established: 1\r\n"
	    "Peer: gsm\r\n");
        ast_mutex_unlock(&pvt->lock);

        return 0;

}

static int modem_digit_begin(struct ast_channel *chan, char digit)
{

        return 0;

}

static int modem_digit_end(struct ast_channel *ast, char digit)
{

        struct modem_pvt *pvt;
        char buf[2] = { digit, '\0' };

        pvt = ast->tech_pvt;
		char *suppress_dtmf = pbx_builtin_getvar_helper(ast,
			"SUPPRESS_DTMF");

		ast_log(LOG_DEBUG, "Dialed %c, suppressdtmf: %s\n", digit,
			suppress_dtmf);

		/* suppress_dtmf  defined to "NO" -not suppressed(in band)
			- no event should rise */
		if (suppress_dtmf && !strcmp(suppress_dtmf, "NO"))
	    	return 0;
	
        switch(digit) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case '*':
        case '#':
                send_3g_request(REQUEST_DTMF, buf);
                break;
        default:
                ast_log(LOG_WARNING, "Unknown digit '%c'\n", digit);
                return -1;
        }

        return 0;

}

struct ast_frame ast_null_frame = { AST_FRAME_NULL, };

void ast_shift_right_samples(void *dst, const void *src, int samples, int shift)
{
	int i;
	unsigned short *dst_s = dst;
	const unsigned short *src_s = src;

	for (i=0; i<samples; i++)
		dst_s[i] = src_s[i]<<shift;
}

static struct ast_frame *modem_read(struct ast_channel *ast)
{
        struct modem_pvt *pvt = ast->tech_pvt;
        int r;

        if (!pvt->owner) {
                return &ast_null_frame;
        }
        memset(&pvt->fr, 0x00, sizeof(struct ast_frame));
        pvt->fr.frametype = AST_FRAME_VOICE;
        pvt->fr.subclass = pvt->format;
        pvt->fr.src = "Sebi";
        pvt->fr.offset = AST_FRIENDLY_OFFSET;
        pvt->fr.mallocd = 0;
        pvt->fr.delivery.tv_sec = 0;
        pvt->fr.delivery.tv_usec = 0;
        pvt->fr.data = pvt->io_buf + AST_FRIENDLY_OFFSET;

		if ((r = read(pvt->io_pipe[0], pvt->fr.data, device_frame_size(pvt))) == -1) {
			    ast_log(LOG_ERROR, "read error %d\n", errno);
                return &ast_null_frame;
		}
		pvt->fr.datalen = r;
        pvt->fr.samples = ast_codec_get_samples(&pvt->fr);
		pvt->stats.rx_frames++;
		pvt->stats.rx_octets += r;

		if (pvt->fr.subclass == AST_FORMAT_SLINEAR)
		{
			ast_swapcopy_samples(pvt->fr.data, pvt->fr.data,
				pvt->fr.samples);
			if (pvt->rx_gain_shift)
				ast_shift_right_samples(pvt->fr.data, pvt->fr.data,
					pvt->fr.samples, pvt->rx_gain_shift);

		}

        return &pvt->fr;

}

static int manager_modem_reset_stats(struct mansession *s, struct message *m)
{
	char *device = astman_get_header(m, "Device");
	struct modem_pvt *pvt;

	AST_LIST_LOCK(&devices);
	AST_LIST_TRAVERSE(&devices, pvt, entry) {
		ast_log(LOG_DEBUG,"Device found - %s\n", pvt->id);
		if (!strcmp(pvt->id, device))
			break;
	}
	AST_LIST_UNLOCK(&devices);

	if (!pvt)
	{
		astman_send_error(s, m, "Device not found");
		return 0;
	}

	memset(&pvt->stats, 0, sizeof(audio_stats_t));	

	astman_send_ack(s, m, "Statistics deleted");
	return 0;
}

static int manager_modem_get_stats(struct mansession *s, struct message *m)
{
	char *device = astman_get_header(m, "Device");
	struct modem_pvt *pvt;
	char msg[256] = "";

	AST_LIST_LOCK(&devices);
	AST_LIST_TRAVERSE(&devices, pvt, entry) {
		ast_log(LOG_DEBUG,"Device found - %s\n", pvt->id);
		if (!strcmp(pvt->id, device))
			break;
	}
	AST_LIST_UNLOCK(&devices);

	if (!pvt)
	{
		astman_send_error(s, m, "Device not found");
		return 0;
	}

	snprintf(msg, sizeof(msg), "\r\n"
		"RxPackets: %d\r\n"
		"TxPackets: %d\r\n"
		"RxOctets: %d\r\n"
		"TxOctets: %d\r\n",
		pvt->stats.rx_frames, pvt->stats.tx_frames, pvt->stats.rx_octets,
		pvt->stats.tx_octets);

	astman_send_ack(s, m, msg);
	return 0;
}

static void *do_read(void *data)
{
        struct modem_pvt *pvt = (struct modem_pvt *)data;
	struct ast_channel *ast = pvt->owner;
        char *buf = pvt->read_buf;
	int r;

	do {
                if ((r = voice_read(pvt->voice_socket, buf, device_frame_size(pvt)))) {
                        if (ast->_state == AST_STATE_UP || ast->_state == AST_STATE_RINGING)     // Dont queue the audio in the pipe if the call is not up yet. just toss it. 
                                voice_write(pvt->io_pipe[1], buf, r);
                }
		else
		{
		    ast_mutex_lock(&pvt->lock);
		    close(pvt->voice_socket);
		    pvt->voice_socket = -1;
		    ast_mutex_unlock(&pvt->lock);
		    ast_log(LOG_DEBUG, "Voice read timeout\n");
		}

	} while (r);

        pvt->read_thread = AST_PTHREADT_NULL;
	return NULL;
}

static void start_read(struct modem_pvt *pvt)
{
        ast_mutex_lock(&pvt->lock);

        if (pvt->read_thread != AST_PTHREADT_NULL) {
                ast_mutex_unlock(&pvt->lock);
                return;
        }

        if (!pvt->read_buf &&
            !(pvt->read_buf = malloc(device_frame_size(pvt)))) {
                ast_log(LOG_ERROR, "Error allocating read buffer\n");
                ast_mutex_unlock(&pvt->lock);
                return;
        }

        if (ast_pthread_create(&pvt->read_thread, NULL, do_read, pvt) < 0) {
	        ast_log(LOG_ERROR, "Error creating reading thread\n");
                pvt->read_thread = AST_PTHREADT_NULL;
        }

	ast_log(LOG_DEBUG, "Start voice reading thread\n");
        ast_mutex_unlock(&pvt->lock);
}

static void stop_voice(struct modem_pvt *pvt)
{
        ast_mutex_lock(&pvt->lock);

	if (pvt->read_thread != AST_PTHREADT_NULL) {
		pthread_cancel(pvt->read_thread);
		pthread_join(pvt->read_thread, NULL);
		pvt->read_thread = AST_PTHREADT_NULL;
	}

        if (pvt->read_buf) {
                free(pvt->read_buf);
                pvt->read_buf = NULL;
        }

        if (pvt->voice_socket != -1)
	{
                close(pvt->voice_socket);
		pvt->voice_socket = -1;
	}
		pvt->instant_dial_added = 0;

        ast_mutex_unlock(&pvt->lock);
}

static int modem_write(struct ast_channel *ast, struct ast_frame *frame)
{

        struct modem_pvt *pvt = ast->tech_pvt;
        int res, i, io_need, num_frames, frame_size;
        char *pfr;

        if (frame->frametype != AST_FRAME_VOICE) {
                return 0;
        }
	ast_mutex_lock(&pvt->lock);
        if (pvt->voice_socket < 0){ //Open Voice port if not open yet
                        if      ((res = voice_connect(pvt)) < 0){
                                        ast_log(LOG_DEBUG, "Error opening voice port (%s) from device %s.\n", pvt->voice_port, pvt->id);
					ast_mutex_unlock(&pvt->lock);
                                        return 0;
                        }else{
                                pvt->voice_socket = res;
				start_read(pvt);
                        }
        }

		if (frame->subclass == AST_FORMAT_SLINEAR)
		{
			ast_swapcopy_samples(frame->data, frame->data,
				frame->samples);
		}
		
		frame_size = device_frame_size(pvt);
        io_need = 0;
        if (pvt->io_save_len > 0) {
                io_need = frame_size - pvt->io_save_len;
                memcpy(pvt->io_save_buf + pvt->io_save_len, frame->data, io_need);
				pvt->stats.tx_frames++;
				pvt->stats.tx_octets += frame_size;
                voice_write(pvt->voice_socket, pvt->io_save_buf, frame_size);
        }

        num_frames = (frame->datalen - io_need) / frame_size;
        pfr = frame->data + io_need;

        for (i=0; i<num_frames; i++) {
			pvt->stats.tx_frames++;
			pvt->stats.tx_octets += frame_size;
			voice_write(pvt->voice_socket, pfr, frame_size);
			pfr += frame_size;
        }

        pvt->io_save_len = (frame->datalen - io_need) - (num_frames * frame_size);
        if (pvt->io_save_len > 0) {
                memcpy(pvt->io_save_buf, pfr, pvt->io_save_len);
        }
	ast_mutex_unlock(&pvt->lock);
        
        return 0;
 

}

static int modem_fixup(struct ast_channel *oldchan, struct ast_channel *newchan)
{

        struct modem_pvt *pvt = oldchan->tech_pvt;

        if (pvt && pvt->owner == oldchan)
                pvt->owner = newchan;

        return 0;

}

/*! \brief returns the equivalent of logic or for strings:
 * first one if not empty, otherwise second one.
 */
#define S_OR(a, b)	(!ast_strlen_zero(a) ? (a) : (b))

static int modem_devicestate(void *data)
{

        char *device;
        int res = AST_DEVICE_INVALID;
        struct modem_pvt *pvt;

        device = ast_strdupa(S_OR(data, ""));

        ast_log(LOG_DEBUG, "Checking device state for device %s\n", device);

        AST_LIST_LOCK(&devices);
        AST_LIST_TRAVERSE(&devices, pvt, entry) {
                if (!strcmp(pvt->id, device))
                        break;
        }
        AST_LIST_UNLOCK(&devices);

        if (pvt) {
                if (pvt->connected) {
                        if (pvt->owner)
                                res = AST_DEVICE_INUSE;
                        else
                                res = AST_DEVICE_NOT_INUSE;
                }
        }

        return res;

}

static int modem_indicate(struct ast_channel *ast, int condition,
    const void *data, size_t datalen)
{
    int res = -1;

    if (condition == AST_CONTROL_PLAY_DIAGNOSTICS)
    {
	res = incall_announcement_start_indicated(ast);
    }

    return res;
}

/*

        data  helpers

*/

static int voice_connect(struct modem_pvt *pvt)
{

        struct termios options;
        int res;

        res= open(pvt->voice_port, O_RDWR | O_NOCTTY );

        ast_log(LOG_DEBUG, "Opening port %s from device %s. Result (%d).\n", pvt->voice_port, pvt->id, res);
        if (res <0)
                return -1;
        options.c_cflag = B460800 | CRTSCTS | CS8 | CLOCAL | CREAD | O_NDELAY;
        options.c_iflag = IGNPAR;
        options.c_oflag = 0;
        options.c_lflag = 0;       
        options.c_cc[VMIN]=0;
        options.c_cc[VTIME]=10;
        tcflush(res, TCIOFLUSH);
        tcsetattr(res,TCSANOW,&options);


        return res;
}


/*

        Thread routines

*/
static int voice_write(int s, char *buf, int len)
{

        int r;
        
        if (s == -1) {
                
                ast_log(LOG_DEBUG, "voice_write() not ready\n");
                return 0;
        }

        r = write(s, buf, len);
        if (r == -1) {
                ast_log(LOG_DEBUG, "voice write error %d\n", errno);
                return 0;
        }

        return 1;

}

static int voice_read(int s, char *buf, int len)
{

        int r;
        
        if (s == -1) {
                ast_log(LOG_DEBUG, "voice_read() not ready\n");
                return 0;
        }

        r = read(s, buf, len);
        if (r == -1) {
                ast_log(LOG_DEBUG, "voice_read() error %d\n", errno);
                return 0;
        }

        return r;

} 

static void parse_clip(char *data, char **num, char **name, int *validity)
{
        *num = data;

        data = strchr(data, ',');
        *data = '\0';
        *name = ++data;

        data = strchr(data, ',');
        *data = '\0';
        *validity = atoi(++data);
}

static void notify_manager_instant_dial(struct modem_pvt *p)
{
	if(instant_dial_enabled && !p->instant_dial_added)
	{
		p->instant_dial_added = 1;
		manager_event(EVENT_FLAG_CALL, "AddInstantDial",
			"Number: %s\r\n",p->dial_number); 
	}
}

static int do_state(struct modem_pvt *pvt)
{
        struct ast_channel *chn;

        switch (pvt->state) {
        case MODEM_STATE_PREIDLE: // Nothing handled here, wait for timeout, then off we go... //
                break;
        case MODEM_STATE_IDLE:
                if (strstr(pvt->data_buf, NOTIFY_RING)) {
                        pvt->state = MODEM_STATE_RING;
                }
                break;

        case MODEM_STATE_DIAL: // Nothing handled here, we need to wait for a timeout 
                break;
        case MODEM_STATE_DIAL1:
                if (strstr(pvt->data_buf, RESPONSE_OK)) {
                        ast_setstate(pvt->owner, AST_STATE_RINGING);
						ast_queue_control(pvt->owner, AST_CONTROL_PROGRESS);
						notify_manager_instant_dial(pvt);
                }
                pvt->state = MODEM_STATE_OUTGOING;
                break;
        case MODEM_STATE_OUTGOING:
                if (strstr(pvt->data_buf, NOTIFY_HANGUP)) 
                        ast_queue_control(pvt->owner, AST_CONTROL_HANGUP);
                if (strstr(pvt->data_buf, NOTIFY_ANSWER))
				{
					ast_queue_control(pvt->owner, AST_CONTROL_ANSWER);
					notify_manager_instant_dial(pvt);
					pvt->has_session = 1;
					manager_event(EVENT_FLAG_SYSTEM, "SessionChanged",
					    "Established: 1\r\n"
					    "Peer: gsm\r\n");
				}
				if (strstr(pvt->data_buf, NOTIFY_BUSY))
				        ast_queue_control(pvt->owner, AST_CONTROL_BUSY);
                break;

        case MODEM_STATE_RING:
                {
                        char *num, *name;
                        int validity;
                        if (strstr(pvt->data_buf, NOTIFY_CLIP)) 
                        {
                                parse_clip(pvt->arg_buf, &num, &name, &validity);
                                chn = modem_new(AST_STATE_RING, pvt, num, name, validity);
                                if (chn) {
                                        if (ast_pbx_start(chn)) {
                                                ast_log(LOG_ERROR, "Unable to start pbx on incoming call.\n");
                                                /* The pvt lock is recursive, so no deadlock */
                                                ast_hangup(chn);
                                        } else
                                                pvt->state = MODEM_STATE_RING3;
                                } else {
                                        ast_log(LOG_ERROR, "Unable to allocate channel for incoming call.\n");
                                        send_3g_request(REQUEST_HANGUP, NULL);
                                        pvt->state = MODEM_STATE_IDLE;
                                }
                        }
                }
                break;
        case MODEM_STATE_RING2:
                chn = modem_new(AST_STATE_RING, pvt, "", "", CLI_UNAVAILABLE);
                if (chn) {
                        if (ast_pbx_start(chn)) {
                                ast_log(LOG_ERROR, "Unable to start pbx on incoming call.\n");
                                /* The pvt lock is recursive, so no deadlock */
                                ast_hangup(chn);
                        } else
                                pvt->state = MODEM_STATE_RING3;
                } else {
                        ast_log(LOG_ERROR, "Unable to allocate channel for incoming call.\n");
                        send_3g_request(REQUEST_HANGUP, NULL);
                        pvt->state = MODEM_STATE_IDLE;
                }
                break;
        case MODEM_STATE_RING3:
                if (strstr(pvt->data_buf, NOTIFY_HANGUP)) // Caller disconnected 
                        ast_queue_control(pvt->owner, AST_CONTROL_HANGUP);
                break;
        case MODEM_STATE_HANGUP:
                if (strstr(pvt->data_buf, RESPONSE_OK)) {
                        pvt->state = MODEM_STATE_IDLE;
                }
                break;
        }

        /* Clear buffers */
        pvt->data_buf[0] = '\0';
        pvt->arg_buf[0] = '\0';

        return 0;
}

static int do_state_timeout(struct modem_pvt *pvt)
{
        if (pvt->state == MODEM_STATE_PREIDLE) {
                int res;
                if ((res = voice_connect(pvt)) < 0) {
                        ast_log(LOG_ERROR, "Unable to open voice port %s. Retrying...\n", pvt->voice_port);
                        return 0;
                }else{
                        pvt->voice_socket = res;
                        /* OK, we see that we can open voice port.  Now close it
                         * until actually needed. */
                        stop_voice(pvt);
                }
                pvt->connected = 1;
                ast_verbose(VERBOSE_PREFIX_3 "Modem Device initialised and ready.\n");
                pvt->state = MODEM_STATE_IDLE;
        } else if (pvt->state == MODEM_STATE_DIAL) {
                if (send_3g_request(REQUEST_CALL, pvt->dial_number)) {
                        ast_log(LOG_ERROR, "Dial failed on %s state %d\n", pvt->owner->name, pvt->state);
                        ast_queue_control(pvt->owner, AST_CONTROL_CONGESTION);
                        pvt->state = MODEM_STATE_IDLE;
                } else {
                        pvt->state = MODEM_STATE_DIAL1;
                }
        } else if (pvt->state == MODEM_STATE_DIAL1) {
                ast_log(LOG_ERROR, "Dial failed on %s state %d\n", pvt->owner->name, pvt->state);
                ast_queue_control(pvt->owner, AST_CONTROL_CONGESTION);
                ast_queue_control(pvt->owner, AST_CONTROL_HANGUP);
                pvt->state = MODEM_STATE_IDLE;
        } else if (pvt->state == MODEM_STATE_RING) { // No CLIP?, bump it
                pvt->state = MODEM_STATE_RING2;
        }
	else if (pvt->state == MODEM_STATE_RING2)
	{
	    /* Only one RING?, bump it */
	    do_state(pvt);
        }
	else if (pvt->state == MODEM_STATE_HANGUP) {
                if (pvt->do_hangup) {
                        if (pvt->hangup_count == 6) {
                                ast_log(LOG_DEBUG, "Device %s failed to hangup after 6 tries, disconnecting.\n", pvt->id);
                                pvt->monitor_enable = 0;
                        }
                        send_3g_request(REQUEST_HANGUP, NULL);
                        pvt->hangup_count++;
                } else
                        pvt->state = MODEM_STATE_IDLE;
        }

        return 0;
}

static void *do_monitor_phone(void *data)
{

        struct modem_pvt *pvt = (struct modem_pvt *)data;
        int t;
		struct timeval tv;

        ast_log(LOG_WARNING, "Starting monitor loop...\n");
        pvt->monitor_enable = 1;

        ast_mutex_lock(&pvt->lock);
        while (pvt->monitor_enable) {
			    memset(&tv, 0, sizeof(tv));

                if (pvt->state == MODEM_STATE_DIAL1)
                        tv.tv_sec = pvt->dial_timeout;
                else if (pvt->state == MODEM_STATE_HANGUP)
                        tv.tv_sec = 2;
                else
                        tv.tv_sec = 1;

                ast_mutex_unlock(&pvt->lock);
                t = select(0, NULL, NULL, NULL, &tv);
                ast_mutex_lock(&pvt->lock);
				if (!t)
					do_state_timeout(pvt);
        }

        stop_voice(pvt);
        ast_mutex_unlock(&pvt->lock);

        pvt->connected = 0;
        pvt->monitor_thread = AST_PTHREADT_NULL;

        manager_event(EVENT_FLAG_SYSTEM, "SebiStatus", "Status: Disconnect\r\n"
                "Device: %s\r\n", pvt->id);

        return NULL;
}

static int start_monitor(struct modem_pvt *pvt)
{
        if (ast_pthread_create(&pvt->monitor_thread, NULL, do_monitor_phone, pvt)
                < 0) {
                pvt->monitor_thread = AST_PTHREADT_NULL;
                return 0;
        }
        return 1;
}

static int modem_load_config(int reloading)
{
        struct ast_config *cfg = NULL;
        char *cat = NULL;
        const char *voiceport, *context;
        struct modem_pvt *pvt = NULL;
		struct ast_codec_pref format;
		char *rx_gain_shift;

        cfg = ast_config_load(MODEM_CONFIG);
        if (!cfg)
                return 0;

        ast_log(LOG_WARNING, "Loaded sebi.conf...\n");

        cat = ast_category_browse(cfg, NULL);
        while (cat) {
		char *var_value;

                ast_log(LOG_DEBUG, "Loading device %s.\n", cat);
                voiceport = ast_variable_retrieve(cfg, cat, "voice");
                context = ast_variable_retrieve(cfg, cat, "context");
		ast_codec_pref_init(&format);
		if ((var_value = ast_variable_retrieve(cfg, cat, "disallow")))
			ast_parse_allow_disallow(&format, NULL, var_value, 0);
		if ((var_value = ast_variable_retrieve(cfg, cat, "allow")))
			ast_parse_allow_disallow(&format, NULL, var_value, 1);
		if ((var_value = ast_variable_retrieve(cfg, cat, "instantdialenabled")))
			instant_dial_enabled = atoi(var_value);
		rx_gain_shift = ast_variable_retrieve(cfg, cat, "rx_gain_shift");

                /* Find existing */
                AST_LIST_LOCK(&devices);
                AST_LIST_TRAVERSE(&devices, pvt, entry) {
                        if (!strcmp(pvt->id, cat))
	break;
                }
                AST_LIST_UNLOCK(&devices);

                if (!pvt && (pvt = calloc(1, sizeof(*pvt)))) {
                        ast_copy_string(pvt->voice_port, voiceport, sizeof(pvt->voice_port));
                        ast_copy_string(pvt->context, S_OR(context, "default"), sizeof(pvt->context));

                        ast_copy_string(pvt->id, cat, sizeof(pvt->id));

                        pvt->state = MODEM_STATE_PREIDLE;
                        pvt->voice_socket = -1;
                        pvt->monitor_thread = AST_PTHREADT_NULL;
                        pvt->read_thread = AST_PTHREADT_NULL;
						pvt->format = ast_codec_pref_bits(&format);
						pvt->ptime = ast_get_ptime_by_format(&format, pvt->format);

						pvt->io_buf = malloc(device_frame_size(pvt) +
							AST_FRIENDLY_OFFSET);
						pvt->io_save_buf = malloc(device_frame_size(pvt));

						pvt->instant_dial_added = 0;
						pvt->rx_gain_shift = rx_gain_shift ? atoi(rx_gain_shift) : 0;

						ast_log(LOG_DEBUG, "Format: %s:%d\n",
							ast_codec2str(pvt->format), pvt->ptime);

                        ast_mutex_init(&pvt->lock);

                        AST_LIST_LOCK(&devices);
                        AST_LIST_INSERT_HEAD(&devices, pvt, entry);
                        AST_LIST_UNLOCK(&devices);

                        if (start_monitor(pvt)) {
                                manager_event(EVENT_FLAG_SYSTEM, "ModemStatus", "Status: Connect\r\n");
                                if (option_verbose > 2)
	                            ast_verbose(VERBOSE_PREFIX_3 "Modem Device has connected.\n");
                        }

                }
                if (!pvt)
                {
                        ast_log(LOG_ERROR, "Couldn't allocate pvt!\n");
                        return 0;
                }

                if (reloading)
                        pvt->mark = 1;

                cat = ast_category_browse(cfg, cat);
        }

        ast_config_destroy(cfg);
        return 1;
}

static void destroy_device_list(int reloading)
{
        struct modem_pvt *pvt;

        AST_LIST_LOCK(&devices);
        AST_LIST_TRAVERSE_SAFE_BEGIN(&devices, pvt, entry) {
                if (!reloading || !pvt->mark)
                {
                        /* If in a call, hangup */
                        if (pvt->owner)
                        {
                            ast_queue_control(pvt->owner, AST_CONTROL_HANGUP);
	                    pvt->owner->tech_pvt = NULL;
                        }

                        AST_LIST_REMOVE_CURRENT(&devices, entry);
                        if (pvt->monitor_thread != AST_PTHREADT_NULL) {
							pvt->monitor_enable = 0;
                            pthread_kill(pvt->monitor_thread, SIGURG);
                            pthread_join(pvt->monitor_thread, NULL);
                        }
                        stop_voice(pvt);

						free(pvt->io_buf);
						free(pvt->io_save_buf);
                        free(pvt);
                }
                else
                        pvt->mark = 0;
        }
        AST_LIST_TRAVERSE_SAFE_END
        AST_LIST_UNLOCK(&devices);
}

static int __unload_module(void)
{
        /* Unregister Manager actions */
        ast_manager_unregister("3GNotify");
        ast_manager_unregister("3GResponse");
		ast_manager_unregister("3Gpvtgetstats");
		ast_manager_unregister("3gpvtresetstats");

        /* First, take us out of the channel loop */
        ast_channel_unregister(&modem_tech);

        /* Destroy the device list */
	destroy_device_list(0);
	
        /* Unregister the CLI & APP */
        ast_cli_unregister_multiple(modem_cli, sizeof(modem_cli) / sizeof(modem_cli[0]));
        ast_unregister_application(app_sebistatus);
		ast_unregister_application(app_ussd);

	memset(conf_file_md5, 0, MD5_DIGEST_LEN);

        return 0;

}

int unload_module()
{
	return __unload_module();
}

int load_module(void)
{
        ast_log(LOG_WARNING, "Loading module chan_sebi\n");

        if (!modem_load_config(0)) {
                ast_log(LOG_ERROR, "Unable to read config file %s. Not loading module.\n", MODEM_CONFIG);
                return -1;
        }

        ast_cli_register_multiple(modem_cli, sizeof(modem_cli) / sizeof(modem_cli[0]));
        ast_register_application(app_sebistatus, modem_status_exec, sebistatus_synopsis, sebistatus_desc);
		ast_register_application(app_ussd, modem_ussd_exec, ussd_synopsis, 
			ussd_desc);

        /* Make sure we can register our channel type */
        if (ast_channel_register(&modem_tech)) {
                ast_log(LOG_ERROR, "Unable to register channel class %s\n", "Sebi");
                return -1;
        }

        /* Register for Manager actions */
        ast_manager_register2("3GResponse", EVENT_FLAG_CALL,
                manager_action_3g_response, "Responses from 3G Entity", NULL);
        ast_manager_register2("3GNotify", EVENT_FLAG_CALL,
                manager_action_3g_notify, "Notifications from 3G Entity", NULL);
		ast_manager_register2("3Gpvtgetstats", EVENT_FLAG_SYSTEM,
			manager_modem_get_stats, "Get Packet Statistics", NULL);
		ast_manager_register2("3Gpvtresetstats", EVENT_FLAG_SYSTEM,
			manager_modem_reset_stats, "Reset Packet Statistics", NULL);

        return 0;
}

static int _reload(int check_conf_file)
{
	if (!ast_config_file_md5_update(MODEM_CONFIG, conf_file_md5) && check_conf_file)
	{
		ast_log(LOG_DEBUG, "Skipping reload since %s was not changed\n", MODEM_CONFIG);
		return 0;
	}

        if (!modem_load_config(1)) {
                ast_log(LOG_ERROR, "Unable to read config file %s. Not Re-loading module.\n", MODEM_CONFIG);
                return -1;
        }

        /* Destroy the device list */
        destroy_device_list(1);

	return 0;
}

int reload(void)
{
	return _reload(0);
}

int reload_if_changed(void)
{
	return _reload(1);
}

int usecount()
{
	return 0;
}

char *description()
{
	return (char *)desc;
}

char *key()
{
	return ASTERISK_GPL_KEY;
}
#if 0
AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_DEFAULT, "Sebi Modem Device Channel Driver",
                .load = load_module,
                .unload = unload_module,
);
#endif
