/****************************************************************************
 *
 * rg/pkg/voip/asterisk/channels/chan_jdsp.c
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

/*
 *
 * JDSP abstraction layer interface
 *
 */

#include <stdio.h>
#include <string.h>
#ifdef __NetBSD__
#include <pthread.h>
#include <signal.h>
#else
#include <sys/signal.h>
#endif
#include <errno.h>
#include <stdlib.h>
#if !defined(SOLARIS) && !defined(__FreeBSD__)
#include <stdint.h>
#endif
#include <unistd.h>
#include <math.h>
#include <ctype.h>

#include "asterisk.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision: 1.180.4.3.2.189.2.6 $")


#include "asterisk/lock.h"
#include "asterisk/channel.h"
#include "asterisk/config.h"
#include "asterisk/logger.h"
#include "asterisk/module.h"
#include "asterisk/pbx.h"
#include "asterisk/options.h"
#include "asterisk/file.h"
#include "asterisk/ulaw.h"
#include "asterisk/alaw.h"
#include "asterisk/callerid.h"
#include "asterisk/adsi.h"
#include "asterisk/cli.h"
#include "asterisk/cdr.h"
#include "asterisk/features.h"
#include "asterisk/musiconhold.h"
#include "asterisk/say.h"
#include "asterisk/tdd.h"
#include "asterisk/app.h"
#include "asterisk/astdb.h"
#include "asterisk/manager.h"
#include "asterisk/causes.h"
#include "asterisk/term.h"
#include "asterisk/utils.h"
#include "asterisk/transcap.h"
#include "asterisk/rtp.h"
#if defined(T38_SUPPORT)
#include "asterisk/udptl.h"
#endif
#include "asterisk/jdsp_common.h"
#include "asterisk/incall_announcement.h"

#include <util/openrg_gpl.h>
#include <voip/dsp/phone.h>
#include <kos_chardev_id.h>
#include <rg_ioctl.h>

/* XXX Must be configurable */
#define NATIVE_FORMATS ((AST_FORMAT_MAX_AUDIO << 1) - 1)

#define DEFAULT_FAXMODEM_PTIME 20 /* 20 mls */
#define DEFAULT_VOICE_PTIME 20 /* 20 mls */
#define MAX_TONE_DURATION 20 /* sec */
#define MAX_RING_DURATION 20 /* sec */
#define MAX_RING_RETRY_TIMES 10 
#define RING_RETRY_DURATION 5 /* sec */
#define MAX_CALL_KEY_SEQ 10 /* digits */

static const char desc[] = "Jungo DSP Abstraction Layer";

static const char tdesc[] = "Jungo DSP Abstraction Layer Driver";

static const char type[] = "jdsp";
static const char config[] = "jdsp.conf";

static unsigned char conf_file_md5[MD5_DIGEST_LEN];

/* The time between the fwd_disconnect onhook and offhook is about 1 second.
 * We take 2 seconds timeout in order to be on the safe side. */
#define FWD_DISCONNECT_TIMEOUT 2000

typedef enum {
	UNKNOWN = -1,
	HOOKED_OFF_DIALTONE = 0,
	LINE_RING_BACK = 1,
	LINE_BUSY_TONE = 2,
	ONE_ACTIVE_CALL = 3,
	CALLWAITING = 4,
	CALLWAITING_HANGUP = 5,
	CALLHOLD_ACTIVECALL = 6,
	CALLHOLD_DIALTONE = 7,
	CALLHOLD_RINGBACK = 8,
	CALLHOLD_BUSYTONE = 9,
	CALLHOLD_FBUSYTONE = 10,
	CALLHOLD_3WAY_CALL = 11,
	CONFERENCE = 12,
	LINE_FBUSY_TONE = 13,
	STATE_LAST = LINE_FBUSY_TONE, 
} call_matrix_states_t;

static code2str_t states_msg_str[] = {
	{HOOKED_OFF_DIALTONE, "HOOKED_OFF_DIALTONE"},
	{LINE_RING_BACK, "LINE_RING_BACK"},
	{LINE_BUSY_TONE, "LINE_BUSY_TONE"},
	{ONE_ACTIVE_CALL, "ONE_ACTIVE_CALL"},
	{CALLWAITING, "CALLWAITING"},
	{CALLWAITING_HANGUP, "CALLWAITING_HANGUP"},
	{CALLHOLD_ACTIVECALL, "CALLHOLD_ACTIVECALL"},
	{CALLHOLD_DIALTONE, "CALLHOLD_DIALTONE"},
	{CALLHOLD_RINGBACK, "CALLHOLD_RINGBACK"},
	{CALLHOLD_BUSYTONE, "CALLHOLD_BUSYTONE"},
	{CALLHOLD_FBUSYTONE, "CALLHOLD_FBUSYTONE"},
	{CALLHOLD_3WAY_CALL, "CALLHOLD_3WAY_CALL"},
	{CONFERENCE, "CONFERENCE"},
	{LINE_FBUSY_TONE, "LINE_FBUSY_TONE"},
	{-1, "UNKNOWN"}
};

typedef enum {
	IGNORE = 0,
	RESTART_DIALTONE = 1,
	DROP_AND_RESTART_DIALTONE = 2,
	SWITCH_CALL = 3,
	SWITCH_3WAY_CALL = 4,
	SWITCH_HG_CALLWAITNG_CALL = 5,
	DROP_AND_SWITCH = 6,
	STOP_TONE_AND_SWITCH = 7,
	PUT_ON_HOLD = 8,
	REJECT_INCOMING_CALL = 9,
	TERMINATE_HELD_CALL = 10,
	TERMINATE_3WAYHELD_CALL = 11,
	TERMINATE_ACTIVE_SWITCH_TO_HELD = 12,
	START_CONFERENCE = 13,
	DROP_LAST_PARTY_FROM_CONF = 14,
	ENQUEUE_FLASH_EVENT = 15,
	TRANSFER_CALL = 16,
	HOLD_LAST_PARTY_FROM_CONF = 17, /* switch to real, put 3way on hold */
	HOLD_FIRST_PARTY_FROM_CONF = 18, /* switch to 3way, put real on hold */
	ACTION_LAST = HOLD_FIRST_PARTY_FROM_CONF,
} call_matrix_actions_t;

static code2str_t actions_msg_str[] = {
	{IGNORE, "IGNORE"},
	{RESTART_DIALTONE, "RESTART_DIALTONE"},
	{DROP_AND_RESTART_DIALTONE, "DROP_AND_RESTART_DIALTONE"},
	{SWITCH_CALL, "SWITCH_CALL"},
	{SWITCH_3WAY_CALL, "SWITCH_3WAY_CALL"},
	{SWITCH_HG_CALLWAITNG_CALL, "SWITCH_HG_CALLWAITNG_CALL"},
	{DROP_AND_SWITCH, "DROP_AND_SWITCH"},
	{STOP_TONE_AND_SWITCH, "STOP_TONE_AND_SWITCH"},
	{PUT_ON_HOLD , "PUT_ON_HOLD"},
	{REJECT_INCOMING_CALL, "REJECT_INCOMING_CALL"},
	{TERMINATE_HELD_CALL, "TERMINATE_HELD_CALL"},
	{TERMINATE_3WAYHELD_CALL, "TERMINATE_3WAYHELD_CALL"},
	{TERMINATE_ACTIVE_SWITCH_TO_HELD, "TERMINATE_ACTIVE_SWITCH_TO_HELD"},
	{START_CONFERENCE, "START_CONFERENCE"},
	{DROP_LAST_PARTY_FROM_CONF, "DROP_LAST_PARTY_FROM_CONF"},
	{ENQUEUE_FLASH_EVENT, "ENQUEUE_FLASH_EVENT"},
	{TRANSFER_CALL, "TRANSFER_CALL"},
	{HOLD_LAST_PARTY_FROM_CONF, "HOLD_LAST_PARTY_FROM_CONF"},
	{HOLD_FIRST_PARTY_FROM_CONF, "HOLD_FIRST_PARTY_FROM_CONF"},
	{-1, "UNKNOWN"}
};

typedef enum {
    FWD_DISCONNECT_OFF = 0,
    FWD_DISCONNECT_ON = 1,
    FWD_DISCONNECT_IN_PROGRESS = 2,
} fwd_disconnect_t;

typedef enum {
    MWI_OFF = 0,
    MWI_EXTERNAL_PER_LINE = 1, /* message notifications received from chan_sip,
	                            * and are coupled with a specific line (ATA) */
    MWI_EXTERNAL_GLOBAL = 2, /* message notifications received from chan_sip,
	                          * and are indicated on all lines (PBX) */
    MWI_INTERNAL = 3, /* indicate messages from asterisk internal voicemail */
} mwi_type_t;

#define SIG_FXOLS	1 /* JDSP_SIG_FXOLS */
#define SIG_FXOGS	2 /* JDSP_SIG_FXOGS */

typedef enum {
    TRANSFER_OFF = 0,
    TRANSFER_SIGNALLING = 1,
    TRANSFER_BRIDGING = 2,
} transfermode_t;

typedef enum {
    FLASH_KEY_NONE = -1,		/* empty sequence */
    FLASH_KEY = 0,			/* flash key */
    FLASH_KEY_PLUS_0 = 1,		/* flash key followed by 0 */
    FLASH_KEY_PLUS_1 = 2,		/* flash key followed by 1 */
    FLASH_KEY_PLUS_2 = 3,		/* flash key followed by 2 */
    FLASH_KEY_PLUS_3 = 4,		/* flash key followed by 3 */
    FLASH_KEY_PLUS_4 = 5,		/* flash key followed by 4 */
    FLASH_KEY_PLUS_5 = 6,		/* flash key followed by 5 */
    FLASH_KEY_PLUS_6 = 7,		/* flash key followed by 6 */
    FLASH_KEY_PLUS_7 = 8,		/* flash key followed by 7 */
    FLASH_KEY_PLUS_8 = 9,		/* flash key followed by 8 */
    FLASH_KEY_PLUS_9 = 10,		/* flash key followed by 9 */
    FLASH_KEY_PLUS_A = 11,		/* flash key followed by A */
    FLASH_KEY_PLUS_B = 12,		/* flash key followed by B */
    FLASH_KEY_PLUS_C = 13,		/* flash key followed by C */
    FLASH_KEY_PLUS_D = 14,		/* flash key followed by D */
    FLASH_KEY_PLUS_ASTERISK = 15,	/* flash key followed by '*' */
    FLASH_KEY_PLUS_POUND = 16,	/* flash key followed by '#' */
    FLASH_KEY_LATE = 17,  	    /* flash key after timeout */
    LAST_ACTION_KEY = FLASH_KEY_PLUS_4
} flash_key_seq_t;

struct jdsp_ring_data_t {
	int channel;
	int duration;
	int retries;
};

static code2str_t flash_msg_str[] = {
	{FLASH_KEY, "FLASH"},				/* flash key */
	{FLASH_KEY_PLUS_0, "FLASH+0"},		/* flash key followed by 0 */
	{FLASH_KEY_PLUS_1, "FLASH+1"},		/* flash key followed by 1 */
	{FLASH_KEY_PLUS_2, "FLASH+2"},		/* flash key followed by 2 */
	{FLASH_KEY_PLUS_3, "FLASH+3"},		/* flash key followed by 3 */
	{FLASH_KEY_PLUS_4, "FLASH+4"},		/* flash key followed by 4 */
	{FLASH_KEY_PLUS_5, "FLASH+5"},		/* flash key followed by 5 */
	{FLASH_KEY_PLUS_6, "FLASH+6"},		/* flash key followed by 6 */
	{FLASH_KEY_PLUS_7, "FLASH+7"},		/* flash key followed by 7 */
	{FLASH_KEY_PLUS_8, "FLASH+8"},		/* flash key followed by 8 */
	{FLASH_KEY_PLUS_9, "FLASH+9"},		/* flash key followed by 9 */
	{FLASH_KEY_PLUS_A, "FLASH+A"},		/* flash key followed by A */
	{FLASH_KEY_PLUS_B, "FLASH+B"},		/* flash key followed by B */
	{FLASH_KEY_PLUS_C, "FLASH+C"},		/* flash key followed by C */
	{FLASH_KEY_PLUS_D, "FLASH+D"},		/* flash key followed by D */
	{FLASH_KEY_PLUS_ASTERISK, "FLASH+*"},	/* flash key followed by '*' */
	{FLASH_KEY_PLUS_POUND, "FLASH+#"},	/* flash key followed by '#' */
	{FLASH_KEY_LATE, "FLASH+timeout"},	/* flash key after timeout */
	{-1, "NO_FLASH_SEQ"}
};

#define FLASH_KEY_TIMEOUT		3500	/* duration between flash and 
						   following digit in millisec*/
typedef call_matrix_actions_t call_matrix_array_t[STATE_LAST+1][LAST_ACTION_KEY+1];

static struct ast_codec_pref global_native_formats;
static int global_preferred_codec = 0;
static int heavyweight_codecs = 0;

static char context[AST_MAX_CONTEXT] = "default";
static char cid_num[256] = "";
static char cid_name[256] = "";

static char language[MAX_LANGUAGE] = "";
static char musicclass[MAX_MUSICCLASS] = "";
static char progzone[10]= "";

static int transfertobusy = 1;

static int use_callerid = 1;
static int cid_signalling = CID_SIG_BELL;
static int jdsptrcallerid = 0;
static int cur_signalling = -1;

static ast_group_t cur_group = 0;
static ast_group_t cur_callergroup = 0;
static ast_group_t cur_pickupgroup = 0;

static int use_caller_variable = 0;

static int enabled = 1;

static int immediate = 0;

static int automatic_call = 0;

static int internalmoh = 1;

static int callwaiting = 0;

static int callwaitingcallerid = 1;

static int hidecallerid = 0;

static int callreturn = 0;

static int threewaycalling = 0;

static int threewayconference = 0;

static int conference_hangup_drop_all = 0;

static int return_to_held_call = 1;

static int use_hold_tone = 1;

static int dead_line_tone_support = 0;

static phone_tone_t dialtone = PHONE_TONE_DIAL;

static mwi_type_t mwi = MWI_OFF;

static mwi_type_t vmwi = MWI_OFF;

static fwd_disconnect_t fwd_disconnect = FWD_DISCONNECT_OFF;

static transfermode_t transfermode = TRANSFER_OFF;

static int canpark = 0;

static float rxgain = 0.0;

static float txgain = 0.0;

static int tonezone = -1;

static int callprogress = 0;

static int internal_transfer_only = 0;

static char accountcode[AST_MAX_ACCOUNT_CODE] = "";

static char mailbox[AST_MAX_EXTENSION];

static char cfwd_unconditional_activate_code[AST_MAX_EXTENSION];

static char cfwd_unconditional_deactivate_code[AST_MAX_EXTENSION];

static char cfwd_busy_activate_code[AST_MAX_EXTENSION];

static char cfwd_busy_deactivate_code[AST_MAX_EXTENSION];

static char cfwd_no_answer_activate_code[AST_MAX_EXTENSION];

static char cfwd_no_answer_deactivate_code[AST_MAX_EXTENSION];

static char dnd_activate_code[AST_MAX_EXTENSION];

static char dnd_deactivate_code[AST_MAX_EXTENSION];

static int adsi = 0;

static faxmethod_t faxtxmethod = FAX_NONE;

static fax_detection_method_t detectionmethod = FAX_DETECTION_TERMINATING;

static int discriminate_call = 0;

/*! \brief Wait up to 15 seconds for first digit (FXO logic) */
static int firstdigittimeout = 15000;

static int autocall_timeout = 15000;

/*! \brief Wait 40 seconds after start playing reorder tone, and before playing
 * off-hook warning tone (FXO logic) */
static int offhookwarningtimeout = 40000;

/*! \brief Play the fast busy tone for 40 sec */
static int fastbusywarningtimeout = 40000;

/*! \brief How long to wait for following digits (FXO logic) */
/* Equivalent to the idt (inter digit timer) in RFC 2897 */
/* Similar to InterDigitTimerStd in TR-104 */
static int gendigittimeout = 8000;

/*! \brief How long to wait for an extra digit, if there is an ambiguous match */
/* Also quivalent to the idt (inter digit timer) in RFC 2897 */
/* Similar to InterDigitTimerOpen in TR-104 */
static int matchdigittimeout = 3000;

/* Delay (miliseconds) after receiving ANS tone (which also can be identified as)
 * modem tone */
static int faxmodem_local_detect_timeout = 0;
static int faxmodem_net_detect_timeout = 0;

static char diag_ivr_activation_seq[MAX_CALL_KEY_SEQ] = { };

static int deflaw = AST_FORMAT_ULAW;

static int usecnt = 0;
static int is_non3g_second_callsallowed = 0;
AST_MUTEX_DEFINE_STATIC(usecnt_lock);

/*! \brief Protect the interface list (of jdsp_pvt's) */
AST_MUTEX_DEFINE_STATIC(iflock);

static int ifcount = 0;

static exten_num_list_t *exten_num_lst = NULL;

static out_call_prefix_list_t *out_call_prefix_lst = NULL;

/*! \brief Protect the monitoring thread, so only one process can kill or start it, and not
   when it's doing something critical. */
AST_MUTEX_DEFINE_STATIC(monlock);

/*! \brief This is the thread for the monitor which checks for input on the channels
   which are not currently in use.  */
static pthread_t monitor_thread = AST_PTHREADT_NULL;

static struct sched_context *sched;

static int stop_immediatly_pound = 0;

static int silence_suppression_enabled = 1;

static int restart_monitor(void);

static inline int jdsp_get_event(int fd, phone_event_t *event)
{
	if (read(fd, event, sizeof(*event)) < 0)
		return -1;
	return 0;
}

#define MIN_MS_SINCE_FLASH			( (2000) )	/*!< 2000 ms */

#define GET_FIRSTDIGIT_TIMEOUT(p) (((p)->automatic_call && \
	!(p)->subs[SUB_THREEWAY].owner) ? (p)->autocall_timeout : \
	(p)->firstdigittimeout)

struct jdsp_pvt;

#define SUB_REAL	0			/*!< Active call */
#define SUB_CALLWAIT	1			/*!< Call-Waiting call on hold */
#define SUB_THREEWAY	2			/*!< Three-way call */

static char *subnames[] = {
	"Real",
	"Callwait",
	"Threeway"
};

struct jdsp_subchannel {
	int dfd; /*!< File descriptor for direct access for DSP (not SLIC) */
	struct ast_channel *owner;
	int chan; 				/*!< Associated channel in DSP (0/1) */
	char buffer[AST_FRIENDLY_OFFSET + READ_SIZE];
	struct ast_frame f;		/*!< One frame for each channel.  How did this ever work before? */
	struct ast_rtp *rtp;
    struct ast_udptl *udptl;
	unsigned short seqno;
	unsigned long lastts;
	struct timeval txcore;

	unsigned int inthreeway:1;
};

#define CONF_USER_REAL		(1 << 0)
#define CONF_USER_THIRDCALL	(1 << 1)

#define MAX_SLAVES	4

static struct jdsp_pvt {
	ast_mutex_t lock;
	struct ast_channel *owner;			/*!< Our current active owner (if applicable) */
							/*!< Up to three channels can be associated with this call */
		
	struct jdsp_subchannel sub_unused;		/*!< Just a safety precaution */
	struct jdsp_subchannel subs[3];			/*!< Sub-channels */

	int jfd;
	struct jdsp_pvt *slaves[MAX_SLAVES];		/*!< Slave to us (follows our conferencing) */
	struct jdsp_pvt *master;			/*!< Master to us (we follow their conferencing) */
	int inconference;				/*!< If our real should be in the conference */
	
	int sig;					/*!< Signalling style */
	float rxgain;
	float txgain;
	int tonezone;					/*!< tone zone for this chan, or -1 for default */
	struct jdsp_pvt *next;				/*!< Next channel in list */
	struct jdsp_pvt *prev;				/*!< Prev channel in list */

	/* flags */
	unsigned int adsi:1;
	unsigned int callreturn:1;
	unsigned int callwaiting:1;
	unsigned int callwaitingcallerid:1;
	unsigned int canpark:1;
	unsigned int confirmanswer:1;			/*!< Wait for '#' to confirm answer */
	unsigned int hidecallerid;
	unsigned int immediate:1;			/*!< Answer before getting digits? */
	unsigned int automatic_call:1;	
	unsigned int follow_call:1;			/*!< Whether the current call was placed right after a disconnected call (without placing the phone on-hook) */
        unsigned int need_follow_call:1;
	unsigned int network_failure_disconnection:1;	/*!< Whether the current call was disconnected as a result of network failure */
	unsigned int outgoing:1;
	unsigned int permcallwaiting:1;
	unsigned int permhidecallerid:1;		/*!< Whether to hide our outgoing caller ID or not */
	unsigned int threewaycalling:1;
	unsigned int threewayconference:1;
	unsigned int use_callerid:1;			/*!< Whether or not to use caller id on this channel */
	unsigned int jdsptrcallerid:1;			/*!< should we use the callerid from incoming call on jdsp transfer or not */
	unsigned int transfertobusy:1;			/*!< allow flash-transfers to busy channels */
	unsigned int faxdetected:1;             /*!< Slow speed fax was detected */
	unsigned int modemdetected:1;
	unsigned int enabled:1;
	unsigned int next_msg_wait:1;
	unsigned int msg_wait:1;
	unsigned int t38enabled:1;
	unsigned int use_caller_variable:1;			/*!< Whether or not to set the caller */
	unsigned int use_hold_tone:1;
	unsigned int internal_transfer_only:1; 
	mwi_type_t mwi;
	mwi_type_t vmwi;
	fwd_disconnect_t fwd_disconnect;
	phone_tone_t dialtone;
	char context[AST_MAX_CONTEXT];
	char defcontext[AST_MAX_CONTEXT];
	char exten[AST_MAX_EXTENSION];
	char language[MAX_LANGUAGE];
	char musicclass[MAX_MUSICCLASS];
	char cid_num[AST_MAX_EXTENSION];
	int cid_ton;					/*!< Type Of Number (TON) */
	char cid_name[AST_MAX_EXTENSION];
	char lastcid_num[AST_MAX_EXTENSION];
	char lastcid_name[AST_MAX_EXTENSION];
	char *origcid_num;				/*!< malloced original callerid */
	char *origcid_name;				/*!< malloced original callerid */
	char callwait_num[AST_MAX_EXTENSION];
	char callwait_name[AST_MAX_EXTENSION];
	char rdnis[AST_MAX_EXTENSION];
	char dnid[AST_MAX_EXTENSION];
	unsigned int group;
	ast_group_t callgroup;
	ast_group_t pickupgroup;
	int channel;					/*!< Channel Number or CRV */
	int dsp_line;					/*!< Hardware DSP line number */
	time_t guardtime;				/*!< Must wait this much time before using for new call */
	int cid_signalling;				/*!< CID signalling type bell202 or v23 */
	int callingpres;				/*!< The value of callling presentation that we're going to use when placing a PRI call */
	int callwaitingalert;				/*!< Flag, means that now played call waiting alert */
	int callwaitrings;
	int callprogress;
	struct timeval flashtime;			/*!< Last flash-hook time */
	char accountcode[AST_MAX_ACCOUNT_CODE];		/*!< Account code */
	char mailbox[AST_MAX_EXTENSION];
	char cfwd_unconditional_activate_code[AST_MAX_EXTENSION];
	char cfwd_unconditional_deactivate_code[AST_MAX_EXTENSION];
	char cfwd_busy_activate_code[AST_MAX_EXTENSION];
	char cfwd_busy_deactivate_code[AST_MAX_EXTENSION];
	char cfwd_no_answer_activate_code[AST_MAX_EXTENSION];
	char cfwd_no_answer_deactivate_code[AST_MAX_EXTENSION];
	char dnd_activate_code[AST_MAX_EXTENSION];
	char dnd_deactivate_code[AST_MAX_EXTENSION];
	char dialdest[256];
	int onhooktime;
	int distinctivering;				/*!< Which distinctivering to use */
	int cidrings;					/*!< Which ring to deliver CID on */
	int dtmfrelax;					/*!< whether to run in relaxed DTMF mode */
#if 0
	int fake_event;
#endif
	int polarityonanswerdelay;
	struct timeval polaritydelaytv;
	int sendcalleridafter;
	int polarity;
	int tone_timeout_id;
	int modem_timer_sched_id;
	int play_tone;
	int matchdigittimeout;
	int firstdigittimeout;
	int autocall_timeout;
	int gendigittimeout;
	int faxtxmethod;
	int detectionmethod;
	transfermode_t transfermode;
	int discriminate_call;                  /*!< Discriminate call flag */
	int alloc_count;
	int real_dc_num;
	int pending_flash_key;
	struct ast_channel *pending_flash_key_channel;
	char callback_extension[AST_MAX_EXTENSION];
	int io_pipe[2];
	voip_dsp_stats_t stats;
	int silence_suppression_enabled;
 	call_matrix_array_t call_matrix; /*!< Per channel call matrix */ 
	char call_key_seq[MAX_CALL_KEY_SEQ];
	struct timeval call_key_seq_ts;
} *iflist = NULL, *ifend = NULL;

static struct ast_channel *jdsp_request(const char *type, const struct ast_codec_pref *formats, void *data, int *cause);
static int jdsp_digit_begin(struct ast_channel *ast, char digit);
static int jdsp_digit_end(struct ast_channel *ast, char digit);
static int jdsp_call(struct ast_channel *ast, char *rdest, int timeout);
static int jdsp_hangup(struct ast_channel *ast);
static int jdsp_answer(struct ast_channel *ast);
struct ast_frame *jdsp_read(struct ast_channel *ast);
static int jdsp_write(struct ast_channel *ast, struct ast_frame *frame);
static int jdsp_indicate(struct ast_channel *chan, int condition, const void *data, size_t datalen);
static int jdsp_fixup(struct ast_channel *oldchan, struct ast_channel *newchan);
static void jdsp_pre_bridge(struct ast_channel *chan);
static void start_call(struct jdsp_pvt *i);
static void alloc_and_start_call(struct jdsp_pvt *i);


void configure_call_matrix_es(struct jdsp_pvt *pvt);
void configure_call_matrix_it(struct jdsp_pvt *pvt);
void configure_call_matrix_de(struct jdsp_pvt *pvt);
void configure_call_matrix_nz(struct jdsp_pvt *pvt);

static void (*configure_call_matrix)(struct jdsp_pvt *pvt) = 
#ifdef CONFIG_RG_VODAFONE_IT
	configure_call_matrix_it;	
#elif CONFIG_RG_VODAFONE_ES
	configure_call_matrix_es;
#elif CONFIG_RG_VODAFONE_PT
	configure_call_matrix_es;
#elif CONFIG_RG_VODAFONE_DE
	configure_call_matrix_de;
#elif CONFIG_RG_VODAFONE_NZ
	configure_call_matrix_nz;
#else	
#error call_matrix function must be defined
#endif

static const struct ast_channel_tech jdsp_tech = {
	.type = type,
	.description = tdesc,
	.capabilities = NATIVE_FORMATS,
	.requester = jdsp_request,
	.send_digit_begin = jdsp_digit_begin,
	.send_digit_end = jdsp_digit_end,
	.call = jdsp_call,
	.hangup = jdsp_hangup,
	.answer = jdsp_answer,
	.read = jdsp_read,
	.write = jdsp_write,
	.indicate = jdsp_indicate,
	.fixup = jdsp_fixup,
	.pre_bridge = jdsp_pre_bridge,
};

struct jdsp_pvt *round_robin[32];

#define ISTRUNK(p) ((p->sig == SIG_FXSLS) || (p->sig == SIG_FXSGS))

#define CANBUSYDETECT(p) ISTRUNK(p)
#define CANPROGRESSDETECT(p) ISTRUNK(p)

#define OTHER_DC_NUM(num) (((num) == 0) ? 1: 0)

static void jdsp_stop_audio(struct ast_channel *ast, int index);

static int jdsp_get_index(struct ast_channel *ast, struct jdsp_pvt *p, int nullok)
{
	int res;
	if (p->subs[0].owner == ast)
		res = 0;
	else if (p->subs[1].owner == ast)
		res = 1;
	else if (p->subs[2].owner == ast)
		res = 2;
	else {
		res = -1;
		if (!nullok)
			ast_log(LOG_WARNING, "Unable to get index, and nullok is not asserted\n");
	}
	return res;
}

static int jdsp_dc_shared_alloc(struct jdsp_pvt *p, int chan, char *id)
{
    alloc_params_t params = {{0}};
    int res;

    ast_log(LOG_DEBUG, "Allocating DC on %s; shared alloc\n", p->owner ? p->owner->name : "(nil)");

    strncpy(params.id, id, MAX_ALLOC_ID_LEN);
    params.chan = chan;

    res = ioctl(p->jfd, VOIP_LINE_DC_ALLOC, &params);
    if (res)
		ast_log(LOG_WARNING, "Failed to allocate shared data channel for %s", p->owner ? p->owner->name : "(nil)"  );

    return res;
}

#define jdsp_dc_exclusive_alloc(p, chan, lame) \
	_jdsp_dc_exclusive_alloc(p, chan, lame, __LINE__)

static int _jdsp_dc_exclusive_alloc(struct jdsp_pvt *p, int chan, int lame, 
	int line)
{
    alloc_params_t params = {{0}};
    int res;

    ast_log(LOG_DEBUG, "(%d) Allocating DC on %s; exclusive alloc\n", line, 
		p->owner ? p->owner->name : "(nil)");

    params.chan = chan;
    params.flags = lame ? DC_ALLOC_FLAG_LAME : 0;

	if ((res = ioctl(p->jfd, VOIP_LINE_DC_ALLOC, &params)))
	{
		ast_log(LOG_WARNING, "(%d) Failed to allocate exclusive %s data channel"
			" for %s", line, lame ? "lame" : "", 
			p->owner ? p->owner->name : "(nil)");
	}
	else
	{
		p->alloc_count++;
		ast_log(LOG_DEBUG, "(%d) Alloc count for %s incremented to %d\n", line,
			p->context, p->alloc_count);
	}

    return res;
}


/* Free data channel */
#define jdsp_dc_free(p, chan) _jdsp_dc_free(p, chan, __LINE__)
static int _jdsp_dc_free(struct jdsp_pvt *p, int chan, int line)
{
	int ret;
	

	if (chan > 1 || chan < 0)
	{
		ast_log(LOG_ERROR, "(%d) Freeing DC on %s with invalid channel %d\n",
			line, p->context, chan);
		return -1;
	}

	ret = ioctl(p->jfd, VOIP_LINE_DC_FREE, chan);

    ast_log(LOG_DEBUG, "(%d) Freeing DC on %s, chan %d\n", line, p->context, 
		chan);

	if (!p->alloc_count)
	{
		ast_log(LOG_DEBUG, "(%d) alloc_count already zero for %s\n",
			line, p->context);
		return ret;
	}

	p->alloc_count--;
	ast_log(LOG_DEBUG, "(%d) Alloc count for %s decremented to %d\n", line,
		p->context, p->alloc_count);
	if (!p->alloc_count)
		p->real_dc_num = -1;

	return ret;
}

static void jdsp_add_stats_from_rtp(voip_dsp_stats_t *stats, 
		struct jdsp_pvt *p, int channel)
{
	voip_dsp_stats_args_t args = {};

	args.channel = channel;

	ioctl(p->jfd, VOIP_DSP_STATS_GET, &args);

	stats->rx_packets += args.stats.rx_packets;
	stats->tx_packets += args.stats.tx_packets;
	stats->rx_octets += args.stats.rx_octets;
	stats->tx_octets += args.stats.tx_octets;
	stats->rx_packets_lost += args.stats.rx_packets_lost;
	stats->jb_overruns += args.stats.jb_overruns;
	stats->jb_underruns += args.stats.jb_underruns;
}

static int manager_jdsp_get_stats(struct mansession *s, struct message *m)
{
	voip_dsp_stats_t stats = {};
    int channel = atoi(astman_get_header(m, "Channel"));
    struct jdsp_pvt *p;
    int i;
    char msg[256];

	ast_log(LOG_DEBUG,"channel = %d\n", channel);	

    for(p = iflist; p; p = p->next)
    {
		if (p->channel == channel)
		{
			stats = p->stats;
			break;
		}
    }

    if(!p)
    {
    	astman_send_error(s, m, "Channel not found");
		return 0;
    }
	
    for(i = 0; i < 3; ++i)
    {
    	if (p->subs[i].chan != -1)
			jdsp_add_stats_from_rtp(&stats, p, p->subs[i].chan); 
    }

	snprintf(msg, sizeof(msg), "\r\n"
		"RxPackets: %d\r\n"
		"TxPackets: %d\r\n"
		"RxOctets: %d\r\n"
		"TxOctets: %d\r\n"
		"RxPacketsLost: %d\r\n"
		"Overruns: %d\r\n"
		"Underruns: %d\r\n",
		stats.rx_packets, stats.tx_packets, stats.rx_octets,
		stats.tx_octets, stats.rx_packets_lost, 
		stats.jb_overruns, stats.jb_underruns);

	astman_send_ack(s, m, msg);
	return 0;
}

static int _jdsp_get_attach_state(int channel)
{
	struct jdsp_pvt *p = NULL;
	
	for (p = iflist; p && p->channel != channel; p = p->next);
	if (!p)
		return -1;
	
	ioctl(p->jfd, VOIP_DSP_PHONE_DETECT, NULL);
	return 0;
}

static int jdsp_get_attach_state_cmd(int fd, int argc, char **argv)
{
	int channel;

	if (argc != 3)
		return RESULT_SHOWUSAGE;

	channel = atoi(argv[2]);

	/* Async call to DSP for getting attach state */
	if (_jdsp_get_attach_state(channel))
	{
		ast_cli(fd, "Channel not found\n");
		return RESULT_FAILURE;		
	}
	ast_cli(fd, "Attachement results will follow in log\n");
	return RESULT_SUCCESS;
}

static int manager_jdsp_get_attach_state(struct mansession *s, struct message *m)
{
	int channel = atoi(astman_get_header(m, "Channel"));

	/* Async call to DSP for getting attach state */
	if (_jdsp_get_attach_state(channel))
		astman_send_error(s, m, "Channel not found");
	
	return 0;
}

static int manager_jdsp_reset_stats(struct mansession *s, struct message *m)
{
    int channel = atoi(astman_get_header(m, "Channel"));
    struct jdsp_pvt *p;

	ast_log(LOG_DEBUG,"Reseting stats for trunk = %d\n", channel);

    for(p = iflist; p && p->channel != channel; p = p->next);

    if (!p)
    {
    	astman_send_error(s, m, "Channel not found");
		return 0;
    }

    memset(&p->stats, 0 , sizeof(struct voip_dsp_stats_t));
	
    astman_send_ack(s, m, "Statistics deleted");

	return 0;
}

static void wakeup_sub(struct jdsp_pvt *p, int a, void *pri)
{
	struct ast_frame null = { AST_FRAME_NULL, };
	for (;;) {
		if (p->subs[a].owner) {
			if (ast_mutex_trylock(&p->subs[a].owner->lock)) {
				ast_mutex_unlock(&p->lock);
				usleep(1);
				ast_mutex_lock(&p->lock);
			} else {
				ast_queue_frame(p->subs[a].owner, &null);
				ast_mutex_unlock(&p->subs[a].owner->lock);
				break;
			}
		} else
			break;
	}
}

static void jdsp_queue_frame(struct jdsp_pvt *p, struct ast_frame *f)
{
	/* We must unlock the PRI to avoid the possibility of a deadlock */
	for (;;) {
		if (p->owner) {
			if (ast_mutex_trylock(&p->owner->lock)) {
				ast_mutex_unlock(&p->lock);
				usleep(1);
				ast_mutex_lock(&p->lock);
			} else {
				ast_queue_frame(p->owner, f);
				ast_mutex_unlock(&p->owner->lock);
				break;
			}
		} else
			break;
	}
}

static void jdsp_self_enqueue_event(struct jdsp_pvt *p, phone_key_t key, int pressed)
{
	phone_event_t ev;

	ev.channel = 0;
	ev.key = key;
	ev.pressed = pressed;

	ast_log(LOG_DEBUG, "Enqueue %d '%c' to channel jdsp/%d\n", key,
		jdsp_key2char(key), p->channel);
	write(p->io_pipe[1], &ev, sizeof(ev));
}

static void jdsp_queue_control(struct ast_channel *ast, int control)
{
	struct ast_frame f = { AST_FRAME_CONTROL, };

	if (!ast) {
		ast_log(LOG_WARNING, "Asked to queue control %d on a dead channel!\n", control);
		return;
	}

	f.subclass = control;
	return jdsp_queue_frame(ast->tech_pvt, &f);
}

static void swap_subs(struct jdsp_pvt *p, int a, int b)
{
	int tchan;
	int tinthreeway;
	struct ast_channel *towner;
	int i;
	voip_dsp_bind_arg_t bind_arg;
	struct ast_rtp *trtp;

	ast_log(LOG_DEBUG, "Swapping %d and %d\n", a, b);

	tchan = p->subs[a].chan;
	towner = p->subs[a].owner;
	tinthreeway = p->subs[a].inthreeway;
	trtp = p->subs[a].rtp;

	p->subs[a].chan = p->subs[b].chan;
	p->subs[a].owner = p->subs[b].owner;
	p->subs[a].inthreeway = p->subs[b].inthreeway;
	p->subs[a].rtp = p->subs[b].rtp;

	p->subs[b].chan = tchan;
	p->subs[b].owner = towner;
	p->subs[b].inthreeway = tinthreeway;
	p->subs[b].rtp = trtp;

	if (p->subs[a].chan != -1 && p->subs[a].dfd != -1 )
	{
		bind_arg.line = p->dsp_line - 1;
		bind_arg.channel = p->subs[a].chan;
		ioctl(p->subs[a].dfd, VOIP_DSP_BIND, &bind_arg);
	}

	if (p->subs[b].chan != -1 && p->subs[b].dfd != -1 )
	{
		bind_arg.line = p->dsp_line - 1;
		bind_arg.channel = p->subs[b].chan;
		ioctl(p->subs[b].dfd, VOIP_DSP_BIND, &bind_arg);
	}

	if (p->subs[a].owner) 
	{
		i = 0;
		if (a == SUB_REAL)
			p->subs[a].owner->fds[i++] = p->jfd;
		p->subs[a].owner->fds[i++] = p->subs[a].dfd;
		if (a == SUB_REAL)
			p->subs[a].owner->fds[i++] = p->io_pipe[0];
		while (i < AST_MAX_FDS - 2) /* not override timingfd and alertpipe */
			p->subs[a].owner->fds[i++] = -1;
	}
	if (p->subs[b].owner) 
	{
		i = 0;
		if (b == SUB_REAL)
			p->subs[b].owner->fds[i++] = p->jfd;
		p->subs[b].owner->fds[i++] = p->subs[b].dfd;
		if (b == SUB_REAL)
			p->subs[b].owner->fds[i++] = p->io_pipe[0];
		while (i < AST_MAX_FDS - 2)
			p->subs[b].owner->fds[i++] = -1;
	}

	wakeup_sub(p, a, NULL);
	wakeup_sub(p, b, NULL);
}

static int jdsp_open_slic(int line)
{
	int fd;

	line--; /* Asterisk uses one based numbering for slics */
	fd = gpl_sys_rg_chrdev_open(KOS_CDT_VOIP_SLIC, O_RDWR | O_NONBLOCK);
	if (fd == -1)
	{
		ast_log(LOG_WARNING, "Unable to open jdsp kos char device\n");
		return -1;
	}
	
	if (ioctl(fd, VOIP_LINE_BIND, line) < 0)
	{
		ast_log(LOG_WARNING, "Invalid line number '%d'\n", line);
		close(fd);
		return -1;
	}

	return fd;
}

static int jdsp_open_dsp(void)
{
	int fd;

	fd = gpl_sys_rg_chrdev_open(KOS_CDT_VOIP_DSP, O_RDWR | O_NONBLOCK);
	if (fd == -1)
	{
		ast_log(LOG_WARNING, "Unable to open jdsp kos char device\n");
		return -1;
	}

	return fd;
}

static void jdsp_close(int fd)
{
	if(fd > 0)
		close(fd);
}


static int unalloc_sub(struct jdsp_pvt *p, int x)
{
	if (!x) {
		ast_log(LOG_WARNING, "Trying to unalloc the real channel %d?!?\n", p->channel);
		return -1;
	}
	ast_log(LOG_DEBUG, "Released sub %d of channel %d\n", x, p->channel);
	/* This is our last chance to stop audio on this subchannel, just before
	 * we zero 'owner'.
	 * For example, this is the place where audio is stopped in the scenario
	 * where we hang up a call while there is a call waiting.
	 * XXX Should we stop it in swap_subs()? */
	if (p->subs[x].owner && p->owner == p->subs[x].owner)
	    jdsp_stop_audio(p->subs[x].owner, x);
	p->subs[x].owner = NULL;
	p->subs[x].inthreeway = 0;
	return 0;
}

static int jdsp_digit_begin(struct ast_channel *ast, char digit)
{
	struct jdsp_pvt *p;
	int res = 0;
	int index;
	int tone;

	switch (digit)
	{
	case '#':
		tone = PHONE_TONE_DTMF_POUND;
		break;

	case '*':
		tone = PHONE_TONE_DTMF_ASTERISK;
		break;

	case 'A':
	case 'B':
	case 'C':
	case 'D':
		tone = PHONE_TONE_DTMFA + digit - 'A';
		break;

	default:
		/* numbers */
		tone = PHONE_TONE_DTMF0 + digit - '0';
		break;
	}

	p = ast->tech_pvt;
	ast_mutex_lock(&p->lock);
	index = jdsp_get_index(ast, p, 0);
	if ((index == SUB_REAL) && p->owner) {
		tone_param_t param;

		param.direction = TONE_DIRECTION_LOCAL;
		param.tone = tone;
		if ((res = ioctl(p->jfd, VOIP_LINE_TONE, &param)))
			ast_log(LOG_WARNING, "Couldn't dial digit %c\n", digit);
	}
	ast_mutex_unlock(&p->lock);
	restart_monitor();
	return res;
}

static int jdsp_digit_end(struct ast_channel *ast, char digit)
{
	struct jdsp_pvt *p = ast->tech_pvt;
	tone_param_t param;

	if (!p)
	    return 0;

	param.direction = TONE_DIRECTION_LOCAL;
	param.tone = PHONE_TONE_NONE;

	ast_mutex_lock(&p->lock);
	ioctl(p->jfd, VOIP_LINE_TONE, &param);
	ast_mutex_unlock(&p->lock);
	return 0;
}

static char *jdsp_sig2str(int sig)
{
	static char buf[256];
	switch(sig) {
	case SIG_FXOLS:
		return "FXO Loopstart";
	case SIG_FXOGS:
		return "FXO Groundstart";
	default:
		snprintf(buf, sizeof(buf), "Unknown signalling %d", sig);
		return buf;
	}
	return NULL;
}

#define sig2str jdsp_sig2str

static int need_mwi_tone(struct jdsp_pvt *p)
{
	switch (p->mwi)
	{
	case MWI_INTERNAL:
		return ast_app_has_voicemail(p->mailbox, "REMOTE") ||
			ast_app_has_voicemail(p->mailbox, NULL);
	case MWI_EXTERNAL_PER_LINE:
	case MWI_EXTERNAL_GLOBAL:
		return p->next_msg_wait;
	default:
		return 0;
	}
}

static phone_tone_t get_dialtone(struct jdsp_pvt *p)
{
    if (p->dialtone == PHONE_TONE_DIAL && need_mwi_tone(p))
	return PHONE_TONE_MWI;

    return p->dialtone;
}

static int vmwi_changed(struct jdsp_pvt *p)
{
	if (p->vmwi == MWI_INTERNAL) {
		p->next_msg_wait = (ast_app_has_voicemail(p->mailbox, "REMOTE") ||
			ast_app_has_voicemail(p->mailbox, NULL));
	}
	return p->vmwi && (p->next_msg_wait != p->msg_wait);
}

static void prepare_cid(struct jdsp_pvt *pvt, phone_caller_id_t *cid,
    char *name, char *num)
{
	time_t tt = time(NULL);
	struct tm *tm = localtime(&tt);

	memset(cid, 0, sizeof(phone_caller_id_t));

	if (!pvt->use_callerid)
	    return;

	snprintf(cid->time, TIME_STRING_LEN, "%.2d%.2d%.2d%.2d", tm->tm_mon + 1,
	    tm->tm_mday, tm->tm_hour, tm->tm_min);

	if (name && *name)
	    strncpy(cid->name, name, sizeof(cid->name) - 1);
	else
	    cid->name_status = PHONE_CALLER_NAME_ABSENT;

	if (num && *num)
	    strncpy(cid->number, num, sizeof(cid->number) - 1);
	else
	    cid->number_status = PHONE_CALLER_NUMBER_ABSENT;
}

#define DISTINCTIVE_RING_PREFIX "Bellcore-dr"
#define DISTINCTIVE_RING_LENGTH (sizeof(DISTINCTIVE_RING_PREFIX)-1)
#define CCBS_DISTINCTIVE_RING "CCBS distinctive ringing"

static int get_distinctive_ring(struct ast_channel *ast)
{
	char *alert_info;
	int ret = 0;

	if (ast && ast_test_flag(ast, AST_FLAG_CALLBACK))
	    return 1; /* distinctive ring 1 is used for call back ring */

	alert_info = pbx_builtin_getvar_helper(ast, "ALERTINFO");
	if (alert_info)
	{
		if (!strcmp(alert_info, CCBS_DISTINCTIVE_RING))
			ret = 1;
		else if (!strncmp(alert_info, DISTINCTIVE_RING_PREFIX, 
			DISTINCTIVE_RING_LENGTH) && !alert_info[DISTINCTIVE_RING_LENGTH+1])
		{
			ret = alert_info[DISTINCTIVE_RING_LENGTH] - '1';
			if (ret < 0 || ret > 4)
				ret = 0;
		}

	}
	ast_log(LOG_DEBUG, "distinctive_ring string (%s) index (%d)\n", alert_info,
	    ret);
	return ret;
}

static int jdsp_callwait(struct jdsp_pvt *pvt, int start)
{
	call_params_t params = {};

	if (!start)
	{
		if (pvt->callwaitingalert)
		{
			pvt->callwaitingalert = 0;
			return ioctl(pvt->jfd, VOIP_SLIC_CALL_WAITING_ALERT, NULL);
		}
		return 0;
	}

	pvt->callwaitingalert = 1;

	ast_log(LOG_DEBUG, "Call wait %s for %s-%s\n", pvt->callwaitingcallerid ?
		"enabled" : "disabled", pvt->callwait_name, pvt->callwait_num); 

	if (pvt->callwaitingcallerid)
		prepare_cid(pvt, &params.cid, pvt->callwait_name, pvt->callwait_num);

	params.distinctive_ring =
		get_distinctive_ring(pvt->subs[SUB_CALLWAIT].owner);
	return ioctl(pvt->jfd, VOIP_SLIC_CALL_WAITING_ALERT, &params);
}

static void jdsp_ring(struct jdsp_pvt *pvt, int start)
{
    	call_params_t params;

	if (!start)
	{
		ioctl(pvt->jfd, VOIP_SLIC_RING, NULL);
		return;
	}

	if (pvt->owner)
	{
		prepare_cid(pvt, &params.cid, pvt->owner->cid.cid_name,
			pvt->owner->cid.cid_num);
	}
	else
		prepare_cid(pvt, &params.cid, pvt->callwait_name, pvt->callwait_num);
	/* set proper distinct ring for callback */
	params.distinctive_ring = get_distinctive_ring(pvt->owner);
	ioctl(pvt->jfd, VOIP_SLIC_RING, &params);
}

static int is_cross_line_call_wait(struct jdsp_pvt *p, struct ast_channel *request_ast)
{
	return (p->owner->call_type && !request_ast->call_type) ||
		(!p->owner->call_type && request_ast->call_type);
}

static int jdsp_reorder_tone_timeout(void *data);
static int jdsp_stop_tone(void *data);

static int jdsp_play_tone(struct jdsp_pvt *p, phone_tone_t tone)
{
	tone_param_t param;

	/* Ignore requests to play a tone that is already playing */
	if (tone == p->play_tone)
	    return 0;

	if (p->tone_timeout_id > -1)
	{
		ast_sched_del(sched, p->tone_timeout_id);
		p->tone_timeout_id = -1;
	}

	switch (tone)
	{
	case PHONE_TONE_REORDER:
		p->tone_timeout_id = ast_sched_add(sched, offhookwarningtimeout,
			jdsp_reorder_tone_timeout, p);
		break;
	case PHONE_TONE_UNALLOCATED: 
		p->tone_timeout_id = ast_sched_add(sched, fastbusywarningtimeout,
			jdsp_stop_tone, p);
		break;
	default:
		break;
	}

	param.direction = TONE_DIRECTION_LOCAL;

	if (tone && p->play_tone)
	{
		param.tone = PHONE_TONE_NONE;
	    ioctl(p->jfd, VOIP_LINE_TONE, &param);
	}

	param.tone = tone;
	p->play_tone = tone;

	return ioctl(p->jfd, VOIP_LINE_TONE, &param);
}

static int jdsp_stop_tone(void *data)
{
	struct jdsp_pvt *p = data;

	ast_mutex_lock(&p->lock);
	/* We are the scheduled callback. We can rip ourself out here. */
	p->tone_timeout_id = -1;
	jdsp_play_tone(p, PHONE_TONE_NONE);
	ast_mutex_unlock(&p->lock);

	return 0;
}

static void queue_dialed_digits(struct ast_channel *ast, char *dest)
{
    struct jdsp_pvt *p = ast->tech_pvt;
    
    ast_copy_string(p->callback_extension, dest, sizeof(p->callback_extension));
    ast_queue_control(ast, AST_CONTROL_CALLBACK);        
}

static void call_key_seq_reset(struct jdsp_pvt *p)
{
    p->call_key_seq[0] = '\0';
}

static int call_key_seq_process(struct jdsp_pvt *p, char key)
{
    char *seq = p->call_key_seq;
    struct timeval *ts = &p->call_key_seq_ts;
    int seq_len = strlen(seq);

    if (p->owner->_state != AST_STATE_UP)
	return 0;
    if (!strlen(diag_ivr_activation_seq))
	return 0;

    if (seq_len + 1 == MAX_CALL_KEY_SEQ ||
	(seq_len && ast_tvdiff_ms(ast_tvfromboot(), *ts) >= p->gendigittimeout))
    {
	call_key_seq_reset(p);
	seq_len = 0;
    }

    *ts = ast_tvfromboot();
    seq[seq_len] = key;
    seq[seq_len + 1] = '\0';

    if (!strcmp(seq, diag_ivr_activation_seq))
    {
	call_key_seq_reset(p);
	return 1;
    }

    return 0;
}

static int jdsp_call(struct ast_channel *ast, char *rdest, int timeout)
{
	struct jdsp_pvt *p = ast->tech_pvt;
	char *c, *n, *l;
	char dest[256]; /* must be same length as p->dialdest */
	int is_offhook;
	
	ioctl(p->jfd, VOIP_SLIC_GET_HOOK, &is_offhook);
	ast_mutex_lock(&p->lock);
	ast_copy_string(dest, rdest, sizeof(dest));
	ast_copy_string(p->dialdest, rdest, sizeof(p->dialdest));
	if ((ast->_state == AST_STATE_BUSY)) {
		ast_mutex_unlock(&p->lock);
		return 0;
	}
	if ((ast->_state != AST_STATE_DOWN) && (ast->_state != AST_STATE_RESERVED)) {
		ast_log(LOG_WARNING, "jdsp_call called on %s, neither down nor reserved\n", ast->name);
		ast_mutex_unlock(&p->lock);
		return -1;
	}
	p->outgoing = 1;

	if (p->owner == ast) {
		/* Offhook- callback only */
		if (is_offhook && ast_test_flag(ast, AST_FLAG_CALLBACK))
		{
	    	queue_dialed_digits(p->owner, rdest);
	    	ast_mutex_unlock(&p->lock);
	    	return 0;
		}
		/* Allocate shared data channel */
		if (jdsp_dc_shared_alloc(p, 0, p->owner->shared_callid)) {
			ast_mutex_unlock(&p->lock);
			return -1;
		}

		p->real_dc_num = 0;

		/* nick@dccinc.com 4/3/03 mods to allow for deferred dialing */
		c = strchr(dest, '/');
		if (c)
			c++;

		jdsp_ring(p, 1);
	} else {
		/* Call waiting call */
		p->callwaitrings = 0;
		if (ast->cid.cid_num)
			ast_copy_string(p->callwait_num, ast->cid.cid_num, sizeof(p->callwait_num));
		else
			p->callwait_num[0] = '\0';
		if (ast->cid.cid_name)
			ast_copy_string(p->callwait_name, ast->cid.cid_name, sizeof(p->callwait_name));
		else
			p->callwait_name[0] = '\0';

		if (p->discriminate_call && is_cross_line_call_wait(p, ast))
			ast_log(LOG_DEBUG, "Ignoring cross-line call waiting indication\n");
		else
		{
			/* Call waiting tone instead */
			if (jdsp_callwait(p, 1)) {
				ast_mutex_unlock(&p->lock);
				return -1;
			}
		}
	}
	n = ast->cid.cid_name;
	l = ast->cid.cid_num;
	if (l)
		ast_copy_string(p->lastcid_num, l, sizeof(p->lastcid_num));
	else
		p->lastcid_num[0] = '\0';
	if (n)
		ast_copy_string(p->lastcid_name, n, sizeof(p->lastcid_name));
	else
		p->lastcid_name[0] = '\0';

	call_key_seq_reset(p);
	ast_setstate(ast, AST_STATE_RINGING);
	ast_queue_control(ast, AST_CONTROL_RINGING);	
	ast_mutex_unlock(&p->lock);
	return 0;
}

static void destroy_jdsp_pvt(struct jdsp_pvt **pvt)
{
	struct jdsp_pvt *p = *pvt;
	/* Remove channel from the list */
	if(p->prev)
		p->prev->next = p->next;
	if(p->next)
		p->next->prev = p->prev;
	ast_mutex_destroy(&p->lock);
	free(p);
	*pvt = NULL;
}

static int destroy_channel(struct jdsp_pvt *prev, struct jdsp_pvt *cur, int now)
{
	int owned = 0;
	int i = 0;

	if (!now) {
		if (cur->owner) {
			owned = 1;
		}

		for (i = 0; i < 3; i++) {
			if (cur->subs[i].owner) {
				owned = 1;
			}
		}
		if (!owned) {
			if (prev) {
				prev->next = cur->next;
				if (prev->next)
					prev->next->prev = prev;
				else
					ifend = prev;
			} else {
				iflist = cur->next;
				if (iflist)
					iflist->prev = NULL;
				else
					ifend = NULL;
			}
			if (cur->jfd > -1) {
				jdsp_close(cur->jfd);
			}
			if (cur->io_pipe[0] > -1)
				jdsp_close(cur->io_pipe[0]);
			if (cur->io_pipe[1] > -1)
				jdsp_close(cur->io_pipe[1]);
			destroy_jdsp_pvt(&cur);
		}
	} else {
		if (prev) {
			prev->next = cur->next;
			if (prev->next)
				prev->next->prev = prev;
			else
				ifend = prev;
		} else {
			iflist = cur->next;
			if (iflist)
				iflist->prev = NULL;
			else
				ifend = NULL;
		}
		if (cur->jfd > -1) {
			jdsp_close(cur->jfd);
		}
		if (cur->io_pipe[0] > -1)
			jdsp_close(cur->io_pipe[0]);
		if (cur->io_pipe[1] > -1)
			jdsp_close(cur->io_pipe[1]);
		destroy_jdsp_pvt(&cur);
	}
	return 0;
}

static int jdsp_get_codec_ptime(int ptime, int format)
{
	int x, y;

	for (x = 0; x < 32 ; x++)
	{
		/* find the used codec */
		if (ast_get_bits(global_native_formats.audio_order[x].slot) == format)
		{
			/* Search for required ptime in the codec's ptime list */
			for (y = 0; y < global_native_formats.audio_order[x].num_of_ptimes;
				y++)
			{
				/* if ptime is legitimate, return it */
				if (ptime == global_native_formats.audio_order[x].ptime_list[y])
					return ptime;
			}

			/* Ptime isn't supported, returning the codec's default ptime */
			return global_native_formats.audio_order[x].ptime_list[0]; 
		}
	}

	/* Haven't found the codec, returning the default ptime (First ptime of 
	   First codec */
	return global_native_formats.audio_order[0].ptime_list[0];
}

static void jdsp_set_audio_formats(struct ast_channel *ast, int index)
{
	struct jdsp_pvt *p = ast->tech_pvt;
	
	if (!p->subs[index].rtp)
	{
		ast_log(LOG_DEBUG,"No rtp, unable to set formats\n");
		return;
	}
	ast_rtp_clear_payload_types(p->subs[index].rtp);
	ast_rtp_set_payload_type(p->subs[index].rtp,
		jdsp_codec_ast2rtp(ast->readformat), ast->readformat, 1);
	ast_rtp_set_payload_type(p->subs[index].rtp,
		jdsp_codec_ast2rtp(ast->writeformat), ast->writeformat, 1);
	ast_rtp_set_payload_type(p->subs[index].rtp, -1, AST_RTP_CN, 0);
}

static void jdsp_prepare_audio_args(struct ast_channel *ast, int index, 
	voip_dsp_record_args_t* args)
{
	struct jdsp_pvt *p = ast->tech_pvt;
	int otherindex;

	if (!ast_test_flag(ast, AST_FLAG_DISALLOW_AUDIO_JUMPERS) &&
	    ast_bridged_channel(ast) && ast_bridged_channel(ast)->tech->get_rtp)
	{
	    p->subs[index].rtp = ast_bridged_channel(ast)->tech->get_rtp(ast_bridged_channel(ast));
	}
	else
	    p->subs[index].rtp = 0;

	if (ast_bridged_channel(ast) && ast_bridged_channel(ast)->tech->get_udptl)
	    p->subs[index].udptl = ast_bridged_channel(ast)->tech->get_udptl(ast_bridged_channel(ast));
	
	/* We assume that voice may be started only for SUB_REAL and SUB_THREEWAY */
	otherindex = index == SUB_REAL ? SUB_THREEWAY : SUB_REAL;
	if (p->subs[otherindex].chan == -1)
	{
		/* use currently allocated channel */
		args->channel = p->real_dc_num; 
	}
	else
	{
		/* there is ongoing call already, use the second channel */
		args->channel = OTHER_DC_NUM(p->subs[otherindex].chan); 
	}

	if (!p->owner || p->owner->_state != AST_STATE_UP)
		args->data_mode = VOIP_DATA_MODE_VOICE;
	else if (p->modemdetected)
	    args->data_mode = VOIP_DATA_MODE_MODEM;
	else
	{
	    switch (p->faxtxmethod) {
	    case FAX_NONE:
		args->data_mode = VOIP_DATA_MODE_VOICE;
		break;
	    case FAX_T38_AUTO:
	    case FAX_PASSTHROUGH_AUTO:
		if (p->faxtxmethod == FAX_T38_AUTO && p->t38enabled) {
			args->data_mode = VOIP_DATA_MODE_T38;
			break;
		}
		args->data_mode = p->faxdetected ? VOIP_DATA_MODE_FAX :
		    VOIP_DATA_MODE_VOICE;
		break;
	    case FAX_PASSTHROUGH_FORCE:
		args->data_mode = VOIP_DATA_MODE_FAX;
		break;
	    default:
	    	args->data_mode = VOIP_DATA_MODE_VOICE;
	    }
	}

	/* For T.38 packets we don't use the jdsp->jrtp fastpath because jrtp
	 * is not designed to handle these kinds of packets. So don't bind the
	 * jdsp to a jrtp session in this case. */
	if (p->subs[index].rtp && args->data_mode != VOIP_DATA_MODE_T38)
	{
		char *annexb = pbx_builtin_getvar_helper(ast,"G729_ANNEXB");
		args->rtp_context = ast_rtp_get_context(p->subs[index].rtp);
		args->codec = jdsp_codec_ast2rtp(ast->readformat);
		args->suppress_dtmf = (args->data_mode != VOIP_DATA_MODE_FAX &&
		    args->data_mode != VOIP_DATA_MODE_MODEM &&
		    !ast_rtp_get_inband_dtmf(p->subs[index].rtp));
		args->dtmf_payload = args->suppress_dtmf ?
		    ast_rtp_lookup_code(p->subs[index].rtp, 0, AST_RTP_DTMF) 
		    : 0;
		
		/* jdsp channel information has higher priority */
 		if (!annexb && ast_bridged_channel(ast))
 		{
 			ast_log(LOG_DEBUG, "Couldn't find G729 ANNEXB on %s, checking on the bridged %s\n", ast->name, ast_bridged_channel(ast)->name);
 			annexb = pbx_builtin_getvar_helper(ast_bridged_channel(ast),"G729_ANNEXB");
 		}
 		args->disable_vad = jdsp_is_vad_disabled(args->codec, annexb, ast, !p->silence_suppression_enabled);
	}
	else
	{
		char *disable_vad = pbx_builtin_getvar_helper(ast_bridged_channel(ast),
			"DISABLE_VAD");
		char *suppress_dtmf = pbx_builtin_getvar_helper(
			ast_bridged_channel(ast), "SUPPRESS_DTMF");
		args->rtp_context = 0;
		args->codec = jdsp_codec_ast2rtp(ast->rawreadformat);
		args->suppress_dtmf = !suppress_dtmf || strcmp(suppress_dtmf, "NO");
		args->disable_vad = disable_vad ? !strcmp(disable_vad, "YES") : !p->silence_suppression_enabled;
		args->dtmf_payload = args->suppress_dtmf ?
		    ast_rtp_lookup_code(NULL, 0, AST_RTP_DTMF) : 0;
	}
	
	args->use_t38_args = 0;
	if (p->subs[index].udptl && args->data_mode == VOIP_DATA_MODE_T38)
	{
		args->use_t38_args = 1;
		args->t38_args.ecMode = jdsp_ecmode_ast2udptl(
			ast_udptl_get_error_correction_scheme(p->subs[index].udptl));
		args->t38_args.maxRemDgrm = 
			ast_udptl_get_far_max_datagram(p->subs[index].udptl);
		args->t38_args.maxBitRate =  
			ast_udptl_get_max_bitrate(p->subs[index].udptl);			
		args->t38_args.t38Version =
			ast_udptl_get_t38_version(p->subs[index].udptl);
		args->t38_args.conversionOption = jdsp_conversionmode_ast2udptl(
			ast_udptl_get_conversion_option(p->subs[index].udptl));
		args->t38_args.rateManagementMethod  = jdsp_ratemgt_ast2udptl(
			ast_udptl_get_rate_management_method(p->subs[index].udptl));
		
		ast_log(LOG_DEBUG, "T38 mode: on channel %s ecMode %d, maxRemDgrm %d,"
			" maxBitrate %d, version %d, conversion option "
			"%x,rateManagementMethod %d\n",
			p->owner->name, args->t38_args.ecMode, 
			args->t38_args.maxRemDgrm, args->t38_args.maxBitRate,
			args->t38_args.t38Version, args->t38_args.conversionOption,
			args->t38_args.rateManagementMethod);
	}

	args->ptime_ms = (ast->ptime ? jdsp_get_codec_ptime(ast->ptime, 
		ast->writeformat) : (p->modemdetected || p->faxdetected) ? 
		DEFAULT_FAXMODEM_PTIME : DEFAULT_VOICE_PTIME);
}

static void jdsp_modify_audio(struct ast_channel *ast, int index)
{
	struct jdsp_pvt *p = ast->tech_pvt;
	voip_dsp_record_args_t args = {};
	jdsp_prepare_audio_args(ast, index, &args);
	jdsp_set_audio_formats(ast, index);

	ast_log(LOG_DEBUG, "Modifing audio on channel %s w/ codec %d, suppress_dtmf %d, vad %d, data_mode %d\n", 
 		p->owner->name, args.codec, args.suppress_dtmf, args.disable_vad,
		args.data_mode);

	if (ioctl(p->jfd, VOIP_DSP_MODIFY, &args) < 0) 
		ast_log(LOG_ERROR, "Failed to modify audio on channel %s\n", ast->name);
}

static void jdsp_start_audio(struct ast_channel *ast, int index)
{
	struct jdsp_pvt *p = ast->tech_pvt;
	voip_dsp_record_args_t args = {};
	voip_dsp_bind_arg_t bind_arg;

	if (p->subs[index].chan != -1)
	{
		jdsp_modify_audio(ast,index);
		return;
	}

	jdsp_prepare_audio_args(ast,index, &args);
	jdsp_set_audio_formats(ast, index);

	ast_log(LOG_DEBUG, "Starting audio on channel %s w/ codec %d, suppress_dtmf %d, vad %d, data_mode %d\n", 
 		p->owner->name, args.codec, args.suppress_dtmf, args.disable_vad,
		args.data_mode);


	if (p->subs[index].dfd == -1)
	{
		ast_log(LOG_ERROR, "It's impossible - Starting audio on channel %s, index %d\n",
			ast->name, index);
	}
	else
	{
		bind_arg.line = p->dsp_line - 1;
		bind_arg.channel = args.channel;
		ioctl(p->subs[index].dfd, VOIP_DSP_BIND, &bind_arg);
	}

	if (ioctl(p->jfd, VOIP_DSP_START, &args) < 0) {
		ast_log(LOG_ERROR, "Failed to start audio on channel %s\n", ast->name);
		return;
	}

	p->subs[index].chan = args.channel;
	p->subs[index].seqno = 0;
}

static void print_debug_stats(struct jdsp_pvt *p)
{
	char msg[256];

	snprintf(msg, sizeof(msg), "RxPackets: %d, TxPackets: %d, "
		"RxOctets: %d, TxOctets: %d, RxPacketsLost: %d\n",
		p->stats.rx_packets, p->stats.tx_packets, p->stats.rx_octets,
		p->stats.tx_octets, p->stats.rx_packets_lost);

	ast_log(LOG_DEBUG, "Stats for jdsp - %s\n", msg);
}

static void jdsp_stop_audio(struct ast_channel *ast, int index)
{
	struct jdsp_pvt *p = ast->tech_pvt;

	ast_log(LOG_DEBUG, "Stopping audio on channel %s\n", ast->name);
	
	if (p->subs[index].rtp)
	{
	    ast_rtp_clear_payload_types(p->subs[index].rtp);
	    p->subs[index].rtp = NULL;
	}
	
	/* This is not error case. jdsp_stop_audio may be called when voice is not
	 * started */
	if (p->subs[index].chan == -1)
	{
	    ast_log(LOG_DEBUG, "Can't stop audio on channel %s, "
			"no dsp channel\n", ast->name);
		return;
	}

	/* Adding statistics */
	jdsp_add_stats_from_rtp(&p->stats, p, p->subs[index].chan);
	print_debug_stats(p);

	if (ioctl(p->jfd, VOIP_DSP_STOP, p->subs[index].chan) < 0)
	{
		ast_log(LOG_ERROR, "Failed to stop audio on channel %s, fd %d\n",
			ast->name, p->jfd);
	}

	/* Conference */
	if (p->alloc_count > 1)
	{
		if (p->subs[index].chan != -1)
		{
			jdsp_dc_free(p, p->subs[index].chan);
			p->real_dc_num = OTHER_DC_NUM(p->subs[index].chan);
		}
	}

	p->subs[index].chan = -1;
}

static void jdsp_moh_stop(struct jdsp_pvt *p, int index)
{
    if (internalmoh)
		ast_moh_stop(ast_bridged_channel(p->subs[index].owner));
    else
		ast_queue_control(p->subs[index].owner, AST_CONTROL_UNHOLD);
}

static void jdsp_moh_start(struct jdsp_pvt *p, int index)
{
    if (internalmoh)
		ast_moh_start(ast_bridged_channel(p->subs[index].owner), NULL);
    else
		ast_queue_control(p->subs[index].owner,	AST_CONTROL_HOLD);
}

static int jdsp_reorder_tone_timeout(void *data)
{
	struct jdsp_pvt *p = data;

	ast_mutex_lock(&p->lock);
	/* We are the scheduled callback. We can rip ourself out here. */
	p->tone_timeout_id = -1;
	/* silencio...*/
	jdsp_play_tone(p, PHONE_TONE_NONE);
	if (return_to_held_call)
	{
		if (p->owner)
			ast_queue_hangup(p->owner);
		else if (p->subs[SUB_REAL].owner)
		{
			p->owner = p->subs[SUB_REAL].owner;
			jdsp_moh_stop(p, SUB_REAL);
			jdsp_start_audio(p->subs[SUB_REAL].owner, SUB_REAL);
			ast_clear_flag(p->subs[SUB_REAL].owner, AST_FLAG_CALLWAIT);
			ast_clear_flag(p->subs[SUB_REAL].owner, AST_FLAG_CALL_ONHOLD);
		}
	}
	ast_mutex_unlock(&p->lock);
	
	return 0;
}

static int jdsp_fax_detect_timeout(void *data)
{
	struct jdsp_pvt *p = data;
	struct ast_channel *owner;

	ast_mutex_lock(&p->lock);
	/* called from schedule thread which requires a lock */
	while ((owner = p->owner) && ast_mutex_trylock(&owner->lock)) {
		ast_mutex_unlock(&p->lock);
		usleep(1);
		ast_mutex_lock(&p->lock);
	}

	p->modem_timer_sched_id = -1;

	/* Indicate FAX quality transmission type to peer.
	 * Note: if you wish to change from fax to modem in the middle
	 * of a call, you should do this only if (!p->faxdetected &&
	 * p->faxtxmethod!=FAX_PASSTHROUGH_FORCE) */
	if (p->subs[SUB_REAL].owner)
	{
		ast_log(LOG_DEBUG, "queue AST_CONTROL_MODEM\n");
		jdsp_modify_audio(owner, SUB_REAL);
		jdsp_queue_control(p->subs[SUB_REAL].owner, AST_CONTROL_MODEM);
	}

	ast_mutex_unlock(&p->lock);
	if (owner) {
		ast_mutex_unlock(&owner->lock);
	}

	return 0;
}

static int cancel_fw_disconnect(void *data)
{
	struct jdsp_pvt *p = data;

	if (p->fwd_disconnect == FWD_DISCONNECT_IN_PROGRESS)
		p->fwd_disconnect = FWD_DISCONNECT_ON;
	return 0;
}

static int jdsp_hangup(struct ast_channel *ast)
{
	int index, x;
	struct jdsp_pvt *p = ast->tech_pvt;
	struct jdsp_pvt *tmp = NULL;
	struct jdsp_pvt *prev = NULL;
	int need_fwd_disconnect = 0;

	if (option_debug)
		ast_log(LOG_DEBUG, "jdsp_hangup(%s)\n", ast->name);
	if (!ast->tech_pvt) {
		ast_log(LOG_WARNING, "Asked to hangup channel not connected\n");
		return 0;
	}
	
	ast_mutex_lock(&p->lock);
	
	index = jdsp_get_index(ast, p, 1);

        if (index > -1)
	    jdsp_stop_audio(ast, index);

	if (p->fwd_disconnect == FWD_DISCONNECT_ON &&
	    	(ast->hangupcause == AST_CAUSE_NORMAL ||
		ast->hangupcause == AST_CAUSE_NORMAL_TEMPORARY_FAILURE) &&
		index == SUB_REAL && !p->subs[index].inthreeway &&
		!p->subs[SUB_CALLWAIT].owner)
	{
		if (ioctl(p->jfd, VOIP_SLIC_FWD_DISCONNECT, NULL))
			ast_log(LOG_ERROR, "Forward disconnect failed\n");
		need_fwd_disconnect = 1;
	}

	x = 0;

	if (p->origcid_num) {
		ast_copy_string(p->cid_num, p->origcid_num, sizeof(p->cid_num));
		free(p->origcid_num);
		p->origcid_num = NULL;
	}	
	if (p->origcid_name) {
		ast_copy_string(p->cid_name, p->origcid_name, sizeof(p->cid_name));
		free(p->origcid_name);
		p->origcid_name = NULL;
	}	
	if (p->exten)
		p->exten[0] = '\0';

	ast_log(LOG_DEBUG, "Hangup: channel: %d, index = %d, normal = %p, callwait = %p, thirdcall = %pd\n",
		p->channel, index, p->subs[SUB_REAL].owner, p->subs[SUB_CALLWAIT].owner, p->subs[SUB_THREEWAY].owner);
	
	if (index > -1) {
		/* Real channel, do some fixup */
		p->subs[index].owner = NULL;

		if (index == SUB_REAL) {
			if ((p->subs[SUB_CALLWAIT].owner) && (p->subs[SUB_THREEWAY].owner)) {
				ast_log(LOG_DEBUG, "Normal call hung up with both three way call and a call waiting call in place?\n");
				if (p->subs[SUB_CALLWAIT].inthreeway) {
					/* We had flipped over to answer a callwait and now it's gone */
					ast_log(LOG_DEBUG, "We were flipped over to the callwait, moving back and unowning.\n");
					/* Move to the call-wait, but un-own us until they flip back. */
					swap_subs(p, SUB_CALLWAIT, SUB_REAL);
					unalloc_sub(p, SUB_CALLWAIT);
					p->owner = NULL;
				} else {
					/* The three way hung up, but we still have a call wait */
					ast_log(LOG_DEBUG, "We were in the threeway and have a callwait still.  Ditching the threeway.\n");
					swap_subs(p, SUB_THREEWAY, SUB_REAL);
					unalloc_sub(p, SUB_THREEWAY);
					if (p->subs[SUB_REAL].inthreeway) {
						/* This was part of a three way call.  Immediately make way for
						   another call */
						ast_log(LOG_DEBUG, "Call was complete, setting owner to former third call\n");
						p->owner = p->subs[SUB_REAL].owner;
					} else {
						/* This call hasn't been completed yet...  Set owner to NULL */
						ast_log(LOG_DEBUG, "Call was incomplete, setting owner to NULL\n");
						p->owner = NULL;
					}
					p->subs[SUB_REAL].inthreeway = 0;
				}
			} 
			else if (p->subs[SUB_CALLWAIT].owner || p->subs[SUB_THREEWAY].owner)
			{
				int sub_index = p->subs[SUB_CALLWAIT].owner ? SUB_CALLWAIT :
					SUB_THREEWAY;
				swap_subs(p, sub_index, SUB_REAL);
				unalloc_sub(p, sub_index);
				struct ast_channel *new_owner = NULL;

				/* Check if we had conference before or not */
				if (!p->subs[SUB_REAL].inthreeway)
				{
					if (p->callwaitingalert) 
					{
						/* We have unanswered call waiting*/
						jdsp_callwait(p, 0);
						ast_set_flag(p->subs[SUB_REAL].owner, AST_FLAG_CALLWAIT);
						jdsp_play_tone(p, PHONE_TONE_REORDER);
					}
					else /* Pending held call*/
					{
						if (return_to_held_call)
						{
							ast_log(LOG_DEBUG, "Return to held call\n");
							new_owner = p->subs[SUB_REAL].owner;
							jdsp_play_tone(p, PHONE_TONE_NONE);
							jdsp_moh_stop(p, SUB_REAL);
							/* Only restart audio on main channel if we're not already hanging it up (see
							 * jdsp_handle_event in special MIN_MS_SINCE_FLASH case) */
							if (!(p->subs[SUB_REAL].owner->_softhangup & AST_SOFTHANGUP_DEV))
								jdsp_start_audio(p->subs[SUB_REAL].owner, SUB_REAL);
						}
						else
						{
							ast_set_flag(p->subs[SUB_REAL].owner, AST_FLAG_CALL_ONHOLD);
							jdsp_play_tone(p, PHONE_TONE_REORDER);
						}
					}
				}
				else
				{
					ast_log(LOG_DEBUG, "Call was complete, setting owner to former third call\n");
					new_owner = p->subs[SUB_REAL].owner;
					p->subs[SUB_REAL].inthreeway = 0;
				}
				p->owner = new_owner;
			}
		} else if (index == SUB_CALLWAIT) {
			/* Ditch the holding callwait call, and immediately make it available */
			if (p->subs[SUB_CALLWAIT].inthreeway) {
				/* This is actually part of a three way, placed on hold.  Place the third part
				   on music on hold now */

				if (p->subs[SUB_THREEWAY].owner && ast_bridged_channel(p->subs[SUB_THREEWAY].owner))
				{
					jdsp_stop_audio(p->subs[SUB_THREEWAY].owner, SUB_THREEWAY); /* XXX Need to check whether this line is needed */
					jdsp_moh_start(p, SUB_THREEWAY);
				}

				p->subs[SUB_THREEWAY].inthreeway = 0;
				/* Make it the call wait now */
				swap_subs(p, SUB_CALLWAIT, SUB_THREEWAY);
				unalloc_sub(p, SUB_THREEWAY);
			} else
				unalloc_sub(p, SUB_CALLWAIT);
		} else if (index == SUB_THREEWAY) {
			if (p->subs[SUB_CALLWAIT].inthreeway) {
				/* The other party of the three way call is currently in a call-wait state.
				   Start music on hold for them, and take the main guy out of the third call */

				if (p->subs[SUB_CALLWAIT].owner && ast_bridged_channel(p->subs[SUB_CALLWAIT].owner))
				{
					jdsp_stop_audio(p->subs[SUB_CALLWAIT].owner, SUB_CALLWAIT); /* XXX Need to check whether this line is needed */
					jdsp_moh_start(p, SUB_CALLWAIT);
				}

				p->subs[SUB_CALLWAIT].inthreeway = 0;
			}
			p->subs[SUB_REAL].inthreeway = 0;
			/* If this was part of a three way call index, let us make
			   another three way call */
			unalloc_sub(p, SUB_THREEWAY);
		} else {
			/* This wasn't any sort of call, but how are we an index? */
			ast_log(LOG_WARNING, "Index found but not any type of call?\n");
		}
	}


	jdsp_callwait(p, 0);

	if (!p->subs[SUB_REAL].owner && !p->subs[SUB_CALLWAIT].owner && !p->subs[SUB_THREEWAY].owner) {

		int res, rxisoffhook;
		p->owner = NULL;
		p->distinctivering = 0;
		p->confirmanswer = 0;
		p->outgoing = 0;
		p->onhooktime = time(NULL);
		p->faxdetected = p->modemdetected = 0;
		p->t38enabled = 0;
		p->follow_call = 0;
		p->network_failure_disconnection = 0;

		res = ioctl(p->jfd, VOIP_SLIC_GET_HOOK, &rxisoffhook);
		if (!res) {
			/* If they're off hook, try playing congestion */
			if (rxisoffhook)
			{
				/* reuse of the follow call logic for
				 * implementation of dial restart after flash
				 * key press */
				if (p->need_follow_call || pbx_builtin_getvar_helper(ast, "ALLOW_FOLLOW_CALL"))
                                {
                                        p->need_follow_call = 0;
					p->follow_call = 1;
                                }
				else if (ast->hangupcause == AST_CAUSE_NORMAL_TEMPORARY_FAILURE)
					p->network_failure_disconnection = 1;
				else
					jdsp_play_tone(p, PHONE_TONE_REORDER);
			}
			else
				jdsp_ring(p, 0);
			if (need_fwd_disconnect && rxisoffhook)
			{
				p->fwd_disconnect = FWD_DISCONNECT_IN_PROGRESS;
				ast_sched_add(sched, FWD_DISCONNECT_TIMEOUT, cancel_fw_disconnect, p);
			}
		}
		/* free data channel only if the ioctl was failed or the hook is on and
		 * no active channels remain */
		if (res || !rxisoffhook)
		    jdsp_dc_free(p, p->real_dc_num);
	}

	if (p->pending_flash_key_channel == ast)
	{
		ast_sched_del(sched, p->pending_flash_key);
		p->pending_flash_key = 0;
		p->pending_flash_key_channel = NULL;
	}

	ast->tech_pvt = NULL;
	ast_mutex_unlock(&p->lock);
	ast_mutex_lock(&usecnt_lock);
	usecnt--;
	if (usecnt < 0) 
		ast_log(LOG_WARNING, "Usecnt < 0???\n");
	ast_mutex_unlock(&usecnt_lock);
	ast_update_use_count();
	if (option_verbose > 2) 
		ast_verbose( VERBOSE_PREFIX_3 "Hungup '%s'\n", ast->name);

	ast_mutex_lock(&iflock);
	tmp = iflist;
	prev = NULL;
	ast_mutex_unlock(&iflock);
		
	if (p->follow_call || p->network_failure_disconnection)
	{
		/* Provide a dial tone and start collecting digits */
		start_call(p);
	}

	return 0;
}


static int jdsp_answer(struct ast_channel *ast)
{
	struct jdsp_pvt *p = ast->tech_pvt;
	int res = 0;
	int index;
	int oldstate = ast->_state;
	ast_setstate(ast, AST_STATE_UP);
	ast_mutex_lock(&p->lock);
	index = jdsp_get_index(ast, p, 0);
	if (index < 0)
		index = SUB_REAL;

	/* Pick up the line */
	ast_log(LOG_DEBUG, "Took %s off hook\n", ast->name);
	jdsp_play_tone(p, PHONE_TONE_NONE);
	if ((index == SUB_REAL) && p->subs[SUB_THREEWAY].inthreeway) {
		if (oldstate == AST_STATE_RINGING) {
			ast_log(LOG_DEBUG, "Finally swapping real and threeway\n");
			swap_subs(p, SUB_THREEWAY, SUB_REAL);
			p->owner = p->subs[SUB_REAL].owner;
			index = SUB_THREEWAY;
		}
	}
	jdsp_start_audio(p->subs[index].owner, index);

	ast_mutex_unlock(&p->lock);
	return res;
}

static void jdsp_unlink(struct jdsp_pvt *slave, struct jdsp_pvt *master, int needlock)
{
	/* Unlink a specific slave or all slaves/masters from a given master */
	int x;
	int hasslaves;
	if (!master)
		return;
	if (needlock) {
		ast_mutex_lock(&master->lock);
		if (slave) {
			while(ast_mutex_trylock(&slave->lock)) {
				ast_mutex_unlock(&master->lock);
				usleep(1);
				ast_mutex_lock(&master->lock);
			}
		}
	}
	hasslaves = 0;
	for (x = 0; x < MAX_SLAVES; x++) {
		if (master->slaves[x]) {
			if (!slave || (master->slaves[x] == slave)) {
				/* Take slave out of the conference */
				ast_log(LOG_DEBUG, "Unlinking slave %d from %d\n", master->slaves[x]->channel, master->channel);
				master->slaves[x]->master = NULL;
				master->slaves[x] = NULL;
			} else
				hasslaves = 1;
		}
		if (!hasslaves)
			master->inconference = 0;
	}
	if (!slave) {
		if (master->master) {
			/* Take master out of the conference */
			hasslaves = 0;
			for (x = 0; x < MAX_SLAVES; x++) {
				if (master->master->slaves[x] == master)
					master->master->slaves[x] = NULL;
				else if (master->master->slaves[x])
					hasslaves = 1;
			}
			if (!hasslaves)
				master->master->inconference = 0;
		}
		master->master = NULL;
	}
	if (needlock) {
		if (slave)
			ast_mutex_unlock(&slave->lock);
		ast_mutex_unlock(&master->lock);
	}
}

static int jdsp_fixup(struct ast_channel *oldchan, struct ast_channel *newchan)
{
	struct jdsp_pvt *p = newchan->tech_pvt;
	int x;
	ast_mutex_lock(&p->lock);
	ast_log(LOG_DEBUG, "New owner for channel %d is %s\n", p->channel, newchan->name);
	if (p->owner == oldchan) {
		p->owner = newchan;
	}
	for (x = 0; x < 3; x++)
		if (p->subs[x].owner == oldchan) {
			if (!x)
				jdsp_unlink(NULL, p, 0);
			p->subs[x].owner = newchan;
		}
	if (newchan->_state == AST_STATE_RINGING) 
		jdsp_indicate(newchan, AST_CONTROL_RINGING, NULL, 0);
	ast_mutex_unlock(&p->lock);
	return 0;
}

static void *ss_thread(void *data);

static struct ast_channel *jdsp_new(struct jdsp_pvt *, int, int, int, int);

static int attempt_transfer(struct jdsp_pvt *p, int target_owner_index)
{
	/* In order to transfer, we need at least one of the channels to
	   actually be in a call bridge.  We can't conference two applications
	   together (but then, why would we want to?) */
	if (ast_bridged_channel(p->subs[SUB_REAL].owner)) {
		/* The three-way person we're about to transfer to could still be in MOH, so
		   stop if now if appropriate */
		if (ast_bridged_channel(p->subs[target_owner_index].owner))
			jdsp_moh_stop(p, target_owner_index);
		if (p->subs[target_owner_index].owner->_state == AST_STATE_RINGING) {
			ast_indicate(ast_bridged_channel(p->subs[SUB_REAL].owner), AST_CONTROL_RINGING);
		}
		if (p->subs[SUB_REAL].owner->cdr) {
			/* Move CDR from second channel to current one */
			p->subs[target_owner_index].owner->cdr =
				ast_cdr_append(p->subs[target_owner_index].owner->cdr, p->subs[SUB_REAL].owner->cdr);
			p->subs[SUB_REAL].owner->cdr = NULL;
		}
		if (ast_bridged_channel(p->subs[SUB_REAL].owner)->cdr) {
			/* Move CDR from second channel's bridge to current one */
			p->subs[target_owner_index].owner->cdr =
				ast_cdr_append(p->subs[target_owner_index].owner->cdr, ast_bridged_channel(p->subs[SUB_REAL].owner)->cdr);
			ast_bridged_channel(p->subs[SUB_REAL].owner)->cdr = NULL;
		}
		/* Last chance to stop audio on the channel before it becomes a
		 * zombie */
		jdsp_stop_audio(p->subs[target_owner_index].owner, target_owner_index);
		 if (ast_channel_masquerade(p->subs[target_owner_index].owner, ast_bridged_channel(p->subs[SUB_REAL].owner))) {
			ast_log(LOG_WARNING, "Unable to masquerade %s as %s\n",
					ast_bridged_channel(p->subs[SUB_REAL].owner)->name, p->subs[target_owner_index].owner->name);
			return -1;
		}
		/* Orphan the channel after releasing the lock */
		ast_mutex_unlock(&p->subs[target_owner_index].owner->lock);
		unalloc_sub(p, target_owner_index);
	} else if (ast_bridged_channel(p->subs[target_owner_index].owner)) {
		if (p->subs[SUB_REAL].owner->_state == AST_STATE_RINGING) {
			ast_indicate(ast_bridged_channel(p->subs[target_owner_index].owner), AST_CONTROL_RINGING);
		}
		if (internalmoh)
			ast_moh_stop(ast_bridged_channel(p->subs[target_owner_index].owner));
		else				  		  
			ast_indicate(ast_bridged_channel(p->subs[target_owner_index].owner),
				AST_CONTROL_UNHOLD);
    	if (p->subs[target_owner_index].owner->cdr) {
			/* Move CDR from second channel to current one */
			p->subs[SUB_REAL].owner->cdr = 
				ast_cdr_append(p->subs[SUB_REAL].owner->cdr, p->subs[target_owner_index].owner->cdr);
			p->subs[target_owner_index].owner->cdr = NULL;
		}
		if (ast_bridged_channel(p->subs[target_owner_index].owner)->cdr) {
			/* Move CDR from second channel's bridge to current one */
			p->subs[SUB_REAL].owner->cdr = 
				ast_cdr_append(p->subs[SUB_REAL].owner->cdr, ast_bridged_channel(p->subs[target_owner_index].owner)->cdr);
			ast_bridged_channel(p->subs[target_owner_index].owner)->cdr = NULL;
		}
		/* Last chance to stop audio on the channel before it becomes a
		 * zombie */
		jdsp_stop_audio(p->subs[SUB_REAL].owner, SUB_REAL);
		if (ast_channel_masquerade(p->subs[SUB_REAL].owner, ast_bridged_channel(p->subs[target_owner_index].owner))) {
			ast_log(LOG_WARNING, "Unable to masquerade %s as %s\n",
					ast_bridged_channel(p->subs[target_owner_index].owner)->name, p->subs[SUB_REAL].owner->name);
			return -1;
		}
		/* Three-way or callwait  is now the REAL */
		swap_subs(p, target_owner_index, SUB_REAL);
		ast_mutex_unlock(&p->subs[SUB_REAL].owner->lock);
		unalloc_sub(p, target_owner_index);
		/* Tell the caller not to hangup */
		return 1;
	} else {
		ast_log(LOG_DEBUG, "Neither %s nor %s are in a bridge, nothing to transfer\n",
					p->subs[SUB_REAL].owner->name, p->subs[target_owner_index].owner->name);
		p->subs[target_owner_index].owner->_softhangup |= AST_SOFTHANGUP_DEV;
		return -1;
	}
	return 0;
}

static int start_transfer_call(struct ast_channel *ast)
{
	struct jdsp_pvt *p = ast->tech_pvt;
	struct ast_channel *target_owner = NULL;
	int target_owner_index = -1;

	if (p->subs[SUB_THREEWAY].owner)
	{
		target_owner = p->subs[SUB_THREEWAY].owner;
		target_owner_index = SUB_THREEWAY;	
	}
	else if (p->subs[SUB_CALLWAIT].owner)
	{
		target_owner = p->subs[SUB_CALLWAIT].owner;
		target_owner_index = SUB_CALLWAIT;	
	}
	else
	{
		ast_log(LOG_DEBUG, "Transfer failed. We do not have"
		    "target owner");
		return 1;
	}
	int inthreeway = p->subs[SUB_REAL].inthreeway &&
		p->subs[SUB_THREEWAY].inthreeway;

	/* In any case this isn't a threeway call anymore */
	p->subs[SUB_REAL].inthreeway = 0;
	p->subs[SUB_THREEWAY].inthreeway = 0;

	/* Only attempt transfer if the phone is ringing */
	if (!p->transfertobusy && ast->_state == AST_STATE_BUSY)
	{
		ast_mutex_unlock(&target_owner->lock);
		/* Swap subs and dis-own channel */
		swap_subs(p, target_owner_index, SUB_REAL);
		p->owner = NULL;
		/* Ring the phone */
		jdsp_ring(p, 1);
	}
	else if (p->transfermode == TRANSFER_BRIDGING)
	{
		struct ast_channel *target_owner_bridge = 
			ast_bridged_channel(target_owner);

		if (p->internal_transfer_only &&
			!jdsp_is_internal_call_leg(target_owner_bridge, ast))
		{
			ast_log(LOG_DEBUG,"The call cannot be transfered because of the	"
				"internal call constraint\n");
			return 1;
		}

		int res = attempt_transfer(p, target_owner_index);

		if (res < 0)
		{			
			p->subs[target_owner_index].owner->_softhangup |= AST_SOFTHANGUP_DEV;
			if (p->subs[target_owner_index].owner)
				ast_mutex_unlock(&p->subs[target_owner_index].owner->lock);
			return 1;
		}
		else if (res)
		{
			/* Don't actually hang up at this point */
			if (p->subs[target_owner_index].owner)
				ast_mutex_unlock(&p->subs[target_owner_index].owner->lock);
		}
	}
	else if (p->transfermode == TRANSFER_SIGNALLING)
	{
		struct ast_frame f;
		/* XXX If transfer-target hasn't answered yet, 'target_chan' will be
		 * NULL and transfer will not be performed. We should add code that
		 * knows how to obtain 'target_chan' in this scenario, and perform
		 * blind transfer. */
		struct ast_channel *target_chan;
		int unlock_sub_index = SUB_THREEWAY;

		/* If we are in a conference call, undo the swap performed by
		 * start_conference. This is done in order to prevent the problem
		 * described in B39277. */
		if (inthreeway)
		{
			swap_subs(p, SUB_THREEWAY, SUB_REAL);
			unlock_sub_index = SUB_REAL;
			target_owner = p->subs[SUB_THREEWAY].owner;
		}
		
		target_chan = ast_bridged_channel(p->subs[SUB_REAL].owner);
		if (target_chan)
		{
			/* Signaled transfer available only between SIP channels */
		if(strncasecmp(target_chan->type, "SIP", strlen("SIP")) ||
			   strncasecmp(ast_bridged_channel(target_owner)->type, "SIP", strlen("SIP")))
			{
				ast_log(LOG_DEBUG,"Attempt to do signalled transfer between "
					"non-SIP channels: target_chan=%s, owner=%s, fallback to "
					"bridged\n", target_chan->type, target_owner->type);
				int res = attempt_transfer(p, target_owner_index);
				if (res < 0)
				{
					target_owner->_softhangup |= AST_SOFTHANGUP_DEV;
					ast_mutex_unlock(&target_owner->lock);
					return 1;
				}
				else if (res)
				{
					/* Don't actually hang up at this point */
					ast_log(LOG_DEBUG,"Error on bridged transfer\n");
					ast_mutex_unlock(&target_owner->lock);
					return 0;
				}
				else
				{
					ast_log(LOG_DEBUG,"Call transfered sucessfully\n");
					return 0;
				}
			}
			int index = jdsp_get_index(ast, p, 0);
			/* Tell bridged channel to do attended transfer */
			memset(&f, 0, sizeof(f));
			f.frametype = AST_FRAME_ATTENDEDTRANSFER;
			f.data = &target_chan;
			f.datalen = sizeof(struct ast_channel *);
			ast_queue_frame(target_owner, &f);
			if (p->subs[unlock_sub_index].owner)
					ast_mutex_unlock(&p->subs[unlock_sub_index].owner->lock);

			p->subs[index].f.frametype = AST_FRAME_NULL;
			p->subs[index].f.subclass = 0;
		}
		else
		{
			target_owner->_softhangup |= AST_SOFTHANGUP_DEV;
			if (p->subs[unlock_sub_index].owner)
				ast_mutex_unlock(&p->subs[unlock_sub_index].owner->lock);
			return 1;
		}
	}
	return 0;
}

static void start_conference(struct ast_channel *ast, struct jdsp_pvt *p)
{
	int otherindex = SUB_THREEWAY;
	
	if (!p->subs[SUB_THREEWAY].owner && p->subs[SUB_CALLWAIT].owner)
	{
		ast_log(LOG_DEBUG,"Not a 3way channel to be confereced with, switching "
			"callwait to 3way\n");
		swap_subs(p, SUB_THREEWAY, SUB_CALLWAIT);
	}

	if (option_verbose > 2)
	{
		ast_verbose(VERBOSE_PREFIX_3 "Building conference on call on %s and %s"
			    		    "\n", p->subs[SUB_THREEWAY].owner->name, 
	    	p->subs[SUB_REAL].owner->name);
    	}
    	/* Put them in the threeway, and flip */
    	p->subs[SUB_THREEWAY].inthreeway = 1;
    	p->subs[SUB_REAL].inthreeway = 1;
    	if (ast->_state == AST_STATE_UP) {
		swap_subs(p, SUB_THREEWAY, SUB_REAL);
		otherindex = SUB_REAL;
    	}
    	else
		jdsp_play_tone(p, PHONE_TONE_NONE);

    	if (p->subs[otherindex].owner &&
	    	ast_bridged_channel(p->subs[otherindex].owner))
    	{
		int index = otherindex == SUB_REAL ? SUB_THREEWAY : SUB_REAL;
		jdsp_stop_audio(p->subs[index].owner, index);
		jdsp_moh_stop(p, otherindex);
		jdsp_start_audio(p->subs[otherindex].owner, otherindex);
		jdsp_start_audio(p->subs[index].owner, index);
	}

    	p->owner = p->subs[SUB_REAL].owner;
    	if (ast->_state == AST_STATE_RINGING) {
		ast_log(LOG_DEBUG, "Enabling ringtone on real and threeway\n");
		jdsp_play_tone(p, PHONE_TONE_RING);
	}
}

static void drop_last_party_from_conference(struct jdsp_pvt *p)
{
	/* Call is already up, drop the last person */
	if (option_debug)
		ast_log(LOG_DEBUG, "Got flash with three way call up, dropping last call on %d\n", p->channel);
	/* If the primary call isn't answered yet, use it */
	if ((p->subs[SUB_REAL].owner->_state != AST_STATE_UP) && (p->subs[SUB_THREEWAY].owner->_state == AST_STATE_UP)) {
		/* Swap back -- we're dropping the real 3-way that isn't finished yet*/
		swap_subs(p, SUB_THREEWAY, SUB_REAL);
		p->owner = p->subs[SUB_REAL].owner;
	}
	/* Drop the last call and stop the conference */
	if (option_verbose > 2)
		ast_verbose(VERBOSE_PREFIX_3 "Dropping three-way call on %s\n", p->subs[SUB_THREEWAY].owner->name);
	jdsp_play_tone(p, PHONE_TONE_NONE);
	ast_softhangup_nolock(p->subs[SUB_THREEWAY].owner, AST_SOFTHANGUP_DEV);
	p->subs[SUB_REAL].inthreeway = 0;
	p->subs[SUB_THREEWAY].inthreeway = 0;
}

static void separate_conference(struct jdsp_pvt *p, int active_index)
{
	/* Check if both parties are still up */
	if (!p->subs[SUB_THREEWAY].owner || !p->subs[SUB_REAL].owner ||
		p->subs[SUB_THREEWAY].owner->_state != AST_STATE_UP ||
		p->subs[SUB_REAL].owner->_state != AST_STATE_UP )
	{
		ast_log(LOG_WARNING, "Trying to separate conference while one of the "
			"sides is already gone\n");
		return;
	}
	p->subs[SUB_THREEWAY].inthreeway = 0;
	p->subs[SUB_REAL].inthreeway = 0;

	if (active_index == SUB_THREEWAY) 
	{
		/* 3way will be active- swap it with real  */
		swap_subs(p, SUB_THREEWAY, SUB_REAL);
	}

	p->owner = p->subs[SUB_REAL].owner;
	jdsp_stop_audio(p->subs[SUB_THREEWAY].owner, SUB_THREEWAY);
	jdsp_moh_start(p, SUB_THREEWAY);
}

static void switch_calls(struct jdsp_pvt *p, int is_callwaiting)
{
    	int other_sub = is_callwaiting ? SUB_CALLWAIT : SUB_THREEWAY;

	swap_subs(p, SUB_REAL, other_sub);
	if (is_callwaiting)
		jdsp_callwait(p, 0);
	p->owner = p->subs[SUB_REAL].owner;
	ast_log(LOG_DEBUG, "Making %s the new owner\n", p->owner->name);
	
	if (!p->subs[other_sub].inthreeway && ast_bridged_channel(p->subs[other_sub].owner)) {
	        /* Hold other channel */
		jdsp_stop_audio(p->subs[other_sub].owner, other_sub);
		jdsp_moh_start(p, other_sub);
	}

	if (is_callwaiting && p->owner->_state == AST_STATE_RINGING) {
	    	/* Channel is ringing, answer */
		ast_setstate(p->owner, AST_STATE_UP);
		jdsp_queue_control(p->owner, AST_CONTROL_ANSWER);
	} else if (ast_bridged_channel(p->owner)) {
	        /* Channel is on hold, unhold it */
		jdsp_moh_stop(p, SUB_REAL);
		jdsp_start_audio(p->owner, SUB_REAL);
    	}

}

static void start_threeway_call(struct jdsp_pvt *p)
{
	struct ast_channel *chan;
	int res;
	pthread_t threadid;
	pthread_attr_t attr;
	char cid_num[256];
	char cid_name[256];

	ast_log(LOG_DEBUG, "in start_threeway_call\n");	

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	if (p->jdsptrcallerid && p->owner) {
		if (p->owner->cid.cid_num)
			ast_copy_string(cid_num, p->owner->cid.cid_num, sizeof(cid_num));
		if (p->owner->cid.cid_name)
			ast_copy_string(cid_name, p->owner->cid.cid_name, sizeof(cid_name));
	}

	/* Make new channel */
	chan = jdsp_new(p, AST_STATE_RESERVED, 0, SUB_THREEWAY, 0);
	if (p->jdsptrcallerid) {
		if (!p->origcid_num)
			p->origcid_num = strdup(p->cid_num);
		if (!p->origcid_name)
			p->origcid_name = strdup(p->cid_name);
		ast_copy_string(p->cid_num, cid_num, sizeof(p->cid_num));
		ast_copy_string(p->cid_name, cid_name, sizeof(p->cid_name));
	}
	/* Swap things around between the three-way and real call */
	swap_subs(p, SUB_THREEWAY, SUB_REAL);
	res = jdsp_play_tone(p, get_dialtone(p));
	if (res)
		ast_log(LOG_WARNING, "Unable to start dial recall tone on channel %d\n", p->channel);
	p->owner = chan;
	if (!chan) {
		ast_log(LOG_WARNING, "Cannot allocate new structure on channel %d\n", p->channel);
	} else if (ast_pthread_create(&threadid, &attr, ss_thread, chan)) {
		ast_log(LOG_WARNING, "Unable to start simple switch on channel %d\n", p->channel);
		res = jdsp_play_tone(p, PHONE_TONE_REORDER);
		ast_hangup(chan);
	} else {
		if (option_verbose > 2)	
			ast_verbose(VERBOSE_PREFIX_3 "Started three way call on channel %d\n", p->channel);

		/* Start music on hold if appropriate */
		if (ast_bridged_channel(p->subs[SUB_THREEWAY].owner))
		{
			jdsp_stop_audio(p->subs[SUB_THREEWAY].owner, SUB_THREEWAY);
			jdsp_moh_start(p, SUB_THREEWAY);
		}
		else
			ast_queue_hangup(p->subs[SUB_THREEWAY].owner);
	}		
}

static int jdsp_on_flash_key_timeout(void* data);

static int jdsp_answer_call_waiting(int fd, int argc, char **argv)
{
	struct jdsp_pvt *p = NULL;
	int channel, r_plus_key = -1;
	phone_key_t key;
	static code2code_t flash_keys[] = {
	    {0, PHONE_KEY_0},
	    {1, PHONE_KEY_1},
	    {2, PHONE_KEY_2},
	    {3, PHONE_KEY_3},
	    {4, PHONE_KEY_4},
	    {5, PHONE_KEY_5},
	    {6, PHONE_KEY_6},
	    {7, PHONE_KEY_7},
	    {8, PHONE_KEY_8},
	    {9, PHONE_KEY_9},
    	{-1, -1} 
	};

	if (argc < 4)
		return RESULT_SHOWUSAGE;

	channel = atoi(argv[3]);
	if(argc == 6)
		r_plus_key = atoi(argv[5]);
	for (p = iflist; p && p->channel != channel; p = p->next);
	if (!p)
		return RESULT_FAILURE;
	if (argc == 4)
		r_plus_key = 2;

	key = code2code(flash_keys, r_plus_key);
	jdsp_self_enqueue_event(p, PHONE_KEY_FLASH, 0);
	if (key != -1)
	{
		jdsp_self_enqueue_event(p, key, 1);
		jdsp_self_enqueue_event(p, key, 0);
	}
	return RESULT_SUCCESS;

}

static int jdsp_show_call_matrix(int fd, int argc, char **argv)
{
	struct jdsp_pvt *p = NULL;
	int channel, state, key, action;

	if (argc != 4)
		return RESULT_SHOWUSAGE;

	channel = atoi(argv[3]);

	for (p = iflist; p && p->channel != channel; p = p->next);
	if (!p)
		return RESULT_FAILURE;
	ast_cli(fd, "Call Matrix: [State] [Key] => Action\n");	
	for (state = HOOKED_OFF_DIALTONE; state <= STATE_LAST; state++)
	{
		for (key = FLASH_KEY; key <= LAST_ACTION_KEY; key++)
		{
			action = p->call_matrix[state][key];
			if (action)
			{
				ast_cli(fd, "\t[%-19s] [%-7s] => %s \n",
					code2str(states_msg_str, state), 
					code2str(flash_msg_str, key), 
					code2str(actions_msg_str, action));	
			}
		}
	}
	return RESULT_SUCCESS;
}

static call_matrix_states_t jdsp_get_call_matrix_state(struct ast_channel *ast)
{
    struct jdsp_pvt *p;
	/* The only case when ast == NULL, if user didn't dial anything
		and channel was destroyed. FAST BUSY or silence on line, so it's
		corresponds to LINE_FBUSY_TONE state */
	if (!ast)
		return LINE_FBUSY_TONE;

	p = ast->tech_pvt;
    /* If AST_FLAG_CALLWAIT is on, then this channel was not answered yet.
       use call waiting answer flash sequence also in this case */ 
	if (ast_test_flag(ast, AST_FLAG_CALLWAIT))
		return CALLWAITING_HANGUP; 
	else if (p->subs[SUB_CALLWAIT].owner) 
	{
		if (p->subs[SUB_CALLWAIT].owner->_state == AST_STATE_UP)
			return CALLHOLD_ACTIVECALL;
		return CALLWAITING;
	} 
	else if (!p->subs[SUB_THREEWAY].owner) 
	{
		/* No active Call or only ONE Active*/
		switch (ast->_state)
		{
		case AST_STATE_UP:
			return ast_test_flag(ast, AST_FLAG_CALL_ONHOLD) ?
				CALLHOLD_FBUSYTONE : ONE_ACTIVE_CALL;
		case AST_STATE_RESERVED: 
		case AST_STATE_DIALING:
			return HOOKED_OFF_DIALTONE;
		case AST_STATE_RINGING: 
			return LINE_RING_BACK;
		case AST_STATE_BUSY:
			if (ast->hangupcause == AST_CAUSE_UNALLOCATED)
				return LINE_FBUSY_TONE;
			return LINE_BUSY_TONE;
		default:
			return UNKNOWN; 
		}
	} 
	else /* Already have a 3 way call */ 
	{
		if (p->subs[SUB_THREEWAY].inthreeway) 
			return CONFERENCE;
		else 
		{
			if (ast->_state == AST_STATE_UP) 
				return CALLHOLD_3WAY_CALL;
			else 
			{
				switch (ast->_state)
				{
				case AST_STATE_DIALING:
				case AST_STATE_RESERVED: 
					return CALLHOLD_DIALTONE;
				case AST_STATE_RINGING: 
					return CALLHOLD_RINGBACK;
				case AST_STATE_BUSY:
					if (ast->hangupcause == AST_CAUSE_UNALLOCATED)
						return CALLHOLD_FBUSYTONE;
					return CALLHOLD_BUSYTONE;
				default:
					return UNKNOWN; 
				}
			}
		}
	}
	return UNKNOWN; 
}

static int need_flash_key_timeout(struct jdsp_pvt *p,
	call_matrix_states_t state)
{
	flash_key_seq_t i;

	for (i = FLASH_KEY_PLUS_0; i <= LAST_ACTION_KEY; i++)
	{
		if (p->call_matrix[state][i] != IGNORE)
			return 1;
	}
	return 0;
}

static int jdsp_is_action_forbidden(struct ast_channel *ast, call_matrix_actions_t action)
{
    call_matrix_actions_t act, emergency_call_hold_forbidden_actions[] = {
	DROP_AND_RESTART_DIALTONE,
	SWITCH_CALL,
	DROP_AND_SWITCH,
	STOP_TONE_AND_SWITCH,
	PUT_ON_HOLD,
	-1,
    };
    int i;

    if (!jdsp_is_call_hold_enabled(ast))
    {
	for (i = 0, act = emergency_call_hold_forbidden_actions[i]; act > 0 &&
	    act != action; act = emergency_call_hold_forbidden_actions[++i]);

	return act > 0;
    }

    return 0;
}

static void jdsp_handle_flash_key(struct ast_channel *ast, struct jdsp_pvt *p, 
	flash_key_seq_t flash_seq, int index)
{
    call_matrix_states_t state = jdsp_get_call_matrix_state(ast);
	call_matrix_actions_t action;

    ast_log(LOG_DEBUG, "In jdsp_handle_flash_key. key %s, ast %s, index %d state:%d\n",
		code2str(flash_msg_str, flash_seq), ast ? ast->name : "(null)", index,
		ast ? ast->_state : -1);

	if (state == UNKNOWN)
	{
		ast_log(LOG_DEBUG, "In jdsp_handle_flash_key. State: UNKNOWN\n");
		return;
	}

	p->flashtime = ast_tvfromboot();
	if (ast && !p->pending_flash_key)
	{
		if (need_flash_key_timeout(p, state))
	    	{
			p->pending_flash_key_channel = ast;
			p->pending_flash_key = ast_sched_add(sched,
				FLASH_KEY_TIMEOUT, jdsp_on_flash_key_timeout, p);
			return;
	    	}
	}
	else
	{
		p->pending_flash_key = 0;
		p->pending_flash_key_channel = NULL;
	}

	if (flash_seq > LAST_ACTION_KEY)
	{
		ast_log(LOG_DEBUG, "In jdsp_handle_flash_key. Unsupported flash"
			"sequence %s\n", code2str(flash_msg_str, flash_seq));
		return;
	}
	action = p->call_matrix[(int)state][flash_seq];

	ast_log(LOG_DEBUG, "In jdsp_handle_flash_key. State: %s Action: %s\n",
		code2str(states_msg_str, state), code2str(actions_msg_str, action));

	if (jdsp_is_action_forbidden(ast, action))
	{
	    ast_log(LOG_DEBUG, "In jdsp_handle_flash_key. Action %s is "
		"forbidden, changing action to %s\n",
		code2str(actions_msg_str, action), "IGNORE");
	    action = IGNORE;
	}

	switch (action)
	{
	case RESTART_DIALTONE:
		p->need_follow_call = 1;
		ast_softhangup_nolock(p->subs[SUB_REAL].owner, AST_SOFTHANGUP_DEV);
		break;
	case DROP_AND_RESTART_DIALTONE:
		if (LINE_RING_BACK == state)
		{
			if (p->subs[index].owner)
			{
				p->need_follow_call = 1;
				ast_softhangup_nolock(p->subs[index].owner, AST_SOFTHANGUP_DEV);
			}
			else
				ast_log(LOG_WARNING, "DROP_AND_RESTART_DIALTONE: null owner.\n");
		}
                else if (LINE_FBUSY_TONE == state || LINE_BUSY_TONE == state)
                {
			jdsp_dc_free(p, p->real_dc_num);
			alloc_and_start_call(p);
		}
		break;
	case ENQUEUE_FLASH_EVENT:
		jdsp_queue_control(p->subs[SUB_REAL].owner, AST_CONTROL_FLASH);
		break;
	case SWITCH_CALL:
		/*Ignoring during cross-line*/
		if (!(p->discriminate_call && is_cross_line_call_wait(p, 
			p->subs[SUB_CALLWAIT].owner)))
		{
			switch_calls(p, 1);
		}
		break;
	case SWITCH_3WAY_CALL:
		switch_calls(p, 0);
		break;
	case SWITCH_HG_CALLWAITNG_CALL:
		{
			struct ast_frame f;
			memset(&f, 0 , sizeof(f));
			jdsp_play_tone(p, PHONE_TONE_NONE);
			p->owner = p->subs[SUB_REAL].owner;
			f.frametype = AST_FRAME_CONTROL;
			f.subclass = AST_CONTROL_ANSWER;
			ast_clear_flag(p->owner, AST_FLAG_CALLWAIT);
			ast_setstate(p->owner, AST_STATE_UP);
			ast_queue_frame(p->owner, &f);
			break;
		}
	case DROP_AND_SWITCH:
		/*Ignoring during cross-line*/
		if (!(p->discriminate_call && is_cross_line_call_wait(p, 
			p->subs[SUB_CALLWAIT].owner)))
		{
			switch_calls(p, 1);
			ast_softhangup_nolock(p->subs[SUB_CALLWAIT].owner, 
				AST_SOFTHANGUP_DEV);
		}
		break;
	case STOP_TONE_AND_SWITCH:
		jdsp_play_tone(p, PHONE_TONE_NONE);
		/* stop the audio, started on RINGING event */
 		if (p->subs[SUB_REAL].owner)
 		    jdsp_stop_audio(p->subs[SUB_REAL].owner, SUB_REAL);
		else
		{
		    ast_log(LOG_WARNING, "STOP_TONE_AND_SWITCH: null owner. "
		        "Suppress stop audio, ra=%p\n", __builtin_return_address(0));
		}
		if (p->subs[SUB_THREEWAY].owner)
		{
			swap_subs(p, SUB_THREEWAY, SUB_REAL);
			p->subs[SUB_THREEWAY].owner->_softhangup |= AST_SOFTHANGUP_DEV;
		}
		p->owner = p->subs[SUB_REAL].owner;
		ast_clear_flag(p->owner, AST_FLAG_CALL_ONHOLD);
		ast_clear_flag(p->owner, AST_FLAG_CALLWAIT);

		if (p->subs[SUB_REAL].owner && 
			ast_bridged_channel(p->subs[SUB_REAL].owner))
		{
			jdsp_moh_stop(p, SUB_REAL);
			/*XXX Need to check whether this line is needed*/
			jdsp_start_audio(p->subs[SUB_REAL].owner, SUB_REAL); 
		}
		break;

	case PUT_ON_HOLD:
		start_threeway_call(p);
		break;
	case REJECT_INCOMING_CALL:
		p->subs[SUB_CALLWAIT].owner->hangupcause = AST_CAUSE_BUSY;
		ast_softhangup_nolock(p->subs[SUB_CALLWAIT].owner, AST_SOFTHANGUP_DEV);
		break;
	case TERMINATE_3WAYHELD_CALL:
		ast_softhangup_nolock(p->subs[SUB_THREEWAY].owner, AST_SOFTHANGUP_DEV);
		break;
	case TERMINATE_HELD_CALL:
		ast_softhangup_nolock(p->subs[SUB_CALLWAIT].owner, AST_SOFTHANGUP_DEV);
		break;
	case TERMINATE_ACTIVE_SWITCH_TO_HELD:
		switch_calls(p, 0);
		ast_softhangup_nolock(p->subs[SUB_THREEWAY].owner, AST_SOFTHANGUP_DEV);
		break;
	case START_CONFERENCE:
		if ((p->transfertobusy || (ast->_state != AST_STATE_BUSY)) && 
			!jdsp_dc_exclusive_alloc(p, OTHER_DC_NUM(p->real_dc_num),0))
		{
			start_conference(ast, p); 
		}
		break;
	case DROP_LAST_PARTY_FROM_CONF:
		drop_last_party_from_conference(p);
		break;
	case TRANSFER_CALL:
		start_transfer_call(ast);
		break;
	case HOLD_LAST_PARTY_FROM_CONF:
		separate_conference(p, SUB_REAL);
		break;
	case HOLD_FIRST_PARTY_FROM_CONF:
		separate_conference(p, SUB_THREEWAY);
		break;
	default: // Ignore action
		return;
	}
}

void configure_call_matrix_es(struct jdsp_pvt *pvt)
{
	if (pvt->threewaycalling)
	{
		pvt->call_matrix[ONE_ACTIVE_CALL][FLASH_KEY] = PUT_ON_HOLD;
		pvt->call_matrix[ONE_ACTIVE_CALL][FLASH_KEY_PLUS_2] = PUT_ON_HOLD;
	}
	else
	{
		pvt->call_matrix[HOOKED_OFF_DIALTONE][FLASH_KEY] =
			pvt->call_matrix[LINE_RING_BACK][FLASH_KEY] = 
			pvt->call_matrix[LINE_BUSY_TONE][FLASH_KEY] =
			pvt->call_matrix[LINE_FBUSY_TONE][FLASH_KEY] =
			pvt->call_matrix[ONE_ACTIVE_CALL][FLASH_KEY] =
			ENQUEUE_FLASH_EVENT;
	}

	pvt->call_matrix[CALLWAITING][FLASH_KEY_PLUS_0] = REJECT_INCOMING_CALL;
	pvt->call_matrix[CALLWAITING][FLASH_KEY_PLUS_1] = DROP_AND_SWITCH;
	pvt->call_matrix[CALLWAITING][FLASH_KEY] =
		pvt->call_matrix[CALLWAITING][FLASH_KEY_PLUS_2] = SWITCH_CALL;
	pvt->call_matrix[CALLWAITING_HANGUP][FLASH_KEY_PLUS_2] = 
		SWITCH_HG_CALLWAITNG_CALL;
	pvt->call_matrix[CALLHOLD_ACTIVECALL][FLASH_KEY_PLUS_2] = SWITCH_CALL;
	pvt->call_matrix[CALLHOLD_ACTIVECALL][FLASH_KEY_PLUS_0] = 
		TERMINATE_HELD_CALL;
	pvt->call_matrix[CALLHOLD_ACTIVECALL][FLASH_KEY_PLUS_1] = DROP_AND_SWITCH;
	pvt->call_matrix[CALLHOLD_DIALTONE][FLASH_KEY_PLUS_2] = 
		pvt->call_matrix[CALLHOLD_FBUSYTONE][FLASH_KEY_PLUS_2] = 
		pvt->call_matrix[CALLHOLD_BUSYTONE][FLASH_KEY_PLUS_2] = 
		STOP_TONE_AND_SWITCH;
	pvt->call_matrix[CALLHOLD_3WAY_CALL][FLASH_KEY_PLUS_0] = 
		TERMINATE_3WAYHELD_CALL;
	pvt->call_matrix[CALLHOLD_3WAY_CALL][FLASH_KEY_PLUS_1] = 
		TERMINATE_ACTIVE_SWITCH_TO_HELD;
	pvt->call_matrix[CALLHOLD_3WAY_CALL][FLASH_KEY_PLUS_2] = SWITCH_3WAY_CALL;

	if (pvt->transfermode != TRANSFER_OFF)
	{
		pvt->call_matrix[CALLHOLD_ACTIVECALL][FLASH_KEY_PLUS_4] =
			pvt->call_matrix[CALLHOLD_3WAY_CALL][FLASH_KEY_PLUS_4] =
			TRANSFER_CALL;
	}

	if (pvt->threewayconference)
	{
		pvt->call_matrix[CALLHOLD_ACTIVECALL][FLASH_KEY_PLUS_3] = 
			START_CONFERENCE; 
		pvt->call_matrix[CALLHOLD_3WAY_CALL][FLASH_KEY_PLUS_3] = 
			START_CONFERENCE;
		pvt->call_matrix[CONFERENCE][FLASH_KEY] =
			DROP_LAST_PARTY_FROM_CONF;
	}
}

void configure_call_matrix_de(struct jdsp_pvt *pvt)
{
	if (pvt->threewaycalling)
		pvt->call_matrix[ONE_ACTIVE_CALL][FLASH_KEY] = PUT_ON_HOLD;
	else
	{
		pvt->call_matrix[HOOKED_OFF_DIALTONE][FLASH_KEY] =
			pvt->call_matrix[LINE_RING_BACK][FLASH_KEY] = 
			pvt->call_matrix[LINE_BUSY_TONE][FLASH_KEY] =
			pvt->call_matrix[LINE_FBUSY_TONE][FLASH_KEY] =
			pvt->call_matrix[ONE_ACTIVE_CALL][FLASH_KEY] =
			ENQUEUE_FLASH_EVENT;
	}

	pvt->call_matrix[CALLWAITING][FLASH_KEY_PLUS_0] = REJECT_INCOMING_CALL;
	pvt->call_matrix[CALLWAITING][FLASH_KEY_PLUS_1] = DROP_AND_SWITCH;
	pvt->call_matrix[CALLWAITING][FLASH_KEY] = 
		pvt->call_matrix[CALLWAITING][FLASH_KEY_PLUS_2] = SWITCH_CALL;
	pvt->call_matrix[CALLWAITING_HANGUP][FLASH_KEY] =
		pvt->call_matrix[CALLWAITING_HANGUP][FLASH_KEY_PLUS_2] =
		SWITCH_HG_CALLWAITNG_CALL;
	pvt->call_matrix[CALLHOLD_ACTIVECALL][FLASH_KEY] =
		pvt->call_matrix[CALLHOLD_ACTIVECALL][FLASH_KEY_PLUS_2] = SWITCH_CALL;
	pvt->call_matrix[CALLHOLD_ACTIVECALL][FLASH_KEY_PLUS_0] =
		TERMINATE_HELD_CALL;
	pvt->call_matrix[CALLHOLD_ACTIVECALL][FLASH_KEY_PLUS_1] = DROP_AND_SWITCH;
	pvt->call_matrix[CALLHOLD_DIALTONE][FLASH_KEY] =
		pvt->call_matrix[CALLHOLD_DIALTONE][FLASH_KEY_PLUS_2] = 
		pvt->call_matrix[CALLHOLD_FBUSYTONE][FLASH_KEY] = 
		pvt->call_matrix[CALLHOLD_FBUSYTONE][FLASH_KEY_PLUS_0] =
		pvt->call_matrix[CALLHOLD_FBUSYTONE][FLASH_KEY_PLUS_1] =
		pvt->call_matrix[CALLHOLD_FBUSYTONE][FLASH_KEY_PLUS_2] =
		pvt->call_matrix[CALLHOLD_FBUSYTONE][FLASH_KEY_PLUS_3] =
		pvt->call_matrix[CALLHOLD_BUSYTONE][FLASH_KEY] = 
		pvt->call_matrix[CALLHOLD_BUSYTONE][FLASH_KEY_PLUS_0] =
		pvt->call_matrix[CALLHOLD_BUSYTONE][FLASH_KEY_PLUS_1] =
		pvt->call_matrix[CALLHOLD_BUSYTONE][FLASH_KEY_PLUS_2] =
		pvt->call_matrix[CALLHOLD_BUSYTONE][FLASH_KEY_PLUS_3] =
		STOP_TONE_AND_SWITCH;
	pvt->call_matrix[CALLHOLD_3WAY_CALL][FLASH_KEY] = SWITCH_3WAY_CALL;
	pvt->call_matrix[CALLHOLD_3WAY_CALL][FLASH_KEY_PLUS_0] = 
		TERMINATE_3WAYHELD_CALL;
	pvt->call_matrix[CALLHOLD_3WAY_CALL][FLASH_KEY_PLUS_1] = 
		TERMINATE_ACTIVE_SWITCH_TO_HELD;
	pvt->call_matrix[CALLHOLD_3WAY_CALL][FLASH_KEY_PLUS_2] = SWITCH_3WAY_CALL;
	pvt->call_matrix[CALLHOLD_RINGBACK][FLASH_KEY_PLUS_1] =
		STOP_TONE_AND_SWITCH;
	if (pvt->threewayconference)
	{
		pvt->call_matrix[CALLHOLD_3WAY_CALL][FLASH_KEY_PLUS_3] =
			pvt->call_matrix[CALLHOLD_ACTIVECALL][FLASH_KEY_PLUS_3] =
			START_CONFERENCE;
		pvt->call_matrix[CONFERENCE][FLASH_KEY] =
			pvt->call_matrix[CONFERENCE][FLASH_KEY_PLUS_0] =
			HOLD_LAST_PARTY_FROM_CONF;
		pvt->call_matrix[CONFERENCE][FLASH_KEY_PLUS_1] =
			pvt->call_matrix[CONFERENCE][FLASH_KEY_PLUS_2] =
			pvt->call_matrix[CONFERENCE][FLASH_KEY_PLUS_3] =
			HOLD_FIRST_PARTY_FROM_CONF;
	}
	/* configure conference "hangup" behaviour */
	conference_hangup_drop_all = 1;
	/* don't switch to held call after fast busy */
	return_to_held_call = 0;
}

void configure_call_matrix_nz(struct jdsp_pvt *pvt)
{
    	if (pvt->threewaycalling)
	{
		pvt->call_matrix[HOOKED_OFF_DIALTONE][FLASH_KEY] = RESTART_DIALTONE;
		pvt->call_matrix[LINE_BUSY_TONE][FLASH_KEY] =
			pvt->call_matrix[LINE_FBUSY_TONE][FLASH_KEY] = 
			DROP_AND_RESTART_DIALTONE;
		pvt->call_matrix[ONE_ACTIVE_CALL][FLASH_KEY] = PUT_ON_HOLD;
	}
	else
	{
		pvt->call_matrix[HOOKED_OFF_DIALTONE][FLASH_KEY] =
			pvt->call_matrix[LINE_BUSY_TONE][FLASH_KEY] =
			pvt->call_matrix[LINE_FBUSY_TONE][FLASH_KEY] =
			pvt->call_matrix[ONE_ACTIVE_CALL][FLASH_KEY] = 
			ENQUEUE_FLASH_EVENT;
	}

	pvt->call_matrix[CALLWAITING][FLASH_KEY] = SWITCH_CALL;
	pvt->call_matrix[CALLWAITING_HANGUP][FLASH_KEY] = 
		SWITCH_HG_CALLWAITNG_CALL;
	pvt->call_matrix[CALLHOLD_ACTIVECALL][FLASH_KEY] = SWITCH_CALL;
	pvt->call_matrix[CALLHOLD_DIALTONE][FLASH_KEY] = 
		pvt->call_matrix[CALLHOLD_FBUSYTONE][FLASH_KEY] = 
		pvt->call_matrix[CALLHOLD_RINGBACK][FLASH_KEY] = 
		pvt->call_matrix[CALLHOLD_BUSYTONE][FLASH_KEY] = 
		STOP_TONE_AND_SWITCH;

	if (pvt->transfermode != TRANSFER_OFF)
	{
		pvt->call_matrix[CALLHOLD_RINGBACK][FLASH_KEY_PLUS_4] = 
			pvt->call_matrix[CALLHOLD_3WAY_CALL][FLASH_KEY_PLUS_4] =
			TRANSFER_CALL;
	}

	if (pvt->threewayconference)
	{
		pvt->call_matrix[CALLHOLD_RINGBACK][FLASH_KEY_PLUS_3] = 
			START_CONFERENCE; 
		pvt->call_matrix[CALLHOLD_3WAY_CALL][FLASH_KEY_PLUS_3] = 
			START_CONFERENCE;
		pvt->call_matrix[CALLHOLD_3WAY_CALL][FLASH_KEY_PLUS_1] =
			SWITCH_3WAY_CALL;
		pvt->call_matrix[CONFERENCE][FLASH_KEY] = 
			DROP_LAST_PARTY_FROM_CONF;
	}
}

void configure_call_matrix_it(struct jdsp_pvt *pvt)
{
	if (pvt->threewaycalling)
	{
		pvt->call_matrix[HOOKED_OFF_DIALTONE][FLASH_KEY] = RESTART_DIALTONE;
		pvt->call_matrix[LINE_BUSY_TONE][FLASH_KEY] =
			pvt->call_matrix[LINE_FBUSY_TONE][FLASH_KEY] =
                        pvt->call_matrix[LINE_RING_BACK][FLASH_KEY] =
			DROP_AND_RESTART_DIALTONE;
		pvt->call_matrix[ONE_ACTIVE_CALL][FLASH_KEY] = PUT_ON_HOLD;
	}
	else
	{
		pvt->call_matrix[HOOKED_OFF_DIALTONE][FLASH_KEY] =
			pvt->call_matrix[LINE_BUSY_TONE][FLASH_KEY] =
			pvt->call_matrix[LINE_FBUSY_TONE][FLASH_KEY] =
			pvt->call_matrix[ONE_ACTIVE_CALL][FLASH_KEY] = 
			ENQUEUE_FLASH_EVENT;
	}

	pvt->call_matrix[CALLWAITING][FLASH_KEY_PLUS_2] = SWITCH_CALL;
	pvt->call_matrix[CALLWAITING_HANGUP][FLASH_KEY_PLUS_2] = 
		SWITCH_HG_CALLWAITNG_CALL;
	pvt->call_matrix[CALLHOLD_ACTIVECALL][FLASH_KEY] = SWITCH_CALL;
	pvt->call_matrix[CALLHOLD_DIALTONE][FLASH_KEY] = 
		pvt->call_matrix[CALLHOLD_FBUSYTONE][FLASH_KEY] = 
		pvt->call_matrix[CALLHOLD_RINGBACK][FLASH_KEY] = 
		pvt->call_matrix[CALLHOLD_BUSYTONE][FLASH_KEY] = 
		STOP_TONE_AND_SWITCH;

	if (pvt->threewayconference)
	{
		pvt->call_matrix[CALLHOLD_3WAY_CALL][FLASH_KEY_PLUS_2] =
			SWITCH_3WAY_CALL;
		pvt->call_matrix[CALLHOLD_RINGBACK][FLASH_KEY_PLUS_3] = 
			START_CONFERENCE; 
		pvt->call_matrix[CALLHOLD_3WAY_CALL][FLASH_KEY_PLUS_3] = 
			START_CONFERENCE;
		pvt->call_matrix[CONFERENCE][FLASH_KEY] = 
			DROP_LAST_PARTY_FROM_CONF;
	}

	/* configure conference "hangup" behaviour */
	conference_hangup_drop_all = 1;
}

static int jdsp_on_flash_key_timeout(void* data)
{
	struct jdsp_pvt *p = (struct jdsp_pvt *)data;
	struct ast_channel *ast = p->pending_flash_key_channel;
	
	if (ast == NULL)
		goto Exit; 

	jdsp_handle_flash_key(ast, p, FLASH_KEY, jdsp_get_index(ast, p, 0));

Exit:
    p->pending_flash_key = 0;
	p->pending_flash_key_channel = NULL;
	return 0;
}

static flash_key_seq_t prepare_flash_event(phone_event_t* ev)
{
    switch(ev->key) 
    {
	case PHONE_KEY_0:	return FLASH_KEY_PLUS_0;
	case PHONE_KEY_1:	return FLASH_KEY_PLUS_1;
	case PHONE_KEY_2:	return FLASH_KEY_PLUS_2;
	case PHONE_KEY_3:	return FLASH_KEY_PLUS_3;
	case PHONE_KEY_4:	return FLASH_KEY_PLUS_4;
	case PHONE_KEY_5:	return FLASH_KEY_PLUS_5;
	case PHONE_KEY_6:	return FLASH_KEY_PLUS_6;
	case PHONE_KEY_7:	return FLASH_KEY_PLUS_7;
	case PHONE_KEY_8:	return FLASH_KEY_PLUS_8;
	case PHONE_KEY_9:	return FLASH_KEY_PLUS_9;
	case PHONE_KEY_A:	return FLASH_KEY_PLUS_A;
	case PHONE_KEY_B:	return FLASH_KEY_PLUS_B;
	case PHONE_KEY_C:	return FLASH_KEY_PLUS_C;
	case PHONE_KEY_D:	return FLASH_KEY_PLUS_D;
	case PHONE_KEY_ASTERISK:return FLASH_KEY_PLUS_ASTERISK;
	case PHONE_KEY_POUND:	return FLASH_KEY_PLUS_POUND;
	default: return FLASH_KEY_NONE;
    }
}

static struct ast_frame *jdsp_handle_event(struct ast_channel *ast)
{
	int index;
	struct jdsp_pvt *p = ast->tech_pvt;
	phone_event_t ev;
	char dtmf_key;
	int res;
	int fd = ast->fds[ast->fdno];
	int timeout = faxmodem_local_detect_timeout; 

	index = jdsp_get_index(ast, p, 0);
	p->subs[index].f.frametype = AST_FRAME_NULL;
	p->subs[index].f.datalen = 0;
	p->subs[index].f.samples = 0;
	p->subs[index].f.mallocd = 0;
	p->subs[index].f.offset = 0;
	p->subs[index].f.src = __FUNCTION__;
	p->subs[index].f.data = NULL;
	if (index < 0)
		return &p->subs[index].f;
	if (jdsp_get_event(fd, &ev) < 0)
	{
	    ast_log(LOG_WARNING, "No event\n");
	    return NULL;
	}

	ast_log(LOG_DEBUG, "Got event \"%s\" on channel %d (index %d)\n", jdsp_event2str(&ev), p->channel, index);
	
	switch(ev.key) {
	case PHONE_KEY_0:
	case PHONE_KEY_1:
	case PHONE_KEY_2:
	case PHONE_KEY_3:
	case PHONE_KEY_4:
	case PHONE_KEY_5:
	case PHONE_KEY_6:
	case PHONE_KEY_7:
	case PHONE_KEY_8:
	case PHONE_KEY_9:
	case PHONE_KEY_A:
	case PHONE_KEY_B:
	case PHONE_KEY_C:
	case PHONE_KEY_D:
	case PHONE_KEY_ASTERISK:
	case PHONE_KEY_POUND:
	    if (index == SUB_REAL && p->pending_flash_key)
	    {
		if (!ev.pressed)
		{
		    jdsp_handle_flash_key(ast, p, prepare_flash_event(&ev), index);
    		    ast_sched_del(sched, p->pending_flash_key);
		    p->pending_flash_key = 0;
		    p->pending_flash_key_channel = NULL;
		}	        
		break;  
	    }
	    dtmf_key = jdsp_key2char(ev.key);
	    if (ev.pressed)
	    {
		    ast_log(LOG_MFT_INFO, "phone %d: DTMF Down '%c'\n", p->channel, logger_hide_number(dtmf_key));
		    p->subs[index].f.frametype = AST_FRAME_DTMF_BEGIN;
		    p->subs[index].f.subclass = dtmf_key;
		    return &p->subs[index].f;
	    }
	    else
	    {
		    ast_log(LOG_MFT_INFO, "phone %d: DTMF Up '%c'\n", p->channel, logger_hide_number(dtmf_key));
		    p->subs[index].f.frametype = AST_FRAME_DTMF_END;
		    p->subs[index].f.subclass = dtmf_key;

		    if (call_key_seq_process(p, dtmf_key))
		    {
			ast_write(ast_bridged_channel(ast), &p->subs[index].f);
			incall_announcement_start(&p->owner, &p->lock,
			    jdsp_modify_audio, SUB_REAL, sched);
			p->subs[index].f.frametype = AST_FRAME_NULL;
		    }

		    return &p->subs[index].f;
	    }
	case PHONE_KEY_FAX_DIS:
	case PHONE_KEY_FAX_CNG:
		/* We allow changing from modem to fax, since answer tone may be 
		 * raised as modem event (as in BCM9636x), however later tones in a fax
		 * call are raised as fax. */
		if (p->faxdetected || p->faxtxmethod == FAX_NONE ||
			p->faxtxmethod == FAX_PASSTHROUGH_FORCE) {
			ast_log(LOG_DEBUG, "Ignore event; faxdetected %d; "
				"faxmethod %d\n", p->faxdetected, p->faxtxmethod);
		    break;
		}


		/* if has modem detection timer, cancel it*/
		if (p->modem_timer_sched_id > -1)
		{
			ast_log(LOG_DEBUG, "Cancel modem detect timer\n");
			ast_sched_del(sched, p->modem_timer_sched_id);
			p->modem_timer_sched_id = -1;
		}
		
		if (p->detectionmethod == FAX_DETECTION_NONE || 
			(ev.key == PHONE_KEY_FAX_CNG && 
			p->detectionmethod == FAX_DETECTION_TERMINATING) ||
			(ev.key == PHONE_KEY_FAX_DIS && 
			p->detectionmethod == FAX_DETECTION_ORIGINATING))
		{
			ast_log(LOG_DEBUG, "Ignoring event. Detection method = %d\n",
				p->detectionmethod);
			break;
		}

		p->modemdetected = 0;
		p->faxdetected = 1;
		
		jdsp_modify_audio(ast, SUB_REAL);
		jdsp_queue_control(p->subs[SUB_REAL].owner, AST_CONTROL_FAX);
		break;
	case PHONE_KEY_FAXMODEM_NET_CED:
		timeout = faxmodem_net_detect_timeout;
		/* Break wasn't inserted intentionally */
	case PHONE_KEY_FAXMODEM_CED:
		/* Note: If we detected fax, we ignore further modem detections.
		 * if changing this policy, just change the following condition
		 * (don't break on faxdetected) */
		if (p->faxdetected || p->modemdetected)
		{
			ast_log(LOG_DEBUG, "Ignore event; faxdetected %d; "
				"modemdetected %d\n", p->faxdetected, p->modemdetected);
		    break;
		}

		if (p->detectionmethod == FAX_DETECTION_NONE || 
			(ev.key == PHONE_KEY_FAXMODEM_NET_CED && 
			p->detectionmethod == FAX_DETECTION_TERMINATING) ||
			(ev.key == PHONE_KEY_FAXMODEM_CED && 
			p->detectionmethod == FAX_DETECTION_ORIGINATING))
		{
			ast_log(LOG_DEBUG, "Ignoring event. Detection method = %d\n",
				p->detectionmethod);
			break;
		}

		/* start delay of faxmodem_local_detect_timeout or faxmodem_net_detect_timeout 
		 * milisec before indicating Modem call. if we'll receive
		 * Net_CNG during these miliseconds then this is a Fax call and not modem. */
		p->modemdetected = 1;
		ast_log(LOG_DEBUG, "Modem detected. Waiting %d milisec...\n", timeout);
		if(!timeout)
			jdsp_fax_detect_timeout(p);
		else
			p->modem_timer_sched_id = ast_sched_add(sched, timeout,
			    jdsp_fax_detect_timeout, p);
		break;
	case PHONE_KEY_MODEM_CNG:
		if (p->modemdetected) 
		    break;

		/* Stop active modem detection timer if exists*/
		if (p->modem_timer_sched_id > -1)
		{
			ast_log(LOG_DEBUG, "Cancel modem detect timer\n");
			ast_sched_del(sched, p->modem_timer_sched_id);
			p->modem_timer_sched_id = -1;
		}

		p->modemdetected = 1;
		p->faxdetected = 0;

 		jdsp_modify_audio(ast, SUB_REAL);
		jdsp_queue_control(p->subs[SUB_REAL].owner, AST_CONTROL_MODEM);
		break;
	case PHONE_KEY_HOOK_ON:
		ast_log(LOG_MFT_INFO, "phone %d: Hook on\n", p->channel);
		manager_event(EVENT_FLAG_CALL, "Onhook",
		    "Channel: jdsp/%d-1\r\n", p->channel);
		if(ast)
		    ast_clear_flag(ast, AST_FLAG_OFFHOOK);
		p->onhooktime = time(NULL);
		/* Check for some special conditions regarding call waiting */
		if (index == SUB_REAL) {
			jdsp_play_tone(p, PHONE_TONE_NONE);
			/* The normal line was hung up */
			if (p->subs[SUB_CALLWAIT].owner) {
				/* There's a call waiting call, so ring the phone, but make it unowned in the mean time */
				swap_subs(p, SUB_CALLWAIT, SUB_REAL);
				if (option_verbose > 2) 
					ast_verbose(VERBOSE_PREFIX_3 "Channel %d still has (callwait) call, ringing phone\n", p->channel);
				unalloc_sub(p, SUB_CALLWAIT);	
				jdsp_callwait(p, 0);
				p->owner = NULL;
				jdsp_ring(p, 1);
			} else if (p->subs[SUB_THREEWAY].owner) {
				unsigned int mssinceflash;
				/* Here we have to retain the lock on both the main channel, the 3-way channel, and
				   the private structure -- not especially easy or clean */
				while(p->subs[SUB_THREEWAY].owner && ast_mutex_trylock(&p->subs[SUB_THREEWAY].owner->lock)) {
					/* Yuck, didn't get the lock on the 3-way, gotta release everything and re-grab! */
					ast_mutex_unlock(&p->lock);
					ast_mutex_unlock(&ast->lock);
					usleep(1);
					/* We can grab ast and p in that order, without worry.  We should make sure
					   nothing seriously bad has happened though like some sort of bizarre double
					   masquerade! */
					ast_mutex_lock(&ast->lock);
					ast_mutex_lock(&p->lock);
					if (p->owner != ast) {
						ast_log(LOG_WARNING, "This isn't good...\n");
						return NULL;
					}
				}
				if (!p->subs[SUB_THREEWAY].owner) {
					ast_log(LOG_NOTICE, "Whoa, threeway disappeared kinda randomly.\n");
					return NULL;
				}
				mssinceflash = ast_tvdiff_ms(ast_tvfromboot(), p->flashtime);
				ast_log(LOG_DEBUG, "Last flash was %d ms ago\n", mssinceflash);
				if (mssinceflash < MIN_MS_SINCE_FLASH) {
					/* It hasn't been long enough since the last flashook.  This is probably a bounce on 
					   hanging up.  Hangup both channels now */
					ast_log(LOG_DEBUG, "Looks like a bounced flash, hanging up both calls on %d\n", p->channel);
					p->subs[SUB_THREEWAY].owner->_softhangup |= AST_SOFTHANGUP_DEV;
					if (p->subs[SUB_THREEWAY].owner)
					    ast_mutex_unlock(&p->subs[SUB_THREEWAY].owner->lock);
				}

				else if ((ast->_state == AST_STATE_UP)
#if defined(CONFIG_RG_VODAFONE_IT) || defined(CONFIG_RG_VODAFONE_DE)
					|| (ast->_state == AST_STATE_RINGING)
#endif
					)
				{
					if (jdsp_get_call_matrix_state(ast) == CONFERENCE &&
						conference_hangup_drop_all)
					{
						p->subs[SUB_THREEWAY].owner->_softhangup |=
							AST_SOFTHANGUP_DEV;
						ast_mutex_unlock(&p->subs[SUB_THREEWAY].owner->lock);
						return NULL;
					}
						if (p->transfermode != TRANSFER_OFF)
						{
							if (!start_transfer_call(ast))
								return &p->subs[index].f;
#if defined(CONFIG_RG_VODAFONE_DE) || defined(CONFIG_RG_VODAFONE_IT)
							if(p->transfermode == TRANSFER_BRIDGING)
							{
								ast_mutex_unlock(&p->subs[SUB_THREEWAY].owner->lock);
								/* Swap subs and dis-own channel */
								swap_subs(p, SUB_THREEWAY, SUB_REAL);
								p->owner = NULL;
								/* Ring the phone */
								jdsp_ring(p, 1);
							}
#endif
						}
						else 
						{
							p->subs[SUB_THREEWAY].owner->_softhangup |= AST_SOFTHANGUP_DEV;
							if (p->subs[SUB_THREEWAY].owner)
								ast_mutex_unlock(&p->subs[SUB_THREEWAY].owner->lock);
						}
				} 
				else 
				{
					ast_mutex_unlock(&p->subs[SUB_THREEWAY].owner->lock);
					/* Swap subs and dis-own channel */
					swap_subs(p, SUB_THREEWAY, SUB_REAL);
					p->owner = NULL;
					/* Ring the phone */
					jdsp_ring(p, 1);
				}
			}
		} else {
			jdsp_dc_free(p, p->real_dc_num);
			ast_log(LOG_WARNING, "Got a hangup and my index is %d?\n", index);
		}
		
		return NULL;
		break;

	case PHONE_KEY_HOOK_OFF:
		manager_event(EVENT_FLAG_CALL, "Offhook",
		    "Channel: jdsp/%d-1\r\n", p->channel);
		switch(ast->_state) {
		case AST_STATE_RINGING:
			/* At this point we already have shared data channel.
			 * We should do exclusive allocation now. */
			if (jdsp_dc_exclusive_alloc(p, 0, 0))
			{
				/* Failed, the call is answered by someone else */

				/* free the shared data channel */
				jdsp_dc_free(p, 0); 

				/* allocate lame DC for reorder tone generation */
				jdsp_dc_exclusive_alloc(p, 0, 1); 

				return &p->subs[index].f;
			}
			p->subs[index].f.frametype = AST_FRAME_CONTROL;
			p->subs[index].f.subclass = AST_CONTROL_ANSWER;
			/* Make sure it stops ringing */
			jdsp_ring(p, 0);
			ast_log(LOG_DEBUG, "channel %d answered\n", p->channel);
			if (p->confirmanswer) {
				/* Ignore answer if "confirm answer" is enabled */
				p->subs[index].f.frametype = AST_FRAME_NULL;
				p->subs[index].f.subclass = 0;
			} else {
				ast_setstate(ast, AST_STATE_UP);
			}
			return &p->subs[index].f;
		case AST_STATE_DOWN:
			ast_setstate(ast, AST_STATE_RING);
			ast->rings = 1;
			p->subs[index].f.frametype = AST_FRAME_CONTROL;
			p->subs[index].f.subclass = AST_CONTROL_OFFHOOK;
			ast_log(LOG_DEBUG, "channel %d picked up\n", p->channel);
			return &p->subs[index].f;
		case AST_STATE_UP:
			/* Make sure it stops ringing */
			jdsp_ring(p, 0);
			/* Okay -- probably call waiting*/

			if (ast_bridged_channel(p->owner))
			{
				jdsp_moh_stop(p, index);
				jdsp_start_audio(p->subs[index].owner, index); /* XXX Need to check whether this line is needed */
			}

			break;
		case AST_STATE_RESERVED:
			/* Start up dialtone */
			res = jdsp_play_tone(p, get_dialtone(p));
			break;
		default:
			ast_log(LOG_WARNING, "FXO phone off hook in weird state %d??\n", ast->_state);
		}
		break;
	case PHONE_KEY_FLASH:
		if (index == SUB_REAL) 
		    jdsp_handle_flash_key(ast, p, FLASH_KEY, index);
		else
		    ast_log(LOG_WARNING, "Got flash hook with index %d on channel %d?!?\n", index, p->channel);
		break;
	default:
		ast_log(LOG_DEBUG, "Dunno what to do with event \"%s\" on channel %d\n", jdsp_event2str(&ev), p->channel);
	}
	return &p->subs[index].f;
}

static struct ast_frame *__jdsp_read(struct ast_channel *ast)
{
	struct jdsp_pvt *p = ast->tech_pvt;
	int res;
	int index;
	unsigned char *readbuf;
	struct ast_frame *f;
	phone_event_t ev;
	int fd = ast->fds[ast->fdno];

	index = jdsp_get_index(ast, p, 1);

	/* Hang up if we don't really exist */
	if (index < 0)
	{
		ast_log(LOG_WARNING, "We dont exist?\n");
		return NULL;
	}
	
	p->subs[index].f.frametype = AST_FRAME_NULL;
	p->subs[index].f.datalen = 0;
	p->subs[index].f.samples = 0;
	p->subs[index].f.mallocd = 0;
	p->subs[index].f.offset = 0;
	p->subs[index].f.subclass = 0;
	p->subs[index].f.delivery = ast_tv(0,0);
	p->subs[index].f.src = __FUNCTION__;
	p->subs[index].f.data = NULL;

	/* If we got a voice packet */
	if (fd == p->subs[index].dfd)
	{
		int timestamp;

		readbuf = p->subs[index].buffer + AST_FRIENDLY_OFFSET;
		CHECK_BLOCKING(ast);
		res = read(fd, readbuf, READ_SIZE);
		/* 8000 timestamp units = 1sec */
		timestamp = ((unsigned long *)readbuf)[1];
		p->subs[index].f.delivery = ast_samp2tv(ntohl(timestamp), 8000);
		ast_clear_flag(ast, AST_FLAG_BLOCKING);
		if (res < 0)
			return NULL;

		p->subs[index].f.mallocd = 0;
		if (p->faxtxmethod == FAX_T38_AUTO && p->t38enabled)
		{
		    p->subs[index].f.datalen = res;
		    p->subs[index].f.frametype = AST_FRAME_MODEM;
		    p->subs[index].f.subclass = AST_MODEM_T38;
		    p->subs[index].f.offset = AST_FRIENDLY_OFFSET;
		    p->subs[index].f.data = readbuf;
		    p->subs[index].f.samples =
			ast_codec_get_samples(&p->subs[index].f);
		}
		else
		{
		    jdsp_codec_rtp2ast(readbuf, &p->subs[index].f, res,
			readbuf);
		}

		return &p->subs[index].f;
	}

	if (!p->owner)
	{
		/* If nobody owns us, absorb the event appropriately, otherwise
		   we loop indefinitely.  This occurs when, during call waiting, the
		   other end hangs up our channel so that it no longer exists, but we
		   have neither FLASH'd nor ONHOOK'd to signify our desire to
		   change to the other channel. */
		if (jdsp_get_event(fd, &ev) < 0)
		{
			ast_log(LOG_WARNING, "No event\n");
			return NULL;
		}
		
		/* Switch to real if it is a PHONE_KEY_HOOK_OFF, a PHONE_KEY_HOOK_ON or a PHONE_KEY_FLASH event */
		if (ev.key == PHONE_KEY_HOOK_OFF || ev.key == PHONE_KEY_FLASH ||
		    ev.key == PHONE_KEY_HOOK_ON)
		{
			ast_log(LOG_DEBUG, "Restoring owner of channel %d on event \"%s\"\n", p->channel, jdsp_event2str(&ev));
			p->owner = p->subs[SUB_REAL].owner;
		}
		switch(ev.key) 
		{
		case PHONE_KEY_HOOK_ON:
			jdsp_play_tone(p, PHONE_TONE_NONE);
			if (p->owner) 
			{
				if (option_verbose > 2) 
					ast_verbose(VERBOSE_PREFIX_3 "Channel %s still has call, ringing phone\n", p->owner->name);
				jdsp_callwait(p, 0);
				jdsp_ring(p, 1);
				ast_setstate(p->owner, AST_STATE_RINGING);
				/* we're hooked on so pvt shouldn't have an owner channel. on
				 * next hook off, the p->owner will be restored. */
				p->owner = NULL;
			} 
			else 
			{
				ast_log(LOG_WARNING, "Absorbed on hook, but nobody is left!?!?\n");
				jdsp_dc_free(p, p->real_dc_num);
			}
			break;
		case PHONE_KEY_HOOK_OFF:
			if (p->owner && ast_bridged_channel(p->owner))
			{
				jdsp_moh_stop(p, index);
				jdsp_start_audio(p->subs[index].owner, index); 
			}
			if (p->owner && (p->owner->_state == AST_STATE_RINGING)) 
			{
				jdsp_ring(p, 0);
				ast_setstate(p->owner, AST_STATE_UP);
				p->subs[index].f.frametype = AST_FRAME_CONTROL;
				p->subs[index].f.subclass = AST_CONTROL_ANSWER;
				ast_clear_flag(p->subs[SUB_REAL].owner, AST_FLAG_CALLWAIT);
				ast_clear_flag(p->subs[SUB_REAL].owner, AST_FLAG_CALL_ONHOLD);
			}
			break;
		case PHONE_KEY_FLASH:
			p->flashtime = ast_tvfromboot();
			if (p->owner) 
			{
				if (option_verbose > 2) 
					ast_verbose(VERBOSE_PREFIX_3 "Channel %d flashed to other channel %s\n", p->channel, p->owner->name);
					
				jdsp_handle_flash_key(ast, p, FLASH_KEY, SUB_REAL);
			} 
			else
				ast_log(LOG_WARNING, "Absorbed on hook, but nobody is left!?!?\n");
			break;
		default:
			ast_log(LOG_WARNING, "Don't know how to absorb event %s\n", jdsp_event2str(&ev));
		}
		f = &p->subs[index].f;
		return f;
	}

	ast_log(LOG_DEBUG, "Read on %d, channel %d\n", fd, p->channel);
	/* If it's not us, return NULL immediately */
	if (ast != p->owner) 
	{
		ast_log(LOG_WARNING, "We're %s, not %s\n", ast->name, p->owner->name);
		f = &p->subs[index].f;
		return f;
	}
	f = jdsp_handle_event(ast);
	return f;
}

struct ast_frame *jdsp_read(struct ast_channel *ast)
{
	struct jdsp_pvt *p = ast->tech_pvt;
	struct ast_frame *f;
	
	for (;;) {
		if (ast_mutex_trylock(&p->lock)) {
			ast_mutex_unlock(&ast->lock);
			usleep(1);
			ast_mutex_lock(&ast->lock);
		} else {
			f = __jdsp_read(ast);
			ast_mutex_unlock(&p->lock);
			break;
		}
	}
	return f;
}

#if 0
static unsigned int calc_txstamp(struct jdsp_subchannel *s, struct timeval *delivery)
{
	struct timeval t;
	long ms;
	if (ast_tvzero(s->txcore)) {
		s->txcore = ast_tvfromboot();
		/* Round to 20ms for nice, pretty timestamps */
		s->txcore.tv_usec -= s->txcore.tv_usec % 20000;
	}
	/* Use previous txcore if available */
	t = (delivery && !ast_tvzero(*delivery)) ? *delivery : ast_tvfromboot();
	ms = ast_tvdiff_ms(t, s->txcore);
	if (ms < 0)
		ms = 0;
	/* Use what we just got for next time */
	s->txcore = t;
	return (unsigned int) ms;
}
#endif

static int jdsp_write_voice_frame(struct jdsp_pvt *p, int index, struct ast_frame *frame)
{
	int fd;
	unsigned int *rtpheader;
	unsigned long ts;
	int payload_type = frame->frametype == AST_FRAME_VOICE ? 
	    jdsp_codec_ast2rtp(frame->subclass) : JRTP_PAYLOAD_CN;

	fd = p->subs[index].dfd;
	if (fd == -1)
		return 0;

	rtpheader = (unsigned int *)((char *)frame->data - RTP_HEADER_SIZE);
	rtpheader[0] = htonl((2 << 30) | (0 << 23) | (payload_type << 16) |\
		(unsigned short)(p->subs[index].seqno + 1));
	if (ast_tvzero(frame->delivery))
	{
		/* If the frame didn't come with its own timestamp, make one up */
		ts = p->subs[index].lastts + ast_codec_get_samples(frame);
	}
	else
		ts = frame->delivery.tv_sec*8000 + frame->delivery.tv_usec/125;
	rtpheader[1] = htonl(ts);
	rtpheader[2] = 0; /* The DSP don't care about the ssrc */
	if (write(fd, rtpheader, frame->datalen + RTP_HEADER_SIZE) > 0)
	{
		p->subs[index].lastts = ts;
		p->subs[index].seqno++;
	}

	return frame->datalen;
}

static int jdsp_write_modem_frame(struct jdsp_pvt *p, int index, struct ast_frame *frame)
{
	int fd;
	unsigned int *udptlheader;

	fd = p->subs[index].dfd;
	if (fd == -1)
		return 0;

	udptlheader = (unsigned int *)((char *)frame->data);
	write(fd, udptlheader, frame->datalen);
	
	return frame->datalen;
}

static int jdsp_write(struct ast_channel *ast, struct ast_frame *frame)
{
	struct jdsp_pvt *p = ast->tech_pvt;
	int res = 0;
	int index;

	if (frame->frametype != AST_FRAME_CALLWAITING &&
	    frame->frametype != AST_FRAME_VOICE &&
	    frame->frametype != AST_FRAME_MODEM &&
	    frame->frametype != AST_FRAME_CNG)
	{
		return 0;
	}

	if (frame->frametype == AST_FRAME_CALLWAITING)
	{
		ast_mutex_lock(&p->lock);
		if (frame->subclass == AST_CALLWAITING_STOP)
			jdsp_callwait(p, 0);
		else
		{
			char *cid = frame->data;
			char *name = NULL, *num = NULL;

			if (cid)
				ast_callerid_parse(cid, &name, &num);
			if (num)
				ast_copy_string(p->callwait_num, num, sizeof(p->callwait_num));
			if (name)
				ast_copy_string(p->callwait_name, name, sizeof(p->callwait_name));
			jdsp_callwait(p, 1);
		}
		ast_mutex_unlock(&p->lock);
		return 0;
	}
	
	index = jdsp_get_index(ast, p, 0);
	if (index < 0) {
		ast_log(LOG_WARNING, "%s doesn't really exist?\n", ast->name);
		return -1;
	}

	/* Write a frame of (presumably voice) data */
	if (frame->frametype != AST_FRAME_VOICE &&
	    frame->frametype != AST_FRAME_MODEM &&
	    frame->frametype != AST_FRAME_CNG)
	{
		if (frame->frametype != AST_FRAME_IMAGE)
			ast_log(LOG_WARNING, "Don't know what to do with frame type '%d'\n", frame->frametype);
		return 0;
	}
	/* Workaround for FAX/T.38, which happens to correspond to voice format G.723. */
	if ((frame->frametype == AST_FRAME_VOICE && !(frame->subclass & ast->nativeformats.audio_bits)) ||
	    (frame->frametype == AST_FRAME_MODEM && frame->subclass == AST_MODEM_T38 &&
  	             p->faxtxmethod != FAX_T38_AUTO)) {
		ast_log(LOG_WARNING, "Cannot handle frames in %d format\n", frame->subclass);
		return -1;
	}
	if (!p->owner) {
		if (option_debug)
			ast_log(LOG_DEBUG, "Dropping frame since there is no active owner on %s...\n",ast->name);
		return 0;
	}
	/* Return if it's not valid data */
	if (!frame->data || !frame->datalen)
		return 0;

	if (frame->subclass != AST_FORMAT_SLINEAR) {
		res = (frame->frametype == AST_FRAME_VOICE ||
		    frame->frametype == AST_FRAME_CNG) ?
		    jdsp_write_voice_frame(p, index, frame) :
		    jdsp_write_modem_frame(p, index, frame);
	}
	if (res < 0) {
		ast_log(LOG_WARNING, "write failed: %s\n", strerror(errno));
		return -1;
	} 
	return 0;
}

static int jdsp_indicate(struct ast_channel *chan, int condition, const void *data, size_t datalen)
{
	struct jdsp_pvt *p = chan->tech_pvt;
	int res = -1;
	int index;

	ast_mutex_lock(&p->lock);
	index = jdsp_get_index(chan, p, 0);
	ast_log(LOG_DEBUG, "Requested indication %d on channel %s\n", condition, chan->name);
	if (index == SUB_REAL) {
		switch(condition) {
		case AST_CONTROL_BUSY:
			jdsp_stop_audio(chan, index);
		        if (data && !strcmp((const char *)data, "unobtainable"))
				res = jdsp_play_tone(p, PHONE_TONE_BUSY_UNOBTAINABLE);
			else
				res = jdsp_play_tone(p, PHONE_TONE_BUSY);
			break;
		case AST_CONTROL_UNALLOCATED:
			ast_log(LOG_DEBUG,"Received AST_CONTROL_UNALLOCATED on %s\n",chan->name);

			/* In case of deadline tone is not supported,
			 * behavior is like congestion case */
			if (dead_line_tone_support) {
				res = jdsp_play_tone(p, PHONE_TONE_UNALLOCATED);
				break;
			}
		case AST_CONTROL_CONGESTION:
			ast_log(LOG_DEBUG,"Received AST_CONTROL_CONGESTION on %s\n",chan->name);
			chan->hangupcause = AST_CAUSE_CONGESTION;

			/* after hangup we'll play reorder for timeout */
			ast_queue_hangup(chan);
			res = 0;
			break;
		case AST_CONTROL_RINGING:
			if (chan->_state == AST_STATE_RINGING)
			{
				res = 0;
				break;
			}

			res = jdsp_play_tone(p, PHONE_TONE_RING);
			/* start audio now to enable early CNG detection */
			jdsp_start_audio(chan, index);
			if (chan->_state != AST_STATE_UP) {
			    	ast_setstate(chan, AST_STATE_RINGING);
			}
            		break;
		case AST_CONTROL_PROCEEDING:
			ast_log(LOG_DEBUG,"Received AST_CONTROL_PROCEEDING on %s\n",chan->name);
			/* don't continue in ast_indicate */
			res = 0;
			break;
		case AST_CONTROL_PROGRESS:
			/* XXX we assume that we received AST_CONTROL_PROGRESS after receiving
			 * early media. hence, the state can be changed to state RINGING and
			 * local ring back should not be played. */
			ast_setstate(chan, AST_STATE_RINGING);
			ast_log(LOG_DEBUG,"Received AST_CONTROL_PROGRESS on %s\n",chan->name);
			jdsp_play_tone(p, PHONE_TONE_NONE);
			/* don't continue in ast_indicate */
			res = 0;
			break;
		case AST_CONTROL_CALL_LIMIT:
			ast_log(LOG_DEBUG,"Received AST_CONTROL_CALL_LIMIT on %s\n",chan->name);
			chan->hangupcause = AST_CAUSE_CALL_LIMIT;

			/* after hangup we'll play reorder timeout */
			ast_queue_hangup(chan);
			res = 0;
			break;			
		case AST_CONTROL_NOANSWER:
			if (p->subs[SUB_THREEWAY].owner)
			{
			    ast_log(LOG_DEBUG, "has threeway channel (%s) and "
				"second call did not answer. call hangup\n", 
				p->subs[SUB_THREEWAY].owner->name);
			}
			/* We didn't recieve answer, so we can hang up the
			   channel and play reorder tone */
			chan->hangupcause = AST_CAUSE_NOANSWER;
			ast_queue_hangup(chan);
			res = 0;
			break;
		case AST_CONTROL_HOLD:
			ast_log(LOG_DEBUG, "Received AST_CONTROL_HOLD on %s\n", chan->name);
			if (p->use_hold_tone)
				res = jdsp_play_tone(p, PHONE_TONE_HOLD);
			else			    
				res = 0;
			break;
		case AST_CONTROL_UNHOLD:
			ast_log(LOG_DEBUG, "Received AST_CONTROL_UNHOLD on %s\n", chan->name);
			res = jdsp_play_tone(p, PHONE_TONE_NONE);
			break;
		case -1:
		case AST_CONTROL_ANSWER:
			/* ANSWER may be received during bridge when we were blind 
			 * transfered to another party, and they picked up. */
			res = jdsp_play_tone(p, PHONE_TONE_NONE);
			break;
		case AST_CONTROL_T38:
			ast_log(LOG_DEBUG, "Received AST_CONTROL_T38 on %s, switching to "
				"T38\n", chan->name);
			/* Delete modem timer */
			if (p->modem_timer_sched_id > -1)
			{
				ast_log(LOG_DEBUG, "Cancel modem detect timer\n");
				ast_sched_del(sched, p->modem_timer_sched_id);
				p->modem_timer_sched_id = -1;
			}

			p->t38enabled = 1;
			p->faxdetected = 1;
			p->modemdetected = 0;
			jdsp_modify_audio(chan, SUB_REAL);
			res = 0;
			break;
		case AST_CONTROL_CALLBACK:
			{
			    const ast_callback_frame_payload_t *payload;

			    payload = (const ast_callback_frame_payload_t *)data;
			    if (data && payload->state == AST_CALLBACK_IN_PROGRESS)
			    {
				    jdsp_start_audio(chan, index);
				    p->callwaiting = 0;
			    }
			    else if (data && payload->state == AST_CALLBACK_ANSWERED)
			    {
				    if (p->permcallwaiting && jdsp_is_call_waiting_enabled(p->owner))
					    p->callwaiting = 1;

			    }
			    res = 0;
			    break;
			}
		}
	} else
		res = 0;
	ast_mutex_unlock(&p->lock);
	return res;
}

static struct ast_channel *jdsp_new(struct jdsp_pvt *i, int state, int startpbx, int index, int transfercapability)
{
	struct ast_channel *tmp;
	int x,y;
	if (i->subs[index].owner) {
		ast_log(LOG_WARNING, "Channel %d already has a %s call\n", i->channel,subnames[index]);
		return NULL;
	}

	tmp = ast_channel_alloc(1);
	if (tmp) {
		tmp->tech = &jdsp_tech;
		y = 1;
		do {
			snprintf(tmp->name, sizeof(tmp->name), "jdsp/%d-%d", i->channel, y);
			for (x = 0; x < 3; x++) {
				if ((index != x) && i->subs[x].owner && !strcasecmp(tmp->name, i->subs[x].owner->name))
					break;
			}
			y++;
		} while (x < 3);

		if (i->use_caller_variable)
			snprintf(tmp->template_caller_name, sizeof(tmp->template_caller_name), "jdsp/%d", i->channel);
		tmp->type = type;
		if (index == SUB_REAL)
		{
			tmp->fds[0] = i->jfd;
			tmp->fds[1] = i->subs[SUB_REAL].dfd;
			tmp->fds[2] = i->io_pipe[0];
		}

		memcpy(&tmp->nativeformats, &global_native_formats, sizeof(global_native_formats));
		tmp->preferred_codec = global_preferred_codec;

		if (index == SUB_THREEWAY && i->transfermode != TRANSFER_OFF)
		{
			tmp->fds[0] = i->subs[SUB_THREEWAY].dfd;
		    /* XXX Temporary solution for the problematic scenario where
		     * we're in a call with party A using codec X, put them on
		     * hold, and call party B which chooses codec Y, and then
		     * try to transfer A to B, the transfer will fail since we
		     * will try to be the bridge but we can't do codec
		     * conversion. Before making the 2nd call, we pretend to
		     * support only the codec that is already used in the 1st
		     * call, and set the '_FORCECODEC' variable to indicate the
		     * trunk channel to include only this codec in the SDP.
		     * This fix will be no longer needed when either transfer is
		     * be implemented using REFER, or when a new negotiation
		     * algorithm will be used that will allow us to force a
		     * codec in more graceful way. */
		    /* XXX The current implementation assumes that's there's a single
 	             * high and low quality codecs. We need the fix because of the
 	             * previous comment and the assumption that all internal calls that
 	             * are "bridge" transferred could always have high quality, transfer
 	             * to external SIP is "signalled" so there's no issue with different
 	             * codecs for the sides of the transfer */
		    ast_codec_pref_remove2(&tmp->nativeformats,
			    heavyweight_codecs);
		    ast_codec_pref_set_top_quality(&tmp->nativeformats,
		    	    ast_codec_get_quality(i->subs[SUB_REAL].owner->readformat));
		    pbx_builtin_setvar_helper(tmp, "_FORCECODEC", "");
		}

		/* Start out assuming ulaw since it's smaller :) */
		tmp->rawreadformat = deflaw;
		tmp->readformat = deflaw;
		tmp->rawwriteformat = deflaw;
		tmp->writeformat = deflaw;
		
		if (state == AST_STATE_RING)
			tmp->rings = 1;
		tmp->tech_pvt = i;
		tmp->callgroup = i->callgroup;
		tmp->pickupgroup = i->pickupgroup;
		if (!ast_strlen_zero(i->language))
			ast_copy_string(tmp->language, i->language, sizeof(tmp->language));
		if (!ast_strlen_zero(i->musicclass))
			ast_copy_string(tmp->musicclass, i->musicclass, sizeof(tmp->musicclass));
		if (!i->owner)
			i->owner = tmp;
		if (!ast_strlen_zero(i->accountcode))
			ast_copy_string(tmp->accountcode, i->accountcode, sizeof(tmp->accountcode));
		i->subs[index].owner = tmp;
		ast_copy_string(tmp->context, i->context, sizeof(tmp->context));
		/* If we've been told "no ADSI" then enforce it */
		if (!i->adsi)
			tmp->adsicpe = AST_ADSI_UNAVAILABLE;
		if (!ast_strlen_zero(i->exten))
			ast_copy_string(tmp->exten, i->exten, sizeof(tmp->exten));
		if (!ast_strlen_zero(i->rdnis))
			tmp->cid.cid_rdnis = strdup(i->rdnis);
		if (!ast_strlen_zero(i->dnid))
			tmp->cid.cid_dnid = strdup(i->dnid);

		ast_set_callerid(tmp, i->cid_num, i->cid_name, i->cid_num);
		tmp->cid.cid_pres = i->callingpres;
		tmp->cid.cid_ton = i->cid_ton;
		jdsp_build_callid(tmp->shared_callid, AST_MAX_SHARED_CALLID);
		ast_setstate(tmp, state);
		ast_mutex_lock(&usecnt_lock);
		usecnt++;
		ast_mutex_unlock(&usecnt_lock);
		ast_update_use_count();
		if (i->hidecallerid)
			pbx_builtin_setvar_helper(tmp, "_EXT_CALLERID_RESTRICTED", "1");
		if (startpbx) {
			if (ast_pbx_start(tmp)) {
				ast_log(LOG_WARNING, "Unable to start PBX on %s\n", tmp->name);
				ast_hangup(tmp);
				tmp = NULL;
			}
		}
	} else
		ast_log(LOG_WARNING, "Unable to allocate channel structure\n");
	return tmp;
}

#define CFWD_UNCONDITIONAL_ENTRY "call_forwarding_unconditional"
#define CFWD_BUSY_ENTRY "call_forwarding_on_busy"
#define CFWD_NO_ANSWER_ENTRY "call_forwarding_on_no_answer"

static void configure_cfwd(int channel, char *entry, char *dest)
{
    char cmd[256];

    snprintf(cmd, sizeof(cmd), "conf set voip/line/%d/%s/enabled "
	"%d", channel - 1, entry, dest ? 1 : 0); 
    ast_run_openrg_cmd(cmd);
    
    if (dest)
    {
	snprintf(cmd, sizeof(cmd), "conf set voip/line/%d/%s/"
	    "destination %s", channel - 1, entry, dest);
	ast_run_openrg_cmd(cmd);
    }

    ast_run_openrg_cmd("conf reconf 1");
}

static int match_code(char *exten, char *code)
{
	return !ast_strlen_zero(code) && ast_extension_match(code, exten);
}

static char *get_cfwd_activate_type(struct jdsp_pvt *p, char *exten)
{
    if (match_code(exten, p->cfwd_unconditional_activate_code))
	return CFWD_UNCONDITIONAL_ENTRY;
    else if (match_code(exten, p->cfwd_busy_activate_code)) 
	return CFWD_BUSY_ENTRY;
    else if (match_code(exten, p->cfwd_no_answer_activate_code))
	return CFWD_NO_ANSWER_ENTRY;
    else
	return NULL;
}

static char *get_cfwd_deactivate_type(struct jdsp_pvt *p, char *exten)
{
    if (match_code(exten, p->cfwd_unconditional_deactivate_code))
	return CFWD_UNCONDITIONAL_ENTRY;
    else if (match_code(exten, p->cfwd_busy_deactivate_code)) 
	return CFWD_BUSY_ENTRY;
    else if (match_code(exten, p->cfwd_no_answer_deactivate_code))
	return CFWD_NO_ANSWER_ENTRY;
    else
	return NULL;
}

static void *ss_thread(void *data)
{
	struct ast_channel *chan = data;
	struct jdsp_pvt *p = chan->tech_pvt;
	char exten[AST_MAX_EXTENSION] = "";
	int timeout;
	char *getforward = NULL;
	int len = 0;
	int res;
	int index;
	int blindtransfer = 0;
	int stop_immediatly = 0;
	if (option_verbose > 2) 
		ast_verbose( VERBOSE_PREFIX_3 "Starting simple switch on '%s'\n", chan->name);
	index = jdsp_get_index(chan, p, 1);
	if (index < 0) {
		ast_log(LOG_WARNING, "Huh?\n");
		ast_hangup(chan);
		return NULL;
	}
	/* Read the first digit */
	timeout = GET_FIRSTDIGIT_TIMEOUT(p);
	while(len < AST_MAX_EXTENSION-1) {
	    	char *cfwd_type = NULL;

		/* Read digit unless it's supposed to be immediate, in which case the
		   only answer is 's'. The 'immediate' flag affects only the
		   behavior after taking the phone off-hook, therefore it's
		   ignored in case of a follow call. */
		if ((p->immediate && !p->follow_call) || p->network_failure_disconnection)
			res = 's';
		else
			res = ast_waitfordigit(chan, timeout);
		timeout = 0;
		if (res == AST_RESULT_CALLBACK) 
		{
		    res = jdsp_play_tone(p, PHONE_TONE_NONE);
		    ast_copy_string(chan->exten, p->callback_extension,
				sizeof(chan->exten));
		    if (!ast_strlen_zero(p->cid_num)) {
				if (!p->hidecallerid)
			    	ast_set_callerid(chan, p->cid_num, NULL, p->cid_num); 
				else
			    	ast_set_callerid(chan, NULL, NULL, p->cid_num); 
		    	}
		    if (!ast_strlen_zero(p->cid_name)) {
				if (!p->hidecallerid)
			    	ast_set_callerid(chan, NULL, p->cid_name, NULL);
		    }
		    ast_setstate(chan, AST_STATE_RING);
		    p->callback_extension[0] ='\0';
		    res = ast_pbx_run(chan);
		    if (res) {
				ast_log(LOG_WARNING, "PBX exited non-zero\n");
				res = jdsp_play_tone(p, PHONE_TONE_REORDER);
		    }
		    return NULL;
		}
		/*  We want to detect an automatic call only if no digit was
		 *  dialed at all (len == 0 && res == 0) and it's not a three way call*/
		if (p->automatic_call && !res && !len && !p->subs[SUB_THREEWAY].owner) 
		{
			ast_log(LOG_DEBUG, "automatic_call use: waitfordigit returned 0\n");
		    	res = 's';
		}
		if (res < 0) {
			ast_log(LOG_DEBUG, "waitfordigit returned < 0...\n");
			res = jdsp_play_tone(p, PHONE_TONE_NONE);
			ast_hangup(chan);
			return NULL;
		} else if (res)  {
			if (stop_immediatly_pound && (res == '#') && (len > 0))
			{
				/* Supplmentry services start with either # or * */
				if ((exten[0] == '#' || exten[0] == '*')
				    && !jdsp_is_exten_num(exten_num_lst, exten)
				    && !jdsp_has_out_call_prefix(out_call_prefix_lst, exten))
				{
					exten[len++] = res;
					exten[len] = '\0';
				}
				else
					stop_immediatly = 1;
			}
			else
			{
				exten[len++] = res;
				exten[len] = '\0';
			}
		}
		if (!ast_ignore_pattern(chan->context, exten))
			jdsp_play_tone(p, PHONE_TONE_NONE);
		else
			jdsp_play_tone(p, PHONE_TONE_DIAL);
		if (!strcmp(exten, "*98") && p->subs[SUB_THREEWAY].owner) {
			if (option_verbose > 2) 
				ast_verbose(VERBOSE_PREFIX_3 "Initiating blind transfer on %s\n", chan->name);

			/* Treat the next dialed number as a
			 * destination for blind transfer */
			blindtransfer = 1;

			res = jdsp_play_tone(p, PHONE_TONE_DIAL); /* XXX need secondary dial tone */
			if (res) {
				ast_log(LOG_WARNING, "Unable to do dial recall on channel %s: %s\n", 
					chan->name, strerror(errno));
			}
			len = 0;
			*exten = 0;
			timeout = GET_FIRSTDIGIT_TIMEOUT(p);
		} else if ((cfwd_type = get_cfwd_activate_type(p, exten))) {
			jdsp_play_tone(p, PHONE_TONE_STUTTER_DIAL);
			getforward = cfwd_type;
			memset(exten, 0, sizeof(exten));
			len = 0;
		} else if ((cfwd_type = get_cfwd_deactivate_type(p, exten))) {
			if (option_verbose > 2)
				ast_verbose(VERBOSE_PREFIX_3 "Cancelling call forwarding unconditional on channel %d\n", p->channel);
			jdsp_play_tone(p, PHONE_TONE_CONFIRM);
			configure_cfwd(p->channel, cfwd_type, NULL); 
			getforward = NULL;
			memset(exten, 0, sizeof(exten));
			len = 0;
		} else if (match_code(exten, p->dnd_activate_code) ||
			match_code(exten, p->dnd_deactivate_code)) {
		    	char cmd[256];

			jdsp_play_tone(p, PHONE_TONE_CONFIRM);
			snprintf(cmd, sizeof(cmd), "conf set "
			    "voip/line/%d/do_not_disturb_enabled %d",
			    p->channel - 1,
			    match_code(exten, p->dnd_activate_code) ? 1 : 0); 
			ast_run_openrg_cmd(cmd);
			ast_run_openrg_cmd("conf reconf 1");

			memset(exten, 0, sizeof(exten));
			len = 0;
		} else if (ast_exists_extension(chan, chan->context, exten, 1, p->cid_num) ||
			(!ast_strlen_zero(exten) && strcmp(exten, ast_parking_ext()))) {
			if (!res || !ast_matchmore_extension(chan, chan->context, exten, 1, p->cid_num)) {
				if (getforward) {
					/* Record this as the forwarding extension */
					configure_cfwd(p->channel, getforward, exten); 
					if (option_verbose > 2)
						ast_verbose(VERBOSE_PREFIX_3 "Setting call forward to '%s' on channel %d\n", exten, p->channel);
					jdsp_play_tone(p, PHONE_TONE_CONFIRM);
					memset(exten, 0, sizeof(exten));
					len = 0;
					getforward = NULL;
				} else if (blindtransfer) {
					struct ast_frame f;

					/* If the transferee hung up, stop the transfer */
					if (!p->subs[SUB_THREEWAY].owner)
						goto Exit;

					/* Tell bridged channel to do
					 * blind transfer */
					memset(&f, 0 , sizeof(f));
					f.frametype = AST_FRAME_BLINDTRANSFER;
					f.data = exten;
					f.datalen = strlen(exten) + 1;
					ast_queue_frame(p->subs[SUB_THREEWAY].owner, &f);
				
					/* Provide a new dial tone and
					 * allow making another call */
					blindtransfer = 0;
					len = 0;
					*exten = 0;
					timeout = GET_FIRSTDIGIT_TIMEOUT(p);
					jdsp_play_tone(p, PHONE_TONE_CONFIRM);
				} else {
					res = jdsp_play_tone(p, PHONE_TONE_NONE);
					ast_copy_string(chan->exten, exten, sizeof(chan->exten));
					if (!ast_strlen_zero(p->cid_num)) {
						if (!p->hidecallerid)
							ast_set_callerid(chan, p->cid_num, NULL, p->cid_num); 
						else
							ast_set_callerid(chan, NULL, NULL, p->cid_num); 
					}
					if (!ast_strlen_zero(p->cid_name)) {
						if (!p->hidecallerid)
							ast_set_callerid(chan, NULL, p->cid_name, NULL);
					}
					ast_setstate(chan, AST_STATE_RING);
					res = ast_pbx_run(chan);
					if (res) {
						ast_log(LOG_WARNING, "PBX exited non-zero\n");
						res = jdsp_play_tone(p, PHONE_TONE_REORDER);
					}
					return NULL;
				}
			} else if (stop_immediatly) {
				timeout = 1;
			} else {
				/* It's a match, but they just typed a digit, and there is an ambiguous match,
				   so just set the timeout to matchdigittimeout and wait some more */
				timeout = p->matchdigittimeout;
			}
		} else if (res == 0) {
			ast_log(LOG_DEBUG, "not enough digits (and no ambiguous match)...\n");
			ast_hangup(chan);
			return NULL;
		} else if (p->callwaiting && !strcmp(exten, "*70")) {
			if (option_verbose > 2) 
				ast_verbose(VERBOSE_PREFIX_3 "Disabling call waiting on %s\n", chan->name);
			/* Disable call waiting if enabled */
			p->callwaiting = 0;
			res = jdsp_play_tone(p, PHONE_TONE_DIAL /*JDSP_TONE_DIALRECALL*/);
			if (res) {
				ast_log(LOG_WARNING, "Unable to do dial recall on channel %s: %s\n", 
					chan->name, strerror(errno));
			}
			len = 0;
			memset(exten, 0, sizeof(exten));
			timeout = GET_FIRSTDIGIT_TIMEOUT(p);
				
		} else if (!strcmp(exten,ast_pickup_ext())) {
			/* Scan all channels and see if any there
			 * ringing channqels with that have call groups
			 * that equal this channels pickup group  
			 */
		  	if (index == SUB_REAL) {
				/* Switch us from Third call to Call Wait */
			  	if (p->subs[SUB_THREEWAY].owner) {
					/* If you make a threeway call and the *8# a call, it should actually 
					   look like a callwait */
				  	swap_subs(p, SUB_CALLWAIT, SUB_THREEWAY);
					unalloc_sub(p, SUB_THREEWAY);
				}
				if (ast_pickup_call(chan)) {
					ast_log(LOG_DEBUG, "No call pickup possible...\n");
					res = jdsp_play_tone(p, PHONE_TONE_REORDER);
				}
				ast_hangup(chan);
				return NULL;
			} else {
				ast_log(LOG_WARNING, "Huh?  Got *8# on call not on real\n");
				ast_hangup(chan);
				return NULL;
			}
			
		} 
#ifndef CONFIG_RG_VODAFONE
		else if (!p->hidecallerid && !strcmp(exten, "*67")) {
			if (option_verbose > 2) 
				ast_verbose(VERBOSE_PREFIX_3 "Disabling Caller*ID on %s\n", chan->name);
			/* Disable Caller*ID if enabled */
			p->hidecallerid = 1;
			if (chan->cid.cid_num)
				free(chan->cid.cid_num);
			chan->cid.cid_num = NULL;
			if (chan->cid.cid_name)
				free(chan->cid.cid_name);
			chan->cid.cid_name = NULL;
			res = jdsp_play_tone(p, PHONE_TONE_DIAL /*JDSP_TONE_DIALRECALL*/);
			if (res) {
				ast_log(LOG_WARNING, "Unable to do dial recall on channel %s: %s\n", 
					chan->name, strerror(errno));
			}
			len = 0;
			memset(exten, 0, sizeof(exten));
			timeout = GET_FIRSTDIGIT_TIMEOUT(p);
		} else if (p->callreturn && !strcmp(exten, "*69")) {
			res = 0;
			if (!ast_strlen_zero(p->lastcid_num)) {
				res = ast_say_digit_str(chan, p->lastcid_num, "", chan->language);
			}
			if (!res)
				res = jdsp_play_tone(p, PHONE_TONE_DIAL /*JDSP_TONE_DIALRECALL*/);
			goto Exit;
		} else if ((p->transfermode == TRANSFER_BRIDGING || p->canpark) && !strcmp(exten, ast_parking_ext()) && 
					p->subs[SUB_THREEWAY].owner &&
					ast_bridged_channel(p->subs[SUB_THREEWAY].owner)) {
			/* This is a three way call, the main call being a real channel, 
				and we're parking the first call. */
			ast_masq_park_call(ast_bridged_channel(p->subs[SUB_THREEWAY].owner), chan, 0, NULL);
			if (option_verbose > 2)
				ast_verbose(VERBOSE_PREFIX_3 "Parking call to '%s'\n", chan->name);
			goto Exit;
		} else if (!ast_strlen_zero(p->lastcid_num) && !strcmp(exten, "*60")) {
			if (option_verbose > 2)
				ast_verbose(VERBOSE_PREFIX_3 "Blacklisting number %s\n", p->lastcid_num);
			res = ast_db_put("blacklist", p->lastcid_num, "1");
			if (!res) {
				res = jdsp_play_tone(p, PHONE_TONE_DIAL /*JDSP_TONE_DIALRECALL*/);
				memset(exten, 0, sizeof(exten));
				len = 0;
			}
		} else if (p->hidecallerid && !strcmp(exten, "*82")) {
			if (option_verbose > 2) 
				ast_verbose(VERBOSE_PREFIX_3 "Enabling Caller*ID on %s\n", chan->name);
			/* Enable Caller*ID if enabled */
			p->hidecallerid = 0;
			if (chan->cid.cid_num)
				free(chan->cid.cid_num);
			chan->cid.cid_num = NULL;
			if (chan->cid.cid_name)
				free(chan->cid.cid_name);
			chan->cid.cid_name = NULL;
			ast_set_callerid(chan, p->cid_num, p->cid_name, NULL);
			res = jdsp_play_tone(p, PHONE_TONE_DIAL /*JDSP_TONE_DIALRECALL*/);
			if (res) {
				ast_log(LOG_WARNING, "Unable to do dial recall on channel %s: %s\n", 
					chan->name, strerror(errno));
			}
			len = 0;
			memset(exten, 0, sizeof(exten));
			timeout = GET_FIRSTDIGIT_TIMEOUT(p);
		} 
#endif		
		else if (!ast_canmatch_extension(chan, chan->context, exten, 1, chan->cid.cid_num) &&
						((exten[0] != '*') || (strlen(exten) > 2))) {
			if (option_debug)
				ast_log(LOG_DEBUG, "Can't match %s from '%s' in context %s\n", exten, chan->cid.cid_num ? chan->cid.cid_num : "<Unknown Caller>", chan->context);
			goto Exit;
		}
		if (!timeout)
			timeout = p->gendigittimeout;
		if (len && !ast_ignore_pattern(chan->context, exten))
			jdsp_play_tone(p, PHONE_TONE_NONE);
	}
	
Exit:
	res = jdsp_play_tone(p, PHONE_TONE_REORDER);
	if (res < 0)
			ast_log(LOG_WARNING, "Unable to play congestion tone on channel %d\n", p->channel);
	ast_hangup(chan);
	return NULL;
}

static void start_call(struct jdsp_pvt *i)
{
    pthread_t threadid;
    pthread_attr_t attr;
    struct ast_channel *chan;
    int res;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	/* Check for callerid, digits, etc */
	chan = jdsp_new(i, AST_STATE_RESERVED, 0, SUB_REAL, 0);
	if (chan) {
		if (i->network_failure_disconnection)
		{
		    	pbx_builtin_setvar_helper(chan, "SYSTEM_NETWORK_DISCONNECTION", "1");
		    	res = jdsp_play_tone(i, PHONE_TONE_NONE);
		}
		else
		        res = jdsp_play_tone(i, get_dialtone(i));
		if (res < 0) 
			ast_log(LOG_WARNING, "Unable to play dialtone on channel %d\n", i->channel);
		if (ast_pthread_create(&threadid, &attr, ss_thread, chan)) {
			ast_log(LOG_WARNING, "Unable to start simple switch thread on channel %d\n", i->channel);
			res = jdsp_play_tone(i, PHONE_TONE_REORDER);
			if (res < 0)
				ast_log(LOG_WARNING, "Unable to play congestion tone on channel %d\n", i->channel);
			ast_hangup(chan);
		}
		call_key_seq_reset(i);
	} else
		ast_log(LOG_WARNING, "Unable to create channel\n");
}

static int is_chan_sebi_used(void)
{
    struct ast_channel *chan = NULL;
    int issebiused = 0;

    while ((chan = ast_channel_walk_locked(chan)) != NULL)
    {
		issebiused = !strcasecmp(chan->tech->type,"Sebi");
		ast_mutex_unlock(&chan->lock);
		if (issebiused)
	 		return 1;
    }
    return 0;
}

static void alloc_and_start_call(struct jdsp_pvt *i)
{
	if (jdsp_dc_exclusive_alloc(i, 0, 0))
	{
		/* Try to allocate lame DC to indicate DC unavailabilty */
		if (!jdsp_dc_exclusive_alloc(i, 0, 1)) 
		{ 
			if (jdsp_play_tone(i, PHONE_TONE_REORDER) < 0)
			{
				ast_log(LOG_WARNING, "Unable to play congestion tone on channel"
					"%d\n", i->channel);
			}
		}
		return;
	}

	i->real_dc_num = 0;
	if (i->immediate) 
	{
		/* The channel is immediately up.  Start right away */
		if (!jdsp_new(i, AST_STATE_RING, 1, SUB_REAL, 0)) 
		{
			ast_log(LOG_WARNING, "Unable to start PBX on channel %d\n", 
				i->channel);
			if (jdsp_play_tone(i, PHONE_TONE_REORDER) < 0)
			{
				ast_log(LOG_WARNING, "Unable to play congestion tone on channel"
					"%d\n", i->channel);
			}
			jdsp_dc_free(i, 0);
		}
	} 
	else 
		start_call(i);
}

static int handle_init_event(struct jdsp_pvt *i, phone_event_t *event)
{
	int res;

	if (!i->enabled)
		return 0;

	ast_log(LOG_DEBUG, "Got event \"%s\" on channel %d\n", jdsp_event2str(event), i->channel);
	/* Handle an event on a given channel for the monitor thread. */
	switch(event->key) {
	case PHONE_KEY_HOOK_OFF:
		ast_log(LOG_MFT_INFO, "phone %d: Hook off\n", i->channel);
		manager_event(EVENT_FLAG_CALL, "Offhook",
			"Channel: jdsp/%d-1\r\n", i->channel);
		/* Skipping OFFHOOK because it originates from forward disconnect. */
		if (i->fwd_disconnect == FWD_DISCONNECT_IN_PROGRESS) {
			jdsp_play_tone(i, PHONE_TONE_REORDER);
			i->fwd_disconnect = FWD_DISCONNECT_ON;
			break;
		}
		/* Check if chan_sebi in use - not available if internal calls enabled */
		if (!is_non3g_second_callsallowed && is_chan_sebi_used())
		{
		    jdsp_play_tone(i, PHONE_TONE_REORDER);
		    break;		
		}
		alloc_and_start_call(i);
		break;
	case PHONE_KEY_FLASH:
		jdsp_handle_flash_key(NULL, i, FLASH_KEY, SUB_REAL);
		break;
	case PHONE_KEY_HOOK_ON:
		ast_log(LOG_MFT_INFO, "phone %d: Hook on\n", i->channel);
		manager_event(EVENT_FLAG_CALL, "Onhook",
			"Channel: jdsp/%d-1\r\n", i->channel);
		res = jdsp_play_tone(i, PHONE_TONE_NONE);
		if (i->real_dc_num != -1)
			jdsp_dc_free(i, i->real_dc_num);
		break;
	case PHONE_KEY_PORT_ATTACHED:
		ast_log(LOG_NOTICE, "Channel %d is connected to a phone\n", i->channel); 
		manager_event(EVENT_FLAG_SYSTEM, "PhoneAttachState",
			"Channel: %d\r\nState: Connected\r\n", i->channel);
		break;
	case PHONE_KEY_PORT_DETACHED:
		ast_log(LOG_NOTICE, "Channel %d is not connected to a phone\n", i->channel); 
		manager_event(EVENT_FLAG_SYSTEM, "PhoneAttachState",
			"Channel: %d\r\nState: Disconnected\r\n", i->channel);
		break;
	default:
		ast_verbose("Don't know how to handle \"%s\" event on channel %d\n",
			jdsp_event2str(event), i->channel);
	}
	return 0;
}

static void jdsp_handle_vmwi(struct jdsp_pvt *p, time_t thispass)
{
	int is_off_hook = 0;

	if (thispass - p->onhooktime < 3 || 
		ioctl(p->jfd, VOIP_SLIC_GET_HOOK, &is_off_hook) || is_off_hook ||
		(p->owner && p->owner->_state == AST_STATE_RING)) {
		/* Cannot handle now - phone is offhook. 
		 * We keep 3 second from hangup to be on the safe side */
		return;
	}

	/* Activate/Deactivate VMWI */
	if (ioctl(p->jfd, VOIP_SLIC_VMWI, p->next_msg_wait)) {
		ast_log(LOG_ERROR, "Failed setting VMWI\n");
		return; /* Upon failure we do not update VMWI. It will be tryed again in
				 * next monitor loop */
	}

	/* Save last state */
	p->msg_wait = p->next_msg_wait;
	p->onhooktime = thispass;
}

static int is_mailbox_in_list(char *list, char *mailbox)
{
    char *i, *j = list;
    size_t len = strlen(mailbox);

    while ((i = strstr(j,mailbox)))
    {
	j = i + len;
	if ((i == list || *(i - 1) == ',') && (*j == '\0' || *j == ','))
	    return 1;
    }
    return 0;
}

/* Update VMWI state for this line */
void jdsp_message_waiting_notify(char *remotemailbox, int activate)
{
	struct jdsp_pvt *i;

	ast_mutex_lock(&iflock);

	/* Find username in list. */
	for (i = iflist; i; i = i->next)
	{
		if (i->mwi == MWI_EXTERNAL_GLOBAL || (i->mwi == MWI_EXTERNAL_PER_LINE &&
			is_mailbox_in_list(remotemailbox, i->mailbox)))
		{
			i->next_msg_wait = activate;
		}

	}
	ast_mutex_unlock(&iflock);
}

static void *do_monitor(void *data)
{
	int count, res, spoint, pollres = 0;
	struct jdsp_pvt *i;
	time_t thispass = 0, lastpass = 0;
	int found;
	struct pollfd *pfds = NULL;
	int lastalloc = -1;
	/* This thread monitors all the frame relay interfaces which are not yet in use
	   (and thus do not have a separate thread) indefinitely */
	/* From here on out, we die whenever asked */
	for(;;) {
		/* Lock the interface list */
		if (ast_mutex_lock(&iflock)) {
			ast_log(LOG_ERROR, "Unable to grab interface lock\n");
			return NULL;
		}
		if (!pfds || (lastalloc != ifcount)) {
			if (pfds)
				free(pfds);
			if (ifcount) {
				pfds = malloc(ifcount * sizeof(struct pollfd));
				if (!pfds) {
					ast_log(LOG_WARNING, "Critical memory error.  Jdsp dies.\n");
					ast_mutex_unlock(&iflock);
					return NULL;
				}
			}
			lastalloc = ifcount;
		}
		/* Build the stuff we're going to poll on, that is the socket of every
		   jdsp_pvt that does not have an associated owner channel */
		count = 0;
		i = iflist;
		while(i) {
			if ((i->jfd > -1) && i->sig) {
				if (!i->owner && !i->subs[SUB_REAL].owner) {
					/* This needs to be watched, as it lacks an owner */
					pfds[count].fd = i->jfd;
					pfds[count].events = POLLIN;
					pfds[count].revents = 0;
					count++;
				}
			}
			i = i->next;
		}
		/* Okay, now that we know what to do, release the interface lock */
		ast_mutex_unlock(&iflock);

		pthread_testcancel();
		res = ast_sched_wait(sched);
		if ((res < 0) || (res > 1000))
			res = 1000;
		/* Wait at most a second for something to happen */
		res = poll(pfds, count, res);
		pthread_testcancel();
		/* Okay, poll has finished.  Let's see what happened.  */
		if (res < 0) {
			if ((errno != EAGAIN) && (errno != EINTR))
				ast_log(LOG_WARNING, "poll return %d: %s\n", res, strerror(errno));
			continue;
		}
		ast_mutex_lock(&monlock);
		ast_sched_runq(sched);
		ast_mutex_unlock(&monlock);
		/* Alright, lock the interface list again, and let's look and see what has
		   happened */
		if (ast_mutex_lock(&iflock)) {
			ast_log(LOG_WARNING, "Unable to lock the interface list\n");
			continue;
		}
		found = 0;
		spoint = 0;
		lastpass = thispass;
		thispass = time(NULL);
		i = iflist;
		while(i) {
			if (vmwi_changed(i)) {
				/* Update Visual Message Waiting Indication  */
				jdsp_handle_vmwi(i, thispass);
			}
			if ((i->jfd > -1) && i->sig) {
				pollres = ast_fdisset(pfds, i->jfd, count, &spoint);
				if (pollres & POLLIN) {
					phone_event_t ev;
					if (i->owner || i->subs[SUB_REAL].owner) {
						ast_log(LOG_WARNING, "Whoa....  I'm owned but found (%d) in read...\n", i->jfd);
						i = i->next;
						continue;
					}

					/* Error may arise if the driver has detected a double key release. Ignore
					 * it and continue */
					if (jdsp_get_event(i->jfd, &ev) < 0)
						continue;

					if (option_debug)
						ast_log(LOG_DEBUG, "Monitor got event %s on channel %d\n", jdsp_event2str(&ev), i->channel);
					/* Don't hold iflock while handling init events -- race with chlock */
					ast_mutex_unlock(&iflock);
					handle_init_event(i, &ev);
					ast_mutex_lock(&iflock);	
				}
			}
			i = i->next;
		}
		ast_mutex_unlock(&iflock);
	}
	/* Never reached */
	return NULL;
	
}

static int restart_monitor(void)
{
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	/* If we're supposed to be stopped -- stay stopped */
	if (monitor_thread == AST_PTHREADT_STOP)
		return 0;
	if (ast_mutex_lock(&monlock)) {
		ast_log(LOG_WARNING, "Unable to lock monitor\n");
		return -1;
	}
	if (monitor_thread == pthread_self()) {
		ast_mutex_unlock(&monlock);
		ast_log(LOG_WARNING, "Cannot kill myself\n");
		return -1;
	}
	if (monitor_thread != AST_PTHREADT_NULL) {
		/* Just signal it to be sure it wakes up */
		pthread_kill(monitor_thread, SIGURG);
	} else {
		/* Start a new monitor */
		if (ast_pthread_create(&monitor_thread, &attr, do_monitor, NULL) < 0) {
			ast_mutex_unlock(&monlock);
			ast_log(LOG_ERROR, "Unable to start monitor thread.\n");
			return -1;
		}
	}
	ast_mutex_unlock(&monlock);
	return 0;
}

static struct jdsp_pvt *mkintf(int channel, int dsp_line, int signalling, int reloading)
{
	/* Make a jdsp_pvt structure for this interface (or CRV if "pri" is specified) */
	struct jdsp_pvt *tmp = NULL, *tmp2,  *prev = NULL;
	int here = 0;
	int x;
	struct jdsp_pvt **wlist;
	struct jdsp_pvt **wend;

	wlist = &iflist;
	wend = &ifend;

	tmp2 = *wlist;
	prev = NULL;

	while (tmp2) {
		if (tmp2->channel == channel) {
			tmp = tmp2;
			here = 1;
			break;
		}
		if (tmp2->channel > channel) {
			break;
		}
		prev = tmp2;
		tmp2 = tmp2->next;
	}

	if (!here && !reloading) {
		tmp = (struct jdsp_pvt*)malloc(sizeof(struct jdsp_pvt));
		if (!tmp) {
			ast_log(LOG_ERROR, "MALLOC FAILED\n");
			destroy_jdsp_pvt(&tmp);
			return NULL;
		}
		memset(tmp, 0, sizeof(struct jdsp_pvt));
		ast_mutex_init(&tmp->lock);
		ifcount++;
		tmp->jfd = -1;
		tmp->io_pipe[0] = -1;
		tmp->io_pipe[1] = -1;
		tmp->tone_timeout_id = -1;
		tmp->modem_timer_sched_id = -1;
		for (x = 0; x < 3; x++)
		{
			tmp->subs[x].chan = -1;
			tmp->subs[x].dfd = -1;
		}
		tmp->channel = channel;
		tmp->dsp_line = dsp_line;
	}

	if (tmp) {
		if (!here) {
			/* Open non-blocking */
			tmp->jfd = jdsp_open_slic(tmp->dsp_line);
			tmp->subs[SUB_REAL].dfd = jdsp_open_dsp();
			tmp->subs[SUB_THREEWAY].dfd = jdsp_open_dsp();
			if (pipe(tmp->io_pipe) == -1)
			{
				ast_log(LOG_ERROR, "Failed to create io_pipe.\n");
				destroy_jdsp_pvt(&tmp);
				return NULL;
			}
			/* Allocate a jdsp structure */
			if (tmp->jfd < 0) {
				ast_log(LOG_ERROR, "Unable to open channel %d: %s\nhere = %d, tmp->channel = %d, channel = %d\n", channel, strerror(errno), here, tmp->channel, channel);
				destroy_jdsp_pvt(&tmp);
				return NULL;
			}
		} else {
			signalling = tmp->sig;
		}
		tmp->enabled = enabled;
		tmp->immediate = immediate;
		tmp->automatic_call = automatic_call;
 		tmp->transfertobusy = transfertobusy;
		tmp->internal_transfer_only = internal_transfer_only;
		tmp->sig = signalling;
		tmp->permcallwaiting = callwaiting;
		tmp->callwaitingcallerid = callwaitingcallerid;
		tmp->threewaycalling = threewaycalling;
		tmp->threewayconference = threewayconference;
		tmp->dialtone = dialtone;
		tmp->mwi = mwi;
		tmp->vmwi = vmwi;
		tmp->fwd_disconnect = fwd_disconnect;
		tmp->adsi = adsi;
		tmp->permhidecallerid = hidecallerid;
		tmp->callreturn = callreturn;
		tmp->callprogress = callprogress;
		ast_copy_string(tmp->cfwd_unconditional_activate_code, cfwd_unconditional_activate_code, sizeof(tmp->cfwd_unconditional_activate_code));
		ast_copy_string(tmp->cfwd_unconditional_deactivate_code, cfwd_unconditional_deactivate_code, sizeof(tmp->cfwd_unconditional_deactivate_code));
		ast_copy_string(tmp->cfwd_busy_activate_code, cfwd_busy_activate_code, sizeof(tmp->cfwd_busy_activate_code));
		ast_copy_string(tmp->cfwd_busy_deactivate_code, cfwd_busy_deactivate_code, sizeof(tmp->cfwd_busy_deactivate_code));
		ast_copy_string(tmp->cfwd_no_answer_activate_code, cfwd_no_answer_activate_code, sizeof(tmp->cfwd_no_answer_activate_code));
		ast_copy_string(tmp->cfwd_no_answer_deactivate_code, cfwd_no_answer_deactivate_code, sizeof(tmp->cfwd_no_answer_deactivate_code));
		ast_copy_string(tmp->dnd_activate_code, dnd_activate_code, sizeof(tmp->dnd_activate_code));
		ast_copy_string(tmp->dnd_deactivate_code, dnd_deactivate_code, sizeof(tmp->dnd_deactivate_code));
		tmp->callwaiting = tmp->permcallwaiting;
		tmp->hidecallerid = tmp->permhidecallerid;
		tmp->channel = channel;
		tmp->use_callerid = use_callerid;
		tmp->use_caller_variable = use_caller_variable;
		tmp->cid_signalling = cid_signalling;
		tmp->jdsptrcallerid = jdsptrcallerid;
		tmp->matchdigittimeout = matchdigittimeout;
        tmp->firstdigittimeout = firstdigittimeout;
		tmp->autocall_timeout = autocall_timeout;
		tmp->gendigittimeout = gendigittimeout;

		ast_copy_string(tmp->accountcode, accountcode, sizeof(tmp->accountcode));
		tmp->canpark = canpark;
		tmp->transfermode = transfermode;
		ast_copy_string(tmp->defcontext,context,sizeof(tmp->defcontext));
		ast_copy_string(tmp->language, language, sizeof(tmp->language));
		ast_copy_string(tmp->musicclass, musicclass, sizeof(tmp->musicclass));
		ast_copy_string(tmp->context, context, sizeof(tmp->context));
		ast_copy_string(tmp->cid_num, cid_num, sizeof(tmp->cid_num));
		tmp->cid_ton = 0;
		ast_copy_string(tmp->cid_name, cid_name, sizeof(tmp->cid_name));
		ast_copy_string(tmp->mailbox, mailbox, sizeof(tmp->mailbox));
		if (!here) {
			tmp->next_msg_wait = 0;
			tmp->msg_wait = 0;
		}
		tmp->group = cur_group;
		tmp->callgroup = cur_callergroup;
		tmp->pickupgroup = cur_pickupgroup;
		tmp->rxgain = rxgain;
		tmp->txgain = txgain;
		tmp->tonezone = tonezone;
		tmp->onhooktime = time(NULL);
		tmp->faxtxmethod = faxtxmethod;
		tmp->detectionmethod = detectionmethod;
		tmp->faxdetected = tmp->modemdetected = 0;
		tmp->follow_call = 0;
		tmp->network_failure_disconnection = 0;
		tmp->use_hold_tone = use_hold_tone;
		tmp->silence_suppression_enabled = silence_suppression_enabled; 

		/* Initializing statistics */
		memset(&tmp->stats, 0, sizeof(voip_dsp_stats_t));

		tmp->discriminate_call = discriminate_call;

		memset(tmp->call_matrix, 0, sizeof(call_matrix_array_t));
		if (configure_call_matrix)
			configure_call_matrix(tmp);

		if (ioctl(tmp->jfd, VOIP_SLIC_SET_POWER, &enabled ))
		    ast_log(LOG_ERROR, "Failed to set SLIC power"); 
	}
	if (tmp && !here) {
		/* nothing on the iflist */
		if (!*wlist) {
			*wlist = tmp;
			tmp->prev = NULL;
			tmp->next = NULL;
			*wend = tmp;
		} else {
			/* at least one member on the iflist */
			struct jdsp_pvt *working = *wlist;

			/* check if we maybe have to put it on the begining */
			if (working->channel > tmp->channel) {
				tmp->next = *wlist;
				tmp->prev = NULL;
				(*wlist)->prev = tmp;
				*wlist = tmp;
			} else {
			/* go through all the members and put the member in the right place */
				while (working) {
					/* in the middle */
					if (working->next) {
						if (working->channel < tmp->channel && working->next->channel > tmp->channel) {
							tmp->next = working->next;
							tmp->prev = working;
							working->next->prev = tmp;
							working->next = tmp;
							break;
						}
					} else {
					/* the last */
						if (working->channel < tmp->channel) {
							working->next = tmp;
							tmp->next = NULL;
							tmp->prev = working;
							*wend = tmp;
							break;
						}
					}
					working = working->next;
				}
			}
		}
	}
	return tmp;
}

static inline int available(struct jdsp_pvt *p, int channelmatch, int groupmatch, int *busy, int *channelmatched, int *groupmatched, int is_callback)
{
	/* First, check group matching */
	if (groupmatch) {
	    if ((p->group & groupmatch) != groupmatch)
			return 0;
		*groupmatched = 1;
	}
	/* Check to see if we have a channel match */
	if (channelmatch != -1) {
	    if (p->channel != channelmatch)
			return 0;
		*channelmatched = 1;
	}
	/* We're at least busy at this point */
	if (busy) {
		*busy = 1;
	}
	/* If guard time, definitely not */
	if (p->guardtime && (time(NULL) < p->guardtime)) 
		return 0;
		
	/* If no owner definitely available */
	if (!p->owner) {
	    	int res;
	    	int rxisoffhook;

	    	if (!p->sig)
			return 1;
	    	/* Check hook state */
	    	if (p->jfd > -1)
			res = ioctl(p->jfd, VOIP_SLIC_GET_HOOK, &rxisoffhook);
	    	else {
			/* Assume not off hook on CVRS */
			res = 0;
			rxisoffhook = 0;
	    	}
	    	if (res) {
			ast_log(LOG_WARNING, "Unable to check hook state on channel %d\n", p->channel);
	    	} else if (rxisoffhook && !is_callback) {
			ast_log(LOG_DEBUG, "Channel %d off hook, can't use\n", p->channel);
			/* Not available when the other end is off hook */
			return 0;
	    	}
		return 1;
	}
	if (is_callback && (p->owner->_state == AST_STATE_RESERVED ||
	    p->owner->_state == AST_STATE_OFFHOOK))
	{	
	    return 1;
	}

	if (!p->callwaiting || !jdsp_is_call_waiting_enabled(p->owner)) {
		/* If they don't have call waiting enabled, then for sure they're unavailable at this point */
		return 0;
	}

	if (p->subs[SUB_CALLWAIT].owner) {
		/* If there is already a call waiting call, then we can't take a second one */
		return 0;
	}

	if (p->owner->_state != AST_STATE_UP) {		
		/* If the current call is not up, then don't allow the call */
		return 0;
	}
	if ((p->subs[SUB_THREEWAY].owner)) {
		/* Can't take a call wait when we are in a three way call. */
		return 0;
	}
	/* We're cool */
	return 1;
}

static struct ast_channel *jdsp_request(const char *type, const struct ast_codec_pref *formats, void *data, int *cause)
{
	int groupmatch = 0;
	int channelmatch = -1;
	int roundrobin = 0;
	int callwait = 0, callback = 0;
	int busy = 0;
	struct jdsp_pvt *p;
	struct ast_channel *tmp = NULL;
	char *dest = NULL;
	int x;
	char *s;
	char opt = 0;
	int res = 0, y = 0;
	int backwards = 0;
	struct jdsp_pvt *exit, *start, *end;
	ast_mutex_t *lock;
	int channelmatched = 0;
	int groupmatched = 0;
	
	/* Assume we're locking the iflock */
	lock = &iflock;
	start = iflist;
	end = ifend;
	/* We do signed linear */
	if (data) {
		/* callback behavior */
	    if (strstr((char *)data, CALLBACK_MAGIC))
		{
			callback = 1;
			dest = ast_strdupa((char *)data + strlen(CALLBACK_MAGIC));
			}
	    else
			dest = ast_strdupa((char *)data);
	} else {
		ast_log(LOG_WARNING, "Channel requested with no data\n");
		return NULL;
	}
	if (toupper(dest[0]) == 'G' || toupper(dest[0]) == 'R') {
		/* Retrieve the group number */
		char *stringp = NULL;
		stringp = dest + 1;
		s = strsep(&stringp, "/");
		if ((res = sscanf(s, "%d%c%d", &x, &opt, &y)) < 1) {
			ast_log(LOG_WARNING, "Unable to determine group for data %s\n", (char *)data);
			return NULL;
		}
		groupmatch = 1 << x;
		if (toupper(dest[0]) == 'G') {
			if (dest[0] == 'G') {
				backwards = 1;
				p = ifend;
			} else
				p = iflist;
		} else {
			if (dest[0] == 'R') {
				backwards = 1;
				p = round_robin[x]?round_robin[x]->prev:ifend;
				if (!p)
					p = ifend;
			} else {
				p = round_robin[x]?round_robin[x]->next:iflist;
				if (!p)
					p = iflist;
			}
			roundrobin = 1;
		}
	} else {
		char *stringp = NULL;
		stringp = dest;
		s = strsep(&stringp, "/");
		p = iflist;
		if ((res = sscanf(s, "%d%c%d", &x, &opt, &y)) < 1) {
			ast_log(LOG_WARNING, "Unable to determine channel for data %s\n", (char *)data);
			return NULL;
		} else {
			channelmatch = x;
		}
	}
	/* Search for an unowned channel */
	if (ast_mutex_lock(lock)) {
		ast_log(LOG_ERROR, "Unable to lock interface list???\n");
		return NULL;
	}
	exit = p;
	while(p && !tmp) {
		if (roundrobin)
			round_robin[x] = p;

		if (p && available(p, channelmatch, groupmatch, &busy, &channelmatched, &groupmatched, callback)) {
			if (option_debug)
				ast_log(LOG_DEBUG, "Using channel %d\n", p->channel);

			callwait = (p->owner != NULL);
			p->outgoing = 1;

			/* don't process callback request on active call */
			
			if (callback && p->owner)
			{
			    /* OFF hook, but not in any other activity */
			    if (p->owner->_state == AST_STATE_RESERVED &&
					p->play_tone == p->dialtone)
			    {
					ast_set_flag(p->owner, AST_FLAG_OFFHOOK);
					tmp = p->owner;
					break;
			    }
			    else
			    {
					*cause = AST_CAUSE_BUSY;
					break;
			    }			    
			}
			tmp = jdsp_new(p, AST_STATE_RESERVED, 0, p->owner ? SUB_CALLWAIT : SUB_REAL, 0);
			/* Make special notes */
			if (res > 1) {
				if (opt == 'c') {
					/* Confirm answer */
					p->confirmanswer = 1;
				} else if (opt == 'r') {
					/* Distinctive ring */
					if (res < 3)
						ast_log(LOG_WARNING, "Distinctive ring missing identifier in '%s'\n", (char *)data);
					else
						p->distinctivering = y;
				} else {
					ast_log(LOG_WARNING, "Unknown option '%c' in '%s'\n", opt, (char *)data);
				}
			}
			/* Note if the call is a call waiting call */
			if (tmp && callwait)
				tmp->cdrflags |= AST_CDR_CALLWAIT;
			break;
		}
		if (backwards) {
			p = p->prev;
			if (!p)
				p = end;
		} else {
			p = p->next;
			if (!p)
				p = start;
		}
		/* stop when you roll to the one that we started from */
		if (p == exit)
			break;
	}
	ast_mutex_unlock(lock);
	restart_monitor();
	if (callwait)
		*cause = AST_CAUSE_BUSY;
 	else if (!tmp) {
 		if (channelmatched) {
 			if (busy)
 				*cause = AST_CAUSE_BUSY;
 		} else if (groupmatched) {
 			*cause = AST_CAUSE_CONGESTION;
 		}
 	}

	/* if has new channel for callback, set callback flag */
	if (tmp && callback)
		ast_set_flag(tmp, AST_FLAG_CALLBACK);

	return tmp;
}

static int jdsp_destroy_channel(int fd, int argc, char **argv)
{
	int channel = 0;
	struct jdsp_pvt *tmp = NULL;
	struct jdsp_pvt *prev = NULL;
	
	if (argc != 4) {
		return RESULT_SHOWUSAGE;
	}
	channel = atoi(argv[3]);

	tmp = iflist;
	while (tmp) {
		if (tmp->channel == channel) {
			destroy_channel(prev, tmp, 1);
			return RESULT_SUCCESS;
		}
		prev = tmp;
		tmp = tmp->next;
	}
	return RESULT_FAILURE;
}

static int jdsp_play_tone_cmd(int fd, int argc, char **argv)
{
	int channel;
	int tone, duration = 0;
	struct jdsp_pvt *tmp = NULL;
	tone_param_t param;

	if (argc != 5 && argc != 6)
		return RESULT_SHOWUSAGE;
	channel = atoi(argv[3]);
	tone = atoi(argv[4]);

	if (argc == 6)
	    duration = atoi(argv[5]);

	/* Don't allow sleeping too long */
        if (duration < 0 || duration > MAX_TONE_DURATION)
	{
            ast_cli(fd, "Duration is incorrect, must be in range [0,%d] sec\n",
		MAX_TONE_DURATION);
	    return RESULT_FAILURE;
	}

	for (tmp = iflist; tmp && tmp->channel != channel; tmp = tmp->next);
	if (!tmp)
		return RESULT_FAILURE;

	param.direction = TONE_DIRECTION_LOCAL;
	param.tone = tone;
	ioctl(tmp->jfd, VOIP_LINE_TONE, &param);
	if (!duration)
	    return RESULT_SUCCESS;
	sleep(duration);
	param.tone = PHONE_TONE_NONE;
	ioctl(tmp->jfd, VOIP_LINE_TONE, &param);

	return RESULT_SUCCESS;
}
static void *jdsp_ring_thread(void *data)
{
	struct jdsp_ring_data_t *ring_data = data;
	struct jdsp_pvt *tmp = NULL;
	call_params_t params;
	
	ast_mutex_lock(&iflock);
	for (tmp = iflist; tmp && tmp->channel != ring_data->channel; 
		tmp = tmp->next);
	ast_mutex_unlock(&iflock);

	if (!tmp)
		goto Exit;

	for (; ring_data->retries > 0 && tmp->owner; ring_data->retries--)
		sleep(RING_RETRY_DURATION);
	
	ast_mutex_lock(&tmp->lock);
	if (tmp->owner)
	{
		ast_mutex_unlock(&tmp->lock);
		goto Exit;
	}
		
    memset(&params, 0, sizeof(call_params_t)); /* set all params to default */
	
    ioctl(tmp->jfd, VOIP_SLIC_RING, &params);
	ast_mutex_unlock(&tmp->lock);
	
	sleep(ring_data->duration);

	ast_mutex_lock(&tmp->lock);
	if (!tmp->owner)
		ioctl(tmp->jfd, VOIP_SLIC_RING, NULL);
	ast_mutex_unlock(&tmp->lock);

Exit:
	free (ring_data);
	return NULL;
}
static int jdsp_ring_cmd(int fd, int argc, char **argv)
{
	pthread_t threadid;
	struct jdsp_ring_data_t *ring_data;
	pthread_attr_t attr;
	
	if (argc != 4 && argc != 5)
		return RESULT_SHOWUSAGE;
	ring_data = malloc (sizeof(struct jdsp_ring_data_t));
	ring_data->channel = atoi(argv[2]);
    ring_data->duration = atoi(argv[3]);
	ring_data->retries = 0;

	if (argc == 5)
		ring_data->retries = atoi(argv[4]);

	/* Don't allow sleeping too long */
    if (ring_data->duration <= 0 || ring_data->duration > MAX_RING_DURATION)
	{
       ast_cli(fd, "Duration is incorrect, must be in range [1,%d] sec\n",
				MAX_RING_DURATION);
	    return RESULT_FAILURE;
	}

	/* Don't allow sleeping too much retries */	
	if (ring_data->retries < 0 || ring_data->retries > MAX_RING_RETRY_TIMES)
	{
       ast_cli(fd, "Retry value is incorrect, must be in range [0,%d]\n",
				MAX_RING_RETRY_TIMES);
	    goto Exit;
	}

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	if (ast_pthread_create(&threadid, &attr, jdsp_ring_thread, ring_data))
	{
		ast_log(LOG_ERROR, "Unable to start ring thread.\n");
	    goto Exit;
	}

	return RESULT_SUCCESS;
Exit:
	free (ring_data);
	return RESULT_FAILURE;
}

static int jdsp_show_tone(int fd, int argc, char **argv)
{
	int channel;
	struct jdsp_pvt *tmp = NULL;
	char *res = "ok";

	if (argc != 4)
		return RESULT_SHOWUSAGE;
	channel = atoi(argv[3]);
	for (tmp = iflist; tmp && tmp->channel != channel; tmp = tmp->next);
	if (!tmp)
		return RESULT_FAILURE;

    if (tmp->tone_timeout_id > -1)
		res = "off hook warn";
	else if ((tmp->play_tone == PHONE_TONE_RING) || (tmp->owner && 
		tmp->owner->_state == AST_STATE_RINGING))
		res = "dial";
	else if (tmp->play_tone == PHONE_TONE_NONE)
		res = "silence";

    ast_cli(fd, "channel %d: %s tone %d\n", tmp->channel, res,
		tmp->play_tone);

	return RESULT_SUCCESS;
}

static int jdsp_show_channels(int fd, int argc, char **argv)
{
#define FORMAT "%7s %-10.10s %-15.15s %-10.10s %-20.20s\n"
#define FORMAT2 "%7s %-10.10s %-15.15s %-10.10s %-20.20s\n"
	struct jdsp_pvt *tmp = NULL;
	char tmps[20] = "";
	ast_mutex_t *lock;
	struct jdsp_pvt *start;

	lock = &iflock;
	start = iflist;

	if (argc != 3)
		return RESULT_SHOWUSAGE;

	ast_mutex_lock(lock);
	
	tmp = start;
	while (tmp) {
		snprintf(tmps, sizeof(tmps), "%d", tmp->channel);
		ast_cli(fd, FORMAT, tmps, tmp->exten, tmp->context, tmp->language, tmp->musicclass);
		tmp = tmp->next;
	}
	ast_mutex_unlock(lock);
	return RESULT_SUCCESS;
#undef FORMAT
#undef FORMAT2
}

static int jdsp_show_hookstate(int fd, int argc, char **argv)
{
	struct jdsp_pvt *tmp = NULL;
	int isoffhook;

	if (argc != 3)
		return RESULT_SHOWUSAGE;
	ast_mutex_lock(&iflock);
	for (tmp = iflist; tmp; tmp = tmp->next)
	{
		if (ioctl(tmp->jfd, VOIP_SLIC_GET_HOOK, &isoffhook) < 0)
			return RESULT_FAILURE;
		ast_cli(fd, "line %d: %s hook\n", tmp->channel, isoffhook ? "off" : "on");
	}
	ast_mutex_unlock(&iflock);

	return RESULT_SUCCESS;
}

static char show_channels_usage[] =
	"Usage: jdsp show channels\n"
	"	Shows a list of available channels\n";

static char destroy_channel_usage[] =
	"Usage: jdsp destroy channel <chan num>\n"
	"	DON'T USE THIS UNLESS YOU KNOW WHAT YOU ARE DOING.  Immediately removes a given channel, whether it is in use or not\n";

static char play_tone_usage[] =
	"Usage: jdsp play tone <chan_num> <tone_num> [<duration>]\n"
	"       Use tone_num = 0 to stop playing tone\n";

static char show_hookstate_usage[] =
	"Usage: jdsp show hookstate\n";

static char jdsp_answer_call_waiting_usage[] =
	"Usage: jdsp answer call_waiting <chan_num> [r] [0-9]\n";

static char jdsp_show_tone_usage[] =
	"Usage: jdsp show tone <chan_num>\n";

static char	jdsp_show_call_matrix_usage[] = 
	"Usage: jdsp show call_matrix <chan_num>\n";

static char ring_usage[] =
	"Usage: jdsp ring <chan_num> <duration in seconds> [<retries>]\n";

static char get_attach_state_usage[] =
	"Usage: jdsp get_attach_state <chan_num>\n";

static struct ast_cli_entry jdsp_cli[] = {
    	{ {"jdsp", "show", "channels", NULL}, jdsp_show_channels,
	  "Show active jdsp channels", show_channels_usage },
	{ {"jdsp", "destroy", "channel", NULL}, jdsp_destroy_channel,
	  "Destroy a channel", destroy_channel_usage },
	{ {"jdsp", "play", "tone", NULL}, jdsp_play_tone_cmd,
	  "Play a tone", play_tone_usage },
	{ {"jdsp", "show", "hookstate", NULL}, jdsp_show_hookstate,
	  "Show hook state of all channels", show_hookstate_usage },
	{ {"jdsp", "answer", "call_waiting", NULL}, jdsp_answer_call_waiting,
	  "Answer call waiting", jdsp_answer_call_waiting_usage },
	{ {"jdsp", "show", "tone", NULL}, jdsp_show_tone,
	  "Display info about the tone played now", jdsp_show_tone_usage},
	{ {"jdsp", "show", "call_matrix", NULL}, jdsp_show_call_matrix,
	  "Display current call matrix", jdsp_show_call_matrix_usage},
	{ {"jdsp", "ring", NULL}, jdsp_ring_cmd, "Ring", ring_usage },
	{ {"jdsp", "get_attach_state", NULL}, jdsp_get_attach_state_cmd, "Checks "
		"if a phone is attached to the fxs port", get_attach_state_usage },
};

#define TRANSFER	0
#define HANGUP		1

static int __unload_module(void)
{
	int x = 0;
	struct jdsp_pvt *p, *pl;

	memset(conf_file_md5, 0, MD5_DIGEST_LEN);

	ast_cli_unregister_multiple(jdsp_cli, sizeof(jdsp_cli) / sizeof(jdsp_cli[0]));
	ast_manager_unregister( "JDSPDialOffhook" );
	ast_manager_unregister( "JDSPHangup" );
	ast_manager_unregister( "JDSPTransfer" );
	ast_manager_unregister( "JDSPDNDoff" );
	ast_manager_unregister( "JDSPDNDon" );
	ast_manager_unregister("JDSPShowChannels");
	ast_manager_unregister("JDSPpvtgetstats");
	ast_manager_unregister("JDSPpvtresetstats");
	ast_channel_unregister(&jdsp_tech);
	if (!ast_mutex_lock(&iflock)) {
		/* Hangup all interfaces if they have an owner */
		p = iflist;
		while(p) {
			if (p->owner)
				ast_softhangup(p->owner, AST_SOFTHANGUP_APPUNLOAD);
			p = p->next;
		}
		ast_mutex_unlock(&iflock);
	} else {
		ast_log(LOG_WARNING, "Unable to lock the monitor\n");
		return -1;
	}
	if (!ast_mutex_lock(&monlock)) {
		if (monitor_thread && (monitor_thread != AST_PTHREADT_STOP) && (monitor_thread != AST_PTHREADT_NULL)) {
			pthread_cancel(monitor_thread);
			pthread_kill(monitor_thread, SIGURG);
			pthread_join(monitor_thread, NULL);
		}
		monitor_thread = AST_PTHREADT_STOP;
		ast_mutex_unlock(&monlock);
	} else {
		ast_log(LOG_WARNING, "Unable to lock the monitor\n");
		return -1;
	}

	if (!ast_mutex_lock(&iflock)) {
		/* Destroy all the interfaces and free their memory */
		p = iflist;
		while(p) {
			/* Close the jdsp thingy */
			if (p->jfd > -1)
				jdsp_close(p->jfd);
			if (p->io_pipe[0] > -1)
				jdsp_close(p->io_pipe[0]);
			if (p->io_pipe[1] > -1)
				jdsp_close(p->io_pipe[1]);

			if (p->tone_timeout_id > -1)
				ast_sched_del(sched, p->tone_timeout_id);

			if (p->modem_timer_sched_id > -1)
				ast_sched_del(sched, p->modem_timer_sched_id);

			pl = p;
			p = p->next;
			x++;
			/* Free associated memory */
			if(pl)
				destroy_jdsp_pvt(&pl);
			ast_verbose(VERBOSE_PREFIX_3 "Unregistered channel %d\n", x);
		}
		iflist = NULL;
		ifcount = 0;
		ast_mutex_unlock(&iflock);
	} else {
		ast_log(LOG_WARNING, "Unable to lock the monitor\n");
		return -1;
	}
	sched_context_destroy(sched);
	sched = NULL;
	return 0;
}

int unload_module()
{
	return __unload_module();
}
		
static mwi_type_t parse_mwi_type(char *value)
{
	if (!strcasecmp(value, "none"))
		return MWI_OFF;
	else if (!strcasecmp(value, "external_per_line"))
		return MWI_EXTERNAL_PER_LINE;
	else if (!strcasecmp(value, "external_global"))
		return MWI_EXTERNAL_GLOBAL;
	else if (!strcasecmp(value, "internal"))
		return MWI_INTERNAL;

	ast_log(LOG_WARNING, "Unknown mwi type '%s'\n", value);
	return MWI_OFF;
}

static int parse_heavyweight_codecs(const char *value)
{
	char *list = strdup(value);
	char *format;
	int formats = 0;

	for (format = strtok(list, ","); format; format = strtok(NULL, ","))
		formats |= ast_getformatbyname(format);

	free(list);
	return formats;
}

static int setup_jdsp(int reload, int check_conf_file)
{
	struct ast_config *cfg;
	struct ast_variable *v;
	struct jdsp_pvt *tmp;
	char *chan;
	char *c;
	int start, finish,x, dsp_line_number = -1;

	if (!ast_config_file_md5_update(config, conf_file_md5) && reload && check_conf_file)
	{
		ast_log(LOG_DEBUG, "Skipping reload since %s was not changed\n", config);
		return 0;
	}

	cfg = ast_config_load(config);

	/* We *must* have a config file otherwise stop immediately */
	if (!cfg) {
		ast_log(LOG_ERROR, "Unable to load config %s\n", config);
		return -1;
	}
	
	memset(&global_native_formats, 0 , sizeof(global_native_formats));
	global_preferred_codec = 0;

	if (ast_mutex_lock(&iflock)) {
		/* It's a little silly to lock it, but we mind as well just to be sure */
		ast_log(LOG_ERROR, "Unable to lock interface list???\n");
		return -1;
	}

	/*General params*/
	*diag_ivr_activation_seq = '\0';
	v = ast_variable_browse(cfg, "general");
	while(v) {
		if (!strcasecmp(v->name, "localdetecttimeout"))
			faxmodem_local_detect_timeout = atoi(v->value);
		else if(!strcasecmp(v->name, "netdetecttimeout"))
			faxmodem_net_detect_timeout = atoi(v->value);
		else if(!strcasecmp(v->name, "diag_ivr_activation_seq"))
		{
			ast_copy_string(diag_ivr_activation_seq, v->value,
			    sizeof(diag_ivr_activation_seq));
                }
		v = v->next;
	}

	v = ast_variable_browse(cfg, "channels");
	while(v) {
		/* Create the interface list */
		if (!strcasecmp(v->name, "channel")
					) {
			if (reload == 0) {
				if (cur_signalling < 0) {
					ast_log(LOG_ERROR, "Signalling must be specified before any channels are.\n");
					ast_config_destroy(cfg);
					ast_mutex_unlock(&iflock);
					return -1;
				}
			}
			c = v->value;

			chan = strsep(&c, ",");
			while(chan) {
				if (sscanf(chan, "%d-%d", &start, &finish) == 2) {
					/* Range */
				} else if (sscanf(chan, "%d", &start)) {
					/* Just one */
					finish = start;
				} else {
					ast_log(LOG_ERROR, "Syntax error parsing '%s' at '%s'\n", v->value, chan);
					ast_config_destroy(cfg);
					ast_mutex_unlock(&iflock);
					return -1;
				}
				if (finish < start) {
					ast_log(LOG_WARNING, "Sillyness: %d < %d\n", start, finish);
					x = finish;
					finish = start;
					start = x;
				}
				for (x = start; x <= finish; x++) {
					tmp = mkintf(x, dsp_line_number == -1 ? x : dsp_line_number, cur_signalling, reload);

					if (tmp) {
						if (option_verbose > 2) {
								ast_verbose(VERBOSE_PREFIX_3 "%s channel %d, %s signalling\n", reload ? "Reconfigured" : "Registered", x, sig2str(tmp->sig));
						}
					} else {
						if (reload == 1)
							ast_log(LOG_ERROR, "Unable to reconfigure channel '%s'\n", v->value);
						else
							ast_log(LOG_ERROR, "Unable to register channel '%s'\n", v->value);
						ast_config_destroy(cfg);
						ast_mutex_unlock(&iflock);
						return -1;
					}
				}
				chan = strsep(&c, ",");
			}
		} else if (!strcasecmp(v->name, "enabled")) {
			enabled = ast_true(v->value);
		} else if (!strcasecmp(v->name, "usecallerid")) {
			use_callerid = ast_true(v->value);
		} else if (!strcasecmp(v->name, "cidsignalling")) {
			if (!strcasecmp(v->value, "bell"))
				cid_signalling = CID_SIG_BELL;
			else if (!strcasecmp(v->value, "v23"))
				cid_signalling = CID_SIG_V23;
			else if (!strcasecmp(v->value, "dtmf"))
				cid_signalling = CID_SIG_DTMF;
			else if (ast_true(v->value))
				cid_signalling = CID_SIG_BELL;
		} else if (!strcasecmp(v->name, "threewaycalling")) {
			threewaycalling = ast_true(v->value);
		} else if (!strcasecmp(v->name, "threewayconference")) {
			threewayconference = ast_true(v->value);
		} else if (!strcasecmp(v->name, "dialtone")) {
		        dialtone = jdsp_get_dialtone_from_str(v->value);
		} else if (!strcasecmp(v->name, "mwi")) {
			mwi = parse_mwi_type(v->value);
		} else if (!strcasecmp(v->name, "vmwi")) {
			vmwi = parse_mwi_type(v->value);
		} else if (!strcasecmp(v->name, "fwd_disconnect")) {
			fwd_disconnect = ast_true(v->value) ? FWD_DISCONNECT_ON :
				FWD_DISCONNECT_OFF;
		} else if (!strcasecmp(v->name, "allow")) {
			ast_parse_allow_disallow(&global_native_formats, NULL, v->value, 1);
		} else if (!strcasecmp(v->name, "disallow")) {
			ast_parse_allow_disallow(&global_native_formats, NULL, v->value, 0);
		} else if (!strcasecmp(v->name, "cfwd_unconditional_activate_code")) {
			ast_copy_string(cfwd_unconditional_activate_code, v->value, sizeof(cfwd_unconditional_activate_code));
		} else if (!strcasecmp(v->name, "cfwd_unconditional_deactivate_code")) {
			ast_copy_string(cfwd_unconditional_deactivate_code, v->value, sizeof(cfwd_unconditional_deactivate_code));
		} else if (!strcasecmp(v->name, "cfwd_busy_activate_code")) {
			ast_copy_string(cfwd_busy_activate_code, v->value, sizeof(cfwd_busy_activate_code));
		} else if (!strcasecmp(v->name, "cfwd_busy_deactivate_code")) {
			ast_copy_string(cfwd_busy_deactivate_code, v->value, sizeof(cfwd_busy_deactivate_code));
		} else if (!strcasecmp(v->name, "cfwd_no_answer_activate_code")) {
			ast_copy_string(cfwd_no_answer_activate_code, v->value, sizeof(cfwd_no_answer_activate_code));
		} else if (!strcasecmp(v->name, "cfwd_no_answer_deactivate_code")) {
			ast_copy_string(cfwd_no_answer_deactivate_code, v->value, sizeof(cfwd_no_answer_deactivate_code));
		} else if (!strcasecmp(v->name, "dnd_activate_code")) {
			ast_copy_string(dnd_activate_code, v->value, sizeof(dnd_activate_code));
		} else if (!strcasecmp(v->name, "dnd_deactivate_code")) {
			ast_copy_string(dnd_deactivate_code, v->value, sizeof(dnd_deactivate_code));
		} else if (!strcasecmp(v->name, "use_caller_variable")) {
			use_caller_variable = ast_true(v->value);
		} else if (!strcasecmp(v->name, "mailbox")) {
			ast_copy_string(mailbox, v->value, sizeof(mailbox));
		} else if (!strcasecmp(v->name, "faxtxmethod")) {
			faxtxmethod = jdsp_fax_method_parse(v->value);
		} else if (!strcasecmp(v->name, "silencesuppressionenabled")) {
			silence_suppression_enabled = ast_true(v->value);
		} else if (!strcasecmp(v->name, "detectionmethod")) {
			detectionmethod = jdsp_fax_detection_method_parse(v->value);
		} else if (!strcasecmp(v->name, "discriminatecall")) {
			discriminate_call = ast_true(v->value);
		} else if (!strcasecmp(v->name, "adsi")) {
			adsi = ast_true(v->value);
		} else if (!strcasecmp(v->name, "transfermode")) {
			if (!strcasecmp(v->value, "off"))
			    	transfermode = TRANSFER_OFF;
			else if (!strcasecmp(v->value, "signalling"))
			    	transfermode = TRANSFER_SIGNALLING;
			else if (!strcasecmp(v->value, "bridging"))
			    	transfermode = TRANSFER_BRIDGING;
			else
			    	ast_log(LOG_WARNING, "Unknown transfer mode '%s'\n", v->value);
		} else if (!strcasecmp(v->name, "canpark")) {
			canpark = ast_true(v->value);
		} else if (!strcasecmp(v->name, "callprogress")) {
			if (ast_true(v->value))
				callprogress |= 1;
			else
				callprogress &= ~1;
		} else if (!strcasecmp(v->name, "faxdetect")) {
			if (!strcasecmp(v->value, "incoming")) {
				callprogress |= 4;
				callprogress &= ~2;
			} else if (!strcasecmp(v->value, "outgoing")) {
				callprogress &= ~4;
				callprogress |= 2;
			} else if (!strcasecmp(v->value, "both") || ast_true(v->value))
				callprogress |= 6;
			else
				callprogress &= ~6;
		} else if (!strcasecmp(v->name, "hidecallerid")) {
			hidecallerid = ast_true(v->value);
		} else if (!strcasecmp(v->name, "callreturn")) {
			callreturn = ast_true(v->value);
		} else if (!strcasecmp(v->name, "callwaiting")) {
			callwaiting = ast_true(v->value);
		} else if (!strcasecmp(v->name, "callwaitingcallerid")) {
			callwaitingcallerid = ast_true(v->value);
		} else if (!strcasecmp(v->name, "context")) {
			ast_copy_string(context, v->value, sizeof(context));
		} else if (!strcasecmp(v->name, "language")) {
			ast_copy_string(language, v->value, sizeof(language));
		} else if (!strcasecmp(v->name, "progzone")) {
			ast_copy_string(progzone, v->value, sizeof(progzone));
		} else if (!strcasecmp(v->name, "musiconhold")) {
			ast_copy_string(musicclass, v->value, sizeof(musicclass));
		} else if (!strcasecmp(v->name, "internalmoh")) {
			internalmoh = ast_true(v->value);
		} else if (!strcasecmp(v->name, "group")) {
			cur_group = ast_get_group(v->value);
		} else if (!strcasecmp(v->name, "callgroup")) {
			cur_callergroup = ast_get_group(v->value);
		} else if (!strcasecmp(v->name, "pickupgroup")) {
			cur_pickupgroup = ast_get_group(v->value);
		} else if (!strcasecmp(v->name, "immediate")) {
			immediate = ast_true(v->value);
		} else if (!strcasecmp(v->name, "automatic_call")) {
			automatic_call = ast_true(v->value);			
 		} else if (!strcasecmp(v->name, "transfertobusy")) {
 			transfertobusy = ast_true(v->value);
		} else if (!strcasecmp(v->name, "internal_transfer_only")) {
 			internal_transfer_only = ast_true(v->value);
		} 
		else if (!strcasecmp(v->name, "dead_line_tone_support")) {
		        dead_line_tone_support = ast_true(v->value); 
		} else if (!strcasecmp(v->name, "rxgain")) {
			if (sscanf(v->value, "%f", &rxgain) != 1) {
				ast_log(LOG_WARNING, "Invalid rxgain: %s\n", v->value);
			}
		} else if (!strcasecmp(v->name, "txgain")) {
			if (sscanf(v->value, "%f", &txgain) != 1) {
				ast_log(LOG_WARNING, "Invalid txgain: %s\n", v->value);
			}
		} else if (!strcasecmp(v->name, "tonezone")) {
			if (sscanf(v->value, "%d", &tonezone) != 1) {
				ast_log(LOG_WARNING, "Invalid tonezone: %s\n", v->value);
			}
		} else if (!strcasecmp(v->name, "callerid")) {
			if (!strcasecmp(v->value, "asreceived")) {
				cid_num[0] = '\0';
				cid_name[0] = '\0';
			} else {
				ast_callerid_split(v->value, cid_name, sizeof(cid_name), cid_num, sizeof(cid_num));
			}
		} else if (!strcasecmp(v->name, "useincomingcalleridonjdsptransfer")) {
			jdsptrcallerid = ast_true(v->value);
		} else if (!strcasecmp(v->name, "accountcode")) {
			ast_copy_string(accountcode, v->value, sizeof(accountcode));
		} else if (!strcasecmp(v->name, "dialingtimeout")) {
			matchdigittimeout = atoi(v->value);
		} else if (!strcasecmp(v->name, "interdigittimeout")) {
			gendigittimeout = atoi(v->value);
		} else if (!strcasecmp(v->name, "autocalltimeout")) {
			autocall_timeout = atoi(v->value);
		} else if (!strcasecmp(v->name, "firstdigittimeout")) {
			firstdigittimeout = atoi(v->value);
		} else if (!strcasecmp(v->name, "deflaw")) {
		        if (!strcasecmp(v->value, "ulaw"))
		            deflaw = AST_FORMAT_ULAW;
			else if (!strcasecmp(v->value, "alaw"))
			    deflaw = AST_FORMAT_ALAW;
		} else if (!strcasecmp(v->name, "isnon3gsecondcallsallowed")) {
		    is_non3g_second_callsallowed = atoi(v->value);
		} else  if (!strcasecmp(v->name, "signalling")) {
			if(!reload){
				if (!strcasecmp(v->value, "fxo_ls")) {
					cur_signalling = SIG_FXOLS;
				} else if (!strcasecmp(v->value, "fxo_gs")) {
					cur_signalling = SIG_FXOGS;
				} else {
					ast_log(LOG_ERROR, "Unknown signalling method '%s'\n", v->value);
				}
			} 
		} else if (!strcasecmp(v->name, "use_hold_tone")) {
			use_hold_tone = ast_true(v->value);
		} else if (!strcasecmp(v->name, "stop_immediatly_pound")) {
			stop_immediatly_pound = ast_true(v->value);
		} else if (!strcasecmp(v->name, "dsp_line_number")) {
			dsp_line_number = atoi(v->value);
		} else if (!strcasecmp(v->name, "heavyweight_codecs")) {
			heavyweight_codecs = parse_heavyweight_codecs(v->value);
		} else
			ast_log(LOG_WARNING, "Ignoring %s\n", v->name);
		v = v->next;
	}

	jdsp_free_exten_num_list(exten_num_lst);
	v = ast_variable_browse(cfg, "extension-numbers");
        exten_num_lst = jdsp_create_exten_num_list_from_conf(v);

	jdsp_free_out_call_prefix_list(out_call_prefix_lst);
	v = ast_variable_browse(cfg, "out-call-prefixes");
	out_call_prefix_lst = jdsp_create_out_call_prefix_list_from_conf(v);

	ast_mutex_unlock(&iflock);
	ast_config_destroy(cfg);
	/* And start the monitor for the first time */
	restart_monitor();
	return 0;
}

int load_module(void)
{
	int res;

	sched = sched_context_create();
	if (!sched) {
		ast_log(LOG_ERROR, "Unable to create schedule context\n");
		return -1;
	}

	res = setup_jdsp(0, 0);
	/* Make sure we can register our JDSP channel type */
	if(res) {
	  return -1;
	}
	if (ast_channel_register(&jdsp_tech)) {
		ast_log(LOG_ERROR, "Unable to register channel class %s\n", type);
		__unload_module();
		return -1;
	}
	ast_cli_register_multiple(jdsp_cli, sizeof(jdsp_cli) / sizeof(jdsp_cli[0]));

	/* Register manager commands */
	ast_manager_register2("JDSPpvtgetstats", EVENT_FLAG_SYSTEM,
		manager_jdsp_get_stats, "Get Packet Statistics", NULL);
	ast_manager_register2("JDSPpvtresetstats", EVENT_FLAG_SYSTEM,
		manager_jdsp_reset_stats, "Reset Packet Statistics", NULL);
	ast_manager_register2("JDSPpvtgetattachstate", EVENT_FLAG_SYSTEM,
		manager_jdsp_get_attach_state, "Get FXS port attachment state", NULL);
	
	memset(round_robin, 0, sizeof(round_robin));
	return res;
}

static int _reload(int check_conf_file)
{
	int res = 0;

	res = setup_jdsp(1, check_conf_file);
	if (res) {
		ast_log(LOG_WARNING, "Reload of chan_jdsp.so is unsuccessful!\n");
		return -1;
	}
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
	return usecnt;
}

char *description()
{
	return (char *) desc;
}

char *key()
{
	return ASTERISK_GPL_KEY;
}

static void jdsp_pre_bridge(struct ast_channel *chan)
{
	struct jdsp_pvt *p = chan->tech_pvt;
	int index;

	ast_log(LOG_DEBUG, "Started jdsp pre_bridge\n");

	ast_mutex_lock(&p->lock);
	index = jdsp_get_index(chan, p, 0);
	if (index == SUB_REAL || p->subs[index].inthreeway)
		jdsp_start_audio(chan, index);
	ast_mutex_unlock(&p->lock);
}
