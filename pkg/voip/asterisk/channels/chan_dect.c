/****************************************************************************
 *
 * rg/pkg/voip/asterisk/channels/chan_dect.c
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
 * DECT abstraction layer interface
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

ASTERISK_FILE_VERSION(__FILE__, "$Revision: 1.1.8.1.2.12 $")

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
#include "asterisk/jdsp_common.h"
#include "dect_cc.h"

#include <util/openrg_gpl.h>
#include <voip/dsp/phone.h>
#include <kos_chardev_id.h>
#include <rg_ioctl.h>

    /* XXX Must be configurable */
#define NATIVE_FORMATS ((AST_FORMAT_MAX_AUDIO << 1) - 1)

    static const char desc[] = "Jungo DECT Abstraction Layer";

    static const char tdesc[] = "Jungo DECT Abstraction Layer Driver";

    static const char type[] = "dect";
    static const char config[] = "dect.conf";

    /* The time between the fwd_disconnect onhook and offhook is about 1 second.
     * We take 2 seconds timeout in order to be on the safe side. */
#define FWD_DISCONNECT_TIMEOUT 2000

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


typedef enum {
    TRANSFER_OFF = 0,
    TRANSFER_SIGNALLING = 1,
    TRANSFER_BRIDGING = 2,
} transfermode_t;

typedef enum {
    FAX_TX_NONE = 0,
    FAX_TX_T38_AUTO = 1,
    FAX_TX_PASSTHROUGH_AUTO = 2,
    FAX_TX_PASSTHROUGH_FORCE = 3,
} faxtxmethod_t;

static unsigned char conf_file_md5[MD5_DIGEST_LEN];

static struct ast_codec_pref global_native_formats;

static char context[AST_MAX_CONTEXT] = "default";
static char cid_num[256] = "";
static char cid_name[256] = "";

static char language[MAX_LANGUAGE] = "";
static char musicclass[MAX_MUSICCLASS] = "";
static char progzone[10]= "";

static int transfertobusy = 1;

static int use_callerid = 1;
static int cid_signalling = CID_SIG_BELL;
static int decttrcallerid = 0;

static ast_group_t cur_group = 0;
static ast_group_t cur_callergroup = 0;
static ast_group_t cur_pickupgroup = 0;

static int enabled = 1;

static int immediate = 0;

static int internalmoh = 1;

static int callwaiting = 1;

static int callwaitingcallerid = 0;

static int hidecallerid = 0;

static int callreturn = 0;

static int threewaycalling = 0;

static int threewayconference = 0;

static int enable_flash_key = 1;

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

static faxtxmethod_t faxtxmethod = FAX_TX_NONE;

/*! \brief Wait up to 20 seconds for first digit (FXO logic) */
static int firstdigittimeout = 20000;

/*! \brief Wait 20 seconds after start playing reorder tone, and before playing
 * off-hook warning tone (FXO logic) */
static int offhookwarningtimeout = 20000;

/*! \brief Play the offhook warning tone for 4 min */
static int stopoffhookwarningtimeout = 240000;

/*! \brief How long to wait for following digits (FXO logic) */
/* Equivalent to the idt (inter digit timer) in RFC 2897 */
/* Similar to InterDigitTimerStd in TR-104 */
static int gendigittimeout = 8000;

/*! \brief How long to wait for an extra digit, if there is an ambiguous match */
/* Also quivalent to the idt (inter digit timer) in RFC 2897 */
/* Similar to InterDigitTimerOpen in TR-104 */
static int matchdigittimeout = 3000;

static int usecnt = 0;
AST_MUTEX_DEFINE_STATIC(usecnt_lock);

/*! \brief Protect the interface list (of dect_pvt's) */
AST_MUTEX_DEFINE_STATIC(iflock);

static int ifcount = 0;

/*! \brief Protect the monitoring thread, so only one process can kill or start it, and not
  when it's doing something critical. */
AST_MUTEX_DEFINE_STATIC(monlock);

/*! \brief This is the thread for the monitor which checks for input on the channels
  which are not currently in use.  */
static pthread_t monitor_thread = AST_PTHREADT_NULL;

static struct sched_context *sched;
static struct io_context *io;           /*!< The IO context */

static int cc_sock = -1;
static int *cc_read_id = NULL;
static int is_cc_init = 0;

static int global_reload_count = 0;

static int restart_monitor(void);

static inline int dect_get_event(int fd, phone_event_t *event)
{
    if (read(fd, event, sizeof(*event)) < 0)
	return -1;
    return 0;
}

#define RTP_HEADER_SIZE 12

/*! Chunk size to read -- we use 20ms chunks to make things happy.  */   
#define READ_SIZE (160 + RTP_HEADER_SIZE)

#define MIN_MS_SINCE_FLASH			( (2000) )	/*!< 2000 ms */

#define CC_RELEASE_TIMEOUT 5000 /*!< 5 seconds */
#define DTMF_DURATION_MS   60
struct dect_pvt;

#define SUB_REAL	0			/*!< Active call */
#define SUB_CALLWAIT	1			/*!< Call-Waiting call on hold */
#define SUB_THREEWAY	2			/*!< Three-way call */

#define MAX_DECT_DSP_LINES cCONFIG_HW_NUMBER_OF_DECT_LINES

static char *subnames[] = {
    "Real",
    "Callwait",
    "Threeway"
};

typedef enum {
    LINE_ALLOC_NONE = 0,
    LINE_ALLOC_SHARED,
    LINE_ALLOC_EXCLUSIVE
} line_alloc_t;

/* DSP interface line. Actually, this is PCM timeslot that is used to
   transfer voice between DECT modem and the DSP.
   Do not confuse with DSP voice channel (dfd)! DSP voice channel is an 
   abstraction that represents actual voice path. One DSP line supports up to
   2 DSP voice channels (2 channels are used in case of conference call). 
 */
struct dect_dsp_line {
    int fd;      /*!< DSP line fd */
    int lnum;    /*!< DSP line number */
    int pcm_timeslot;
    line_alloc_t alloc_status;
    int ref_count;
    char shared_callid[AST_MAX_SHARED_CALLID];

    /* XXX consider implementing it better way*/
    int dc_allocated;
};

struct dect_subchannel {
    int dfd;     /*!< DSP voice channel fd  */
    int chan; 	 /*!< Associated channel in DSP (0/1) */
    struct ast_channel *owner;
    char buffer[AST_FRIENDLY_OFFSET + READ_SIZE];
    struct ast_frame f;		/*!< One frame for each channel.  How did this ever work before? */
    struct ast_rtp *rtp;
    u32 callref;

    unsigned short seqno;
    unsigned long lastts;
    struct timeval txcore;

    unsigned int needanswer:1;
    unsigned int inthreeway:1;
    unsigned int inhold:1;
};

#define CONF_USER_REAL		(1 << 0)
#define CONF_USER_THIRDCALL	(1 << 1)

#define MAX_SLAVES	4

struct dect_dtmf
{
    char dtmf_key;
    int dtmf_sched;
    struct ast_channel *ast;
};

static struct dect_pvt {
    ast_mutex_t lock;
    struct ast_channel *owner;			/*!< Our current active owner (if applicable) */
    /*!< Up to three channels can be associated with this call */

    struct dect_subchannel sub_unused;		/*!< Just a safety precaution */
    struct dect_subchannel subs[3];			/*!< Sub-channels */

    struct dect_dsp_line *line;
    struct dect_pvt *slaves[MAX_SLAVES];		/*!< Slave to us (follows our conferencing) */
    struct dect_pvt *master;			/*!< Master to us (we follow their conferencing) */
    struct dect_dtmf dtmf;
    int inconference;				/*!< If our real should be in the conference */

    float rxgain;
    float txgain;
    int tonezone;					/*!< tone zone for this chan, or -1 for default */
    struct dect_pvt *next;				/*!< Next channel in list */
    struct dect_pvt *prev;				/*!< Prev channel in list */

    /* flags */
    unsigned int adsi:1;
    unsigned int callreturn:1;
    unsigned int callwaiting:1;
    unsigned int callwaitingcallerid:1;
    unsigned int canpark:1;
    unsigned int confirmanswer:1;			/*!< Wait for '#' to confirm answer */
    unsigned int hidecallerid;
    unsigned int immediate:1;			/*!< Answer before getting digits? */
    unsigned int follow_call:1;			/*!< Whether the current call was placed right after a disconnected call (without placing the phone on-hook) */
    unsigned int network_failure_disconnection:1;	/*!< Whether the current call was disconnected as a result of network failure */
    unsigned int outgoing:1;
    unsigned int permcallwaiting:1;
    unsigned int permhidecallerid:1;		/*!< Whether to hide our outgoing caller ID or not */
    unsigned int threewaycalling:1;
    unsigned int threewayconference:1;
    unsigned int use_callerid:1;			/*!< Whether or not to use caller id on this channel */
    unsigned int decttrcallerid:1;			/*!< should we use the callerid from incoming call on dect transfer or not */
    unsigned int transfertobusy:1;			/*!< allow flash-transfers to busy channels */
    unsigned int faxdetected:1;
    unsigned int modemdetected:1;
    unsigned int enabled:1;
    unsigned int next_msg_wait:1;
    unsigned int msg_wait:1;
    unsigned int enable_flash_key:1;

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
    int hs_id;					/*!< Device ID of the DECT handset */
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
    int offhookwarningschedid;
    int play_tone;
    int matchdigittimeout;
    int faxtxmethod;
    transfermode_t transfermode;
    int alloc_count;
    int real_dc_num;
    int release_timer;
    int reload_count;
    int codec; 			   		/*!< which codec is used by dect phone */	

} *iflist = NULL, *ifend = NULL;

struct release_timer_data_t {
    int index;
    struct dect_pvt *pvt;
};

static struct ast_channel *dect_request(const char *type, const struct ast_codec_pref *formats, void *data, int *cause);
static int dect_digit_begin(struct ast_channel *ast, char digit);
static int dect_digit_end(struct ast_channel *ast, char digit);
static int dect_call(struct ast_channel *ast, char *rdest, int timeout);
static int dect_hangup(struct ast_channel *ast);
static int dect_answer(struct ast_channel *ast);
struct ast_frame *dect_read(struct ast_channel *ast);
static int dect_write(struct ast_channel *ast, struct ast_frame *frame);
static int dect_indicate(struct ast_channel *chan, int condition);
static int dect_fixup(struct ast_channel *oldchan, struct ast_channel *newchan);
static void dect_pre_bridge(struct ast_channel *chan);
static void dect_post_bridge(struct ast_channel *chan);
#if 0
static void start_call(struct dect_pvt *i);
#endif

static const struct ast_channel_tech dect_tech = {
    .type = type,
    .description = tdesc,
    .capabilities = NATIVE_FORMATS,
    .requester = dect_request,
    .send_digit_begin = dect_digit_begin,
    .send_digit_end = dect_digit_end,
    .call = dect_call,
    .hangup = dect_hangup,
    .answer = dect_answer,
    .read = dect_read,
    .write = dect_write,
    .indicate = dect_indicate,
    .fixup = dect_fixup,
    .pre_bridge = dect_pre_bridge,
    .post_bridge = dect_post_bridge,
};

static struct dect_pvt *round_robin[32];
static struct dect_dsp_line dsp_line[MAX_DECT_DSP_LINES];

#define CANBUSYDETECT(p) ISTRUNK(p)
#define CANPROGRESSDETECT(p) ISTRUNK(p)

#define OTHER_DC_NUM(num) (((num) == 0) ? 1: 0)

static void dect_stop_audio(struct ast_channel *ast, int index);

static int dect_get_index(struct ast_channel *ast, struct dect_pvt *p, int nullok)
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

/* return -1 if not found */
static int dect_get_index_by_callref(struct dect_pvt *p, u32 cr)
{
    int i;

    if (!cr) /* invalid cr */
	return -1; 

    for (i = 0; i < 3 && p->subs[i].callref != cr; i++);

    return (i == 3) ? -1 : i;
}

static int dect_dc_shared_alloc(struct dect_pvt *p, int chan, char *id)
{
    alloc_params_t params = {{0}};
    int res;

    ast_log(LOG_DEBUG, "hs_id %u: allocating DC on line %d; shared alloc\n", 
	p->hs_id, p->line->lnum);

    strncpy(params.id, id, MAX_ALLOC_ID_LEN);
    params.chan = chan;

    res = ioctl(p->line->fd, VOIP_LINE_DC_ALLOC, &params);
    if (res)
    {
	ast_log(LOG_WARNING, "hs_id %u: failed to allocate shared DC on line %d\n",
	    p->hs_id, p->line->lnum);
    }
    else
	p->line->dc_allocated = 1;

    return res;
}

#define dect_dc_exclusive_alloc(p, chan, lame) \
    _dect_dc_exclusive_alloc(p, chan, lame, __LINE__)

static int _dect_dc_exclusive_alloc(struct dect_pvt *p, int chan, int lame, 
    int line)
{
    alloc_params_t params = {{0}};
    int res;

    ast_log(LOG_DEBUG, "hs_id %u: allocating DC on line %d; exclusive alloc\n",
	p->hs_id, p->line->lnum);

    params.chan = chan;
    params.flags = lame ? DC_ALLOC_FLAG_LAME : 0;

    if ((res = ioctl(p->line->fd, VOIP_LINE_DC_ALLOC, &params)))
    {
	ast_log(LOG_WARNING, "(%d) hs_id %u: failed to allocate exclusive %s DC "
	    "on line %d\n", line, p->hs_id, lame ? "lame" : "", 
	    p->line->lnum);

    }
    else
    {
	p->line->dc_allocated = 1;
	p->alloc_count++;
	ast_log(LOG_DEBUG, "(%d) hs_id %u: alloc count for line %d incremented to %d\n", line,
	    p->hs_id, p->line->lnum, p->alloc_count);
    }

    return res;
}


/* Free data channel */
#define dect_dc_free(p, chan) _dect_dc_free(p, chan, __LINE__)
static int _dect_dc_free(struct dect_pvt *p, int chan, int line)
{
    int ret;

    if (!p->line->dc_allocated)
	return 0;

    if (chan > 1 || chan < 0)
    {
	ast_log(LOG_ERROR, "(%d) hs_id %u: freeing DC on line %d with invalid channel %d\n",
	    line, p->hs_id, p->line->lnum, chan);
	return -1;
    }

    ret = ioctl(p->line->fd, VOIP_LINE_DC_FREE, chan);

    ast_log(LOG_DEBUG, "(%d) hs_id %u: freeing DC on line %d, chan %d\n", line,
	p->hs_id, p->line->lnum, chan);

    if (!p->alloc_count)
    {
	ast_log(LOG_DEBUG, "(%d) hs_id %u: alloc_count already zero for line %d\n",
	    line, p->hs_id, p->line->lnum);
	return ret;
    }

    p->alloc_count--;
    ast_log(LOG_DEBUG, "(%d) hs_id %u: alloc count for line %d decremented to %d\n", line,
	p->hs_id, p->line->lnum, p->alloc_count);
    if (!p->alloc_count)
    {
	p->real_dc_num = -1;
	p->line->dc_allocated = 0;
    }

    return ret;
}

static struct dect_dsp_line *free_line_find(void)
{ 
    int i;

    for (i = 0; i < MAX_DECT_DSP_LINES; i++)
    {
	if (!dsp_line[i].ref_count && dsp_line[i].lnum != -1)
	    return &dsp_line[i];
    }

    return NULL;
}

static int dect_line_shared_alloc(struct dect_pvt *p, char *id)
{
    int i;

    if (p->line) /* already allocated */
    {
	ast_log(LOG_ERROR, "hs_id %d; id %s; DSP line already allocated "
	    "(line %d)\n", p->hs_id, id, p->line->lnum);
	return -1;
    }

    for (i = 0; i < MAX_DECT_DSP_LINES; i++)
    {
	if (!strcmp(dsp_line[i].shared_callid, id))
	{
	    dsp_line[i].ref_count++;
	    p->line = &dsp_line[i];

	    ast_log(LOG_DEBUG, "hs_id %d; shared alloc of line %d; "
		"ref_count = %d\n", p->hs_id, p->line->lnum,
		p->line->ref_count);
	    return 0;
	}
    }

    p->line = free_line_find();
    if (!p->line)
    {
	ast_log(LOG_WARNING, "hs_id %d; couldn't find free DSP line\n", p->hs_id);
	return -1;
    }

    strncpy(p->line->shared_callid, id, AST_MAX_SHARED_CALLID);
    p->line->alloc_status = LINE_ALLOC_SHARED;
    p->line->ref_count++;

    ast_log(LOG_DEBUG, "hs_id %d; shared alloc of line %d; "
	"ref_count = %d\n", p->hs_id, p->line->lnum, p->line->ref_count);

    return 0;
}

static int dect_line_exclusive_alloc(struct dect_pvt *p)
{
    if (p->line)
    {
	ast_log(LOG_DEBUG, "hs_id %d: trying to grab shared line %d\n",
	    p->hs_id, p->line->lnum);

	if (p->line->alloc_status == LINE_ALLOC_EXCLUSIVE)
	{
	    ast_log(LOG_DEBUG, "hs_id %d; DSP line %d was already grabbed by "
		"someone else\n", p->hs_id, p->line->lnum);
	    return -1; 	
	}
    }
    else if ((p->line = free_line_find()))
	p->line->ref_count++;
    else
    {
	ast_log(LOG_DEBUG, "hs_id %d; couldn't find free DSP line\n", p->hs_id);
	return -1;
    }

    p->line->alloc_status = LINE_ALLOC_EXCLUSIVE;
    ast_log(LOG_DEBUG, "hs_id %d; sucessfull exclusive alloc of line %d; "
	"ref_count = %d\n", p->hs_id, p->line->lnum, p->line->ref_count);

    return 0;
}

static void dect_line_free(struct dect_pvt *p)
{
    if (!p->line)
    {
	ast_log(LOG_WARNING, "hs_id %d; No DSP line to free\n", p->hs_id);
	return;
    }

    ast_log(LOG_DEBUG, "hs_id %d; going to free line %d; ref_count = %d\n", 
	p->hs_id, p->line->lnum, p->line->ref_count);

    if (!--p->line->ref_count)
    {
	p->line->shared_callid[0] = 0;
	p->line->alloc_status = LINE_ALLOC_NONE;
    }
    p->line = NULL;
}

static int dect_dsp_shared_alloc(struct dect_pvt *p, int chan, char *id)
{
    if (dect_line_shared_alloc(p, id))
	return -1;

    /* Allocate DC when the line is allocated for a first time */
    if (p->line->ref_count != 1)
	return 0;

    if (dect_dc_shared_alloc(p, chan, id))
    {
	dect_line_free(p);
	return -1;
    }

    return 0;
}

static int dect_dsp_exclusive_alloc(struct dect_pvt *p, int chan)
{
    if (dect_line_exclusive_alloc(p))
	goto Fail;

    if (dect_dc_exclusive_alloc(p, chan, 0))
	goto Fail;

    return 0;

Fail:
    if (p->line)
    {
	dect_dc_free(p, p->real_dc_num);
	dect_line_free(p);
    }
    return -1;
}

static void dect_dsp_free(struct dect_pvt *p)
{
    if (!p->line)
	return;

    /* Free DC when last reference to the line is going to be freed */
    if (p->line->ref_count == 1)
    {
	if (p->alloc_count > 1) /* Conf call is active - free the second DC */
	    dect_dc_free(p, OTHER_DC_NUM(p->real_dc_num));
	dect_dc_free(p, p->real_dc_num);
    }

    dect_line_free(p);
}

static int dect_dsp_line_configure(struct dect_pvt *p)
{
    voip_dsp_line_conf_t dsp_line_conf = {0};
    int res;

    switch (p->codec) {
    case DECT_CODEC_G726:
	dsp_line_conf.pcm_res = PCM_RES_NB_LINEAR_16BIT;
	break;
    case DECT_CODEC_G722:
	dsp_line_conf.pcm_res = PCM_RES_WB_ALAW_8BIT;
	break;
    default:
	dsp_line_conf.pcm_res = PCM_RES_NB_LINEAR_16BIT;
    }

    ast_log(LOG_DEBUG, "hs_id %u: configure PCM line %d; resolution %d\n",
	p->hs_id, p->line->lnum, dsp_line_conf.pcm_res);

    if ((res = ioctl(p->line->fd, VOIP_DSP_LINE_CONF, &dsp_line_conf)))
    {
	ast_log(LOG_WARNING, "(%d) hs_id %u: failed to configure PCM line;\
	    resolution\n", p->hs_id, dsp_line_conf.pcm_res);
    }

    return res;
}

static void wakeup_sub(struct dect_pvt *p, int a, void *pri)
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

static void dect_queue_frame(struct ast_channel *c, struct ast_frame *f)
{
    struct dect_pvt *p = (struct dect_pvt *)c->tech_pvt;

    /* We must unlock the PRI to avoid the possibility of a deadlock */
    for (;;) {
	if (ast_mutex_trylock(&c->lock)) {
	    ast_mutex_unlock(&p->lock);
	    usleep(1);
	    ast_mutex_lock(&p->lock);
	} else {
	    ast_queue_frame(c, f);
	    ast_mutex_unlock(&c->lock);
	    break;
	}
    }
}

static void dect_queue_control(struct ast_channel *ast, int control)
{
    struct ast_frame f = { AST_FRAME_CONTROL, };
    f.subclass = control;
    return dect_queue_frame(ast, &f);
}

static void swap_subs(struct dect_pvt *p, int a, int b)
{
    int tchan;
    int tinthreeway;
    int tinhold;
    struct ast_channel *towner;
    int i;
    int tcallref;
    voip_dsp_bind_arg_t bind_arg;
    struct ast_rtp *trtp;

    ast_log(LOG_DEBUG, "Swapping %d and %d\n", a, b);

    tchan = p->subs[a].chan;
    towner = p->subs[a].owner;
    tinthreeway = p->subs[a].inthreeway;
    trtp = p->subs[a].rtp;
    tcallref = p->subs[a].callref;
    tinhold = p->subs[a].inhold;

    p->subs[a].chan = p->subs[b].chan;
    p->subs[a].owner = p->subs[b].owner;
    p->subs[a].inthreeway = p->subs[b].inthreeway;
    p->subs[a].rtp = p->subs[b].rtp;
    p->subs[a].callref = p->subs[b].callref;
    p->subs[a].inhold = p->subs[b].inhold;

    p->subs[b].chan = tchan;
    p->subs[b].owner = towner;
    p->subs[b].inthreeway = tinthreeway;
    p->subs[b].rtp = trtp;
    p->subs[b].callref = tcallref;
    p->subs[b].inhold = tinhold;

    if (p->subs[a].chan != -1 && p->subs[a].dfd != -1 )
    {
	bind_arg.line = p->line->lnum - 1;
	bind_arg.channel = p->subs[a].chan;
	ioctl(p->subs[a].dfd, VOIP_DSP_BIND, &bind_arg);
    }

    if (p->subs[b].chan != -1 && p->subs[b].dfd != -1 )
    {
	bind_arg.line = p->line->lnum - 1;
	bind_arg.channel = p->subs[b].chan;
	ioctl(p->subs[b].dfd, VOIP_DSP_BIND, &bind_arg);
    }

    if (p->subs[a].owner) 
    {
	i = 0;
	if (a == SUB_REAL)
	    p->subs[a].owner->fds[i++] = p->line->fd;
	p->subs[a].owner->fds[i++] = p->subs[a].dfd;
	p->subs[a].owner->fds[i++] = -1;
    }
    if (p->subs[b].owner) 
    {
	i = 0;
	if (b == SUB_REAL)
	    p->subs[b].owner->fds[i++] = p->line->fd;
	p->subs[b].owner->fds[i++] = p->subs[b].dfd;
	p->subs[b].owner->fds[i++] = -1;
    }

    wakeup_sub(p, a, NULL);
    wakeup_sub(p, b, NULL);
}

static int dect_open_dsp_line(int line)
{
    int fd = -1;

    line--; /* Asterisk uses one based numbering for slics */
    fd = gpl_sys_rg_chrdev_open(KOS_CDT_VOIP_SLIC, O_RDWR | O_NONBLOCK);
    if (fd == -1)
    {
	ast_log(LOG_WARNING, "Unable to open dect kos char device\n");
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

static int dect_open_dsp(void)
{
    int fd;

    fd = gpl_sys_rg_chrdev_open(KOS_CDT_VOIP_DSP, O_RDWR | O_NONBLOCK);
    if (fd == -1)
    {
	ast_log(LOG_WARNING, "Unable to open dect kos char device\n");
	return -1;
    }

    return fd;
}

static void dect_close(int fd)
{
    if(fd > 0)
	close(fd);
}


static int dect_stop_dtmf(void* data)
{
    struct dect_pvt* p = (struct dect_pvt*)data;
    struct ast_frame f = {};

    if(p->dtmf.ast)
    {
	f.subclass = p->dtmf.dtmf_key;
	f.frametype = AST_FRAME_DTMF_END;
	dect_queue_frame(p->dtmf.ast, &f);   
	memset(&(p->dtmf), 0, sizeof(struct dect_dtmf));
    }
    return 0;
}

static void dect_start_dtmf(struct ast_channel *ast, char dtmf_key)
{
    struct dect_pvt* p = ast->tech_pvt;
    struct ast_frame f = {};

    //if already playing a dtmf, stop it and start the new one
    if (p->dtmf.dtmf_sched)
    {
	ast_sched_del(sched, p->dtmf.dtmf_sched);
	dect_stop_dtmf(p);
    }

    p->dtmf.dtmf_key = dtmf_key;
    p->dtmf.ast = ast;
    p->dtmf.dtmf_sched = ast_sched_add(sched, DTMF_DURATION_MS, 
	dect_stop_dtmf, p);

    f.subclass = dtmf_key;
    f.frametype = AST_FRAME_DTMF_BEGIN;
    dect_queue_frame(ast, &f);
}

static int unalloc_sub(struct dect_pvt *p, int x)
{
    if (!x) {
	ast_log(LOG_WARNING, "Trying to unalloc the real channel %d?!?\n", p->hs_id);
	return -1;
    }
    ast_log(LOG_DEBUG, "Released sub %d of hs_id %d\n", x, p->hs_id);
    /* This is our last chance to stop audio on this subchannel, just before
     * we zero 'owner'.
     * For example, this is the place where audio is stopped in the scenario
     * where we hang up a call while there is a call waiting.
     * XXX Should we stop it in swap_subs()? */
    if (p->subs[x].owner && p->owner == p->subs[x].owner)
	dect_stop_audio(p->subs[x].owner, x);
    /* if sub channel is during playing dtmf, stop it*/
    if (p->subs[x].owner && p->subs[x].owner == p->dtmf.ast && 
	p->dtmf.dtmf_sched != 0)
    {
	ast_sched_del(sched, p->dtmf.dtmf_sched);
	dect_stop_dtmf(p);
    }
    p->subs[x].owner = NULL;
    p->subs[x].inthreeway = 0;
    p->subs[x].inhold = 0;
    return 0;
}

static int dect_digit_begin(struct ast_channel *ast, char digit)
{
    struct dect_pvt *p;
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
    index = dect_get_index(ast, p, 0);
    if ((index == SUB_REAL) && p->owner) {
	tone_param_t param;

	param.direction = TONE_DIRECTION_LOCAL;
	param.tone = tone;
	if ((res = ioctl(p->line->fd, VOIP_LINE_TONE, &param)))
	    ast_log(LOG_WARNING, "Couldn't dial digit %c\n", digit);
    }
    ast_mutex_unlock(&p->lock);
    restart_monitor();
    return res;
}

static int dect_digit_end(struct ast_channel *ast, char digit)
{
    struct dect_pvt *p = ast->tech_pvt;
    tone_param_t param;

    if (!p)
	return 0;

    param.direction = TONE_DIRECTION_LOCAL;
    param.tone = PHONE_TONE_NONE;

    ast_mutex_lock(&p->lock);
    ioctl(p->line->fd, VOIP_LINE_TONE, &param);
    ast_mutex_unlock(&p->lock);
    return 0;
}

static int need_mwi_tone(struct dect_pvt *p)
{
    if (p->dialtone != PHONE_TONE_DIAL)
	return 0;

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

static int vmwi_changed(struct dect_pvt *p)
{
    if (p->vmwi == MWI_INTERNAL) {
	p->next_msg_wait = (ast_app_has_voicemail(p->mailbox, "REMOTE") ||
	    ast_app_has_voicemail(p->mailbox, NULL));
    }
    return p->vmwi && (p->next_msg_wait != p->msg_wait);
}

#if 0
static void prepare_outgoing_cid(struct dect_pvt *pvt, phone_caller_id_t *cid,
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
#endif

static void prepare_incoming_cid(struct dect_pvt *p, struct ast_channel *chan)
{
    if (!ast_strlen_zero(p->cid_num)) 
    {
	if (!p->hidecallerid)
	    ast_set_callerid(chan, p->cid_num, NULL, p->cid_num); 
	else
	    ast_set_callerid(chan, NULL, NULL, p->cid_num); 
    }
    if (!ast_strlen_zero(p->cid_name)) 
    {
	if (!p->hidecallerid)
	    ast_set_callerid(chan, NULL, p->cid_name, NULL);
    }
}

static int dect_play_tone(struct dect_pvt *p, phone_tone_t tone);

static int dect_callwait(struct dect_pvt *pvt, int start)
{
    /* line can be null in case the first call was hung up while 
       a second call is pending for answer and then the second call hangs up.*/
    if (!pvt->line)
	return 0;

    if (!start)
    {
	if (pvt->callwaitingalert)
	{
	    ast_log(LOG_DEBUG, "Going to stop call waiting tone\n");
	    pvt->callwaitingalert = 0;
	    return dect_play_tone(pvt, PHONE_TONE_NONE);
	}
	return 0;
    }

    pvt->callwaitingalert = 1;

    ast_log(LOG_DEBUG, "Going to start call waiting tone\n");
    return dect_play_tone(pvt, PHONE_TONE_CALLER_WAITING);
}

static int dect_setup(struct dect_pvt *pvt, int is_callwaiting)
{
    return dect_cc_setup(pvt->hs_id, pvt->line->pcm_timeslot, !is_callwaiting ?
	pvt->owner->cid.cid_num : pvt->callwait_num, !is_callwaiting ? 
	pvt->owner->cid.cid_name : pvt->callwait_name, 
	DECT_CODEC_G726 | DECT_CODEC_G722, is_callwaiting);
}

static int dect_off_hook_warning(void *data);
static int dect_stop_off_hook_warning(void *data);

static int dect_play_tone(struct dect_pvt *p, phone_tone_t tone)
{
    tone_param_t param;
    int res;

    /* Ignore requests to play a tone that is already playing */
    if (tone == p->play_tone)
	return 0;

    if (tone == PHONE_TONE_REORDER && p->offhookwarningschedid == -1)
    {
	p->offhookwarningschedid = ast_sched_add(sched, offhookwarningtimeout,
	    dect_off_hook_warning, p);
    }
    else if (p->offhookwarningschedid > -1 && tone != PHONE_TONE_REORDER &&
	tone != PHONE_TONE_HOOK_OFF)
    {
	ast_sched_del(sched, p->offhookwarningschedid);
	p->offhookwarningschedid = -1;
    }
    else if (tone == PHONE_TONE_HOOK_OFF)
    {
	p->offhookwarningschedid = ast_sched_add(sched, stopoffhookwarningtimeout,
	    dect_stop_off_hook_warning, p);
    }

    param.direction = TONE_DIRECTION_LOCAL;

    if (tone && p->play_tone)
    {
	param.tone = PHONE_TONE_NONE;
	ioctl(p->line->fd, VOIP_LINE_TONE, &param);
    }

    param.tone = tone;
    p->play_tone = tone;

    res = ioctl(p->line->fd, VOIP_LINE_TONE, &param);

    if (res)
    {
	ast_log(LOG_ERROR, "Unable to play tone %d for handset %u [res=%d]\n",
	    tone, p->hs_id, res);
    }

    return res;
}

static int dect_call(struct ast_channel *ast, char *rdest, int timeout)
{
    struct dect_pvt *p = ast->tech_pvt;
    char *c, *n, *l;
    char dest[256]; /* must be same length as p->dialdest */
    ast_mutex_lock(&p->lock);
    ast_copy_string(dest, rdest, sizeof(dest));
    ast_copy_string(p->dialdest, rdest, sizeof(p->dialdest));
    if ((ast->_state == AST_STATE_BUSY)) {
	ast_mutex_unlock(&p->lock);
	return 0;
    }
    if ((ast->_state != AST_STATE_DOWN) && (ast->_state != AST_STATE_RESERVED)) {
	ast_log(LOG_WARNING, "dect_call called on %s, neither down nor reserved\n", ast->name);
	ast_mutex_unlock(&p->lock);
	return -1;
    }
    p->outgoing = 1;

    if (p->owner == ast) {
	/* Allocate DSP resources */
	if (dect_dsp_shared_alloc(p, 0, p->owner->shared_callid)) {
	    ast_mutex_unlock(&p->lock);
	    return -1;
	}

	p->real_dc_num = 0;

	/* nick@dccinc.com 4/3/03 mods to allow for deferred dialing */
	c = strchr(dest, '/');
	if (c)
	    c++;

	if (dect_setup(p, 0))
	    return -1;
    } else {
	/* Call waiting call */
	p->callwaitrings = 0;
	if (ast->cid.cid_num)
	    ast_copy_string(p->callwait_num, ast->cid.cid_num, 
		sizeof(p->callwait_num));
	else
	    p->callwait_num[0] = '\0';
	if (ast->cid.cid_name)
	    ast_copy_string(p->callwait_name, ast->cid.cid_name, 
		sizeof(p->callwait_name));
	else
	    p->callwait_name[0] = '\0';

	ast_log(LOG_DEBUG, "dect_call called on %s, in callwaiting from %s, "
	    "name=%s\n", ast->name,p->callwait_num, p->callwait_name);

	/* Send a new setup message to vf_dect for the call waiting */
	if (dect_setup(p, 1))
	{
	    ast_log(LOG_DEBUG, "dect_setup failed!\n");
	    return -1;
	}
 	
	/* Call waiting tone instead */
	if (dect_callwait(p, 1)) {
	    ast_mutex_unlock(&p->lock);
	    return -1;
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
    ast_setstate(ast, AST_STATE_RINGING);
    dect_queue_control(ast, AST_CONTROL_RINGING);	

    ast_mutex_unlock(&p->lock);
    return 0;
}

static void destroy_dect_pvt(struct dect_pvt **pvt)
{
    struct dect_pvt *p = *pvt;
    /* Remove channel from the list */
    if(p->prev)
	p->prev->next = p->next;
    if(p->next)
	p->next->prev = p->prev;
    ast_mutex_destroy(&p->lock);
    free(p);
    *pvt = NULL;
}

static int destroy_channel(struct dect_pvt *prev, struct dect_pvt *cur, int now)
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
	    destroy_dect_pvt(&cur);
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
	destroy_dect_pvt(&cur);
    }
    return 0;
}

static void dect_start_audio(struct ast_channel *ast, int index)
{
    struct dect_pvt *p = ast->tech_pvt;
    int otherindex;
    voip_dsp_record_args_t args;
    voip_dsp_bind_arg_t bind_arg;

    if (p->subs[index].chan != -1)
	dect_stop_audio(ast, index);

    if (ast_bridged_channel(ast) && ast_bridged_channel(ast)->tech->get_rtp)
	p->subs[index].rtp = ast_bridged_channel(ast)->tech->get_rtp(ast_bridged_channel(ast));

    /* We assume that voice may be started only for SUB_REAL and SUB_THREEWAY */
    otherindex = index == SUB_REAL ? SUB_THREEWAY : SUB_REAL;
    if (p->subs[otherindex].chan == -1)
    {
	/* use currently allocated channel */
	args.channel = p->real_dc_num; 
    }
    else
    {
	/* there is ongoing call already, use the second channel */
	args.channel = OTHER_DC_NUM(p->subs[otherindex].chan); 
    }

    if (p->modemdetected)
	args.data_mode = VOIP_DATA_MODE_MODEM;
    else
    {
	switch (p->faxtxmethod) {
	case FAX_TX_NONE:
	    args.data_mode = VOIP_DATA_MODE_VOICE;
	    break;
	case FAX_TX_T38_AUTO:
	    args.data_mode = p->faxdetected ? VOIP_DATA_MODE_T38 :
		VOIP_DATA_MODE_VOICE;
	    break;
	case FAX_TX_PASSTHROUGH_AUTO:
	    args.data_mode = p->faxdetected ? VOIP_DATA_MODE_FAX :
		VOIP_DATA_MODE_VOICE;
	    break;
	case FAX_TX_PASSTHROUGH_FORCE:
	    args.data_mode = VOIP_DATA_MODE_FAX;
	    break;
	}
    }

    /* For T.38 packets we don't use the dect->jrtp fastpath because jrtp
     * is not designed to handle these kinds of packets. So don't bind the
     * dect to a jrtp session in this case. */
    if (p->subs[index].rtp && args.data_mode != VOIP_DATA_MODE_T38)
    {
	char *annexb = pbx_builtin_getvar_helper(ast,"G729_ANNEXB");
	ast_rtp_clear_payload_types(p->subs[index].rtp);
	ast_rtp_set_payload_type(p->subs[index].rtp,
	    jdsp_codec_ast2rtp(ast->readformat), ast->readformat, 1);
	ast_rtp_set_payload_type(p->subs[index].rtp,
	    jdsp_codec_ast2rtp(ast->writeformat), ast->writeformat, 1);
	ast_rtp_set_payload_type(p->subs[index].rtp, -1, AST_RTP_CN, 0);
	args.rtp_context = ast_rtp_get_context(p->subs[index].rtp);
	args.codec = jdsp_codec_ast2rtp(ast->readformat);
	args.suppress_dtmf = args.data_mode != VOIP_DATA_MODE_FAX &&
	    args.data_mode != VOIP_DATA_MODE_MODEM &&
	    !ast_rtp_get_inband_dtmf(p->subs[index].rtp);

	/* dect channel information has higher priority */
	if (!annexb && ast_bridged_channel(ast))
	{
	    ast_log(LOG_DEBUG, "Couldn't find G729 ANNEXB on %s, checking on "
                "the bridged %s\n", ast->name, ast_bridged_channel(ast)->name);
	    annexb = pbx_builtin_getvar_helper(ast_bridged_channel(ast),"G729_ANNEXB");
	}
	args.disable_vad = annexb && !strcmp(annexb, "NO");
    }
    else
    {
	args.rtp_context = 0;
	args.codec = jdsp_codec_ast2rtp(ast->readformat);
	args.suppress_dtmf = 1;
	args.disable_vad = 0;
    }
    args.ptime_ms = ast->ptime ? ast->ptime : 20;

    ast_log(LOG_DEBUG, "Starting audio on channel %s w/ codec %d, suppress_dtmf %d, vad %d, dc %d\n", 
	p->owner->name, args.codec, args.suppress_dtmf, args.disable_vad, args.channel);


    if (p->subs[index].dfd == -1)
    {
	ast_log(LOG_ERROR, "It's impossible - Starting audio on channel %s, index %d\n",
	    ast->name, index);
    }
    else
    {
	bind_arg.line = p->line->lnum - 1;
	bind_arg.channel = args.channel;
	ioctl(p->subs[index].dfd, VOIP_DSP_BIND, &bind_arg);
    }

    if (ioctl(p->line->fd, VOIP_DSP_START, &args) < 0) {
	ast_log(LOG_ERROR, "Failed to start audio on channel %s\n", ast->name);
	return;
    }

    p->subs[index].chan = args.channel;
    p->subs[index].seqno = 0;
}

static void dect_stop_audio(struct ast_channel *ast, int index)
{
    struct dect_pvt *p = ast->tech_pvt;

    ast_log(LOG_DEBUG, "Stopping audio on channel %s\n", ast->name);

    if (p->subs[index].rtp)
    {
	ast_rtp_clear_formats(p->subs[index].rtp);
	p->subs[index].rtp = NULL;
    }

    /* This is not error case. dect_stop_audio may be called when voice is not
     * started */
    if (p->subs[index].chan == -1)
    {
	ast_log(LOG_DEBUG, "Can't stop audio on channel %s, "
	    "no dsp channel\n", ast->name);
	return;
    }

    if (ioctl(p->line->fd, VOIP_DSP_STOP, p->subs[index].chan) < 0)
    {
	ast_log(LOG_ERROR, "Failed to stop audio on channel %s, line %d (fd %d)\n",
	    ast->name, p->line->lnum, p->line->fd);
    }

    /* Conference */
    if (p->alloc_count > 1)
    {
	if (p->subs[index].chan != -1)
	{
	    dect_dc_free(p, p->subs[index].chan);
	    p->real_dc_num = OTHER_DC_NUM(p->subs[index].chan);
	}
    }

    p->subs[index].chan = -1;
}

static void dect_moh_stop(struct dect_pvt *p, int index)
{
    if (internalmoh)
	ast_moh_stop(ast_bridged_channel(p->subs[index].owner));
    else
	ast_queue_control(p->subs[index].owner, AST_CONTROL_UNHOLD);
}

static void dect_moh_start(struct dect_pvt *p, int index)
{
    if (internalmoh)
	ast_moh_start(ast_bridged_channel(p->subs[index].owner), NULL);
    else
	ast_queue_control(p->subs[index].owner,	AST_CONTROL_HOLD);
}

static int dect_off_hook_warning(void *data)
{
    struct dect_pvt *p = data;

    ast_mutex_lock(&p->lock);
    dect_play_tone(p, PHONE_TONE_HOOK_OFF);
    ast_mutex_unlock(&p->lock);

    return 0;
}

static int dect_stop_off_hook_warning(void *data)
{
    struct dect_pvt *p = data;

    ast_mutex_lock(&p->lock);
    dect_play_tone(p, PHONE_TONE_NONE);
    ast_mutex_unlock(&p->lock);

    return 0;
}


static int dect_hold_channel(struct dect_pvt *p, struct ast_channel *ast, 
			     int index)
{
    ast_log(LOG_WARNING, "hs_id %u; Putting call on HOLD (index %d, channel " 
	"%s)\n", p->hs_id, index, ast->name);

    dect_stop_audio(ast, index); 
    dect_moh_start(p, index);
    p->subs[index].inhold = 1;

    return 0;
}

static int dect_unhold_channel(struct dect_pvt *p, struct ast_channel *ast, 
			       int index)
{
    ast_log(LOG_WARNING, "hs_id %u; UNHOLD call (index %d, channel " 
	"%s)\n", p->hs_id, index, ast->name);

    p->subs[index].inhold = 0;
    dect_moh_stop(p, index);
    /* stop playing call waiting tone - can happen when active call is
       disconnected while the second call is held*/
    dect_callwait(p, 0);

    return 0;
}


#if 0
static int cancel_fw_disconnect(void *data)
{
    struct dect_pvt *p = data;

    if (p->fwd_disconnect == FWD_DISCONNECT_IN_PROGRESS)
	p->fwd_disconnect = FWD_DISCONNECT_ON;
    return 0;
}
#endif

static void dect_handle_release(struct dect_pvt *p, int index);

static int dect_release_timeout(void *data)
{
    struct release_timer_data_t *timer_data = (struct release_timer_data_t *)data;
    struct dect_pvt *p = timer_data->pvt;

    ast_log(LOG_WARNING, "hs_id %u; didn't get CC_RELEASE in %d ms; "
	"releasing\n", p->hs_id, CC_RELEASE_TIMEOUT);
    ast_mutex_lock(&p->lock);
    dect_handle_release(p, timer_data->index);
    ast_mutex_unlock(&p->lock);
    free(timer_data);

    /* Don't call again */
    return 0;
}

static void dect_set_release_timeout(struct dect_pvt *p, int index)
{
    struct release_timer_data_t *data;

    if (!(data = malloc(sizeof(struct release_timer_data_t))))
    {
	ast_log(LOG_ERROR, "hs_id %u; failed to allocate "
	    "release_timer_data_t\n", p->hs_id);
	return;
    }
    data->index = index;
    data->pvt = p;
    p->release_timer = ast_sched_add(sched, CC_RELEASE_TIMEOUT,
	dect_release_timeout, data);
}

static void dect_disconnect(struct dect_pvt *p, int index)
{
    dect_cc_disconnect(p->hs_id, p->subs[index].callref, 
	!p->subs[SUB_CALLWAIT].owner && !p->subs[SUB_THREEWAY].owner);
    dect_set_release_timeout(p, index);
}

static int dect_hangup(struct ast_channel *ast)
{
    int index, x, held_index;
    struct dect_pvt *p = ast->tech_pvt;
    struct dect_pvt *tmp = NULL;
    struct dect_pvt *prev = NULL;
    /* TODO: Check what is FWD_DISCONNECT is fix/remove all the code related */
#if 0
    int need_fwd_disconnect = 0;
#endif

    if (option_debug)
	ast_log(LOG_DEBUG, "dect_hangup(%s)\n", ast->name);
    if (!ast->tech_pvt) {
	ast_log(LOG_WARNING, "Asked to hangup channel not connected\n");
	return 0;
    }

    ast_mutex_lock(&p->lock);

    index = dect_get_index(ast, p, 1);

    dect_stop_audio(ast, index);
    /* stop playing call waiting tone if needed */
    dect_callwait(p, 0);
    /* TODO: Check what is FWD_DISCONNECT is fix/remove all the code related */
#if 0
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
#endif

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

    ast_log(LOG_DEBUG, "Hangup: hs_id: %d, index = %d, normal = %p,"
        "callwait = %p, thirdcall = %p\n", p->hs_id, index, 
	p->subs[SUB_REAL].owner, p->subs[SUB_CALLWAIT].owner, 
	p->subs[SUB_THREEWAY].owner);

    if (index > -1) {
	if (ast == p->dtmf.ast && p->dtmf.dtmf_sched != 0)
	{
	    ast_sched_del(sched, p->dtmf.dtmf_sched);
	    dect_stop_dtmf(p);
	}
	/* Real channel, do some fixup */
	p->subs[index].owner = NULL;
	p->subs[index].needanswer = 0;

	if (index == SUB_REAL) 
	{
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
	    } else if (p->subs[SUB_CALLWAIT].owner || p->subs[SUB_THREEWAY].owner) 
	    {
		held_index = p->subs[SUB_CALLWAIT].owner? SUB_CALLWAIT : SUB_THREEWAY;
		/* If we have a second call, Send cc_disconnect to vf_dect for 
		   the active (REAL) call. switch call handling is done when 
		   receiving release */
		if (p->subs[index].callref) /*if remote party hung up*/
		{
		    ast_log(LOG_DEBUG, "has second call. send disconnect to "
			"dect handset\n");
		    dect_disconnect(p, index);
		}
		else /*if dect hung up*/
		{
		    /*has a second call, switch to it*/
		    ast_log(LOG_DEBUG, "handset has second call. switch to "
			"it\n");
		    swap_subs(p, SUB_REAL, held_index);
		    unalloc_sub(p, held_index);
		    p->owner = p->subs[SUB_REAL].owner;
		    /*now, the former call waiting is the only call we have. if
		      the call was not answered yet (still ringing), free the 
		      dsp line.*/
		    if (p->subs[SUB_REAL].owner->_state != AST_STATE_UP)
		    {
			ast_log(LOG_DEBUG, "second call still not answered. "
			    "freeing dsp line\n");
			dect_dsp_free(p);
		    }
		    else
		    {
			/* play call waiting tone*/
			dect_callwait(p, 1);
		    }
		}
	    } 
#if 0
	    else if (p->subs[SUB_THREEWAY].owner) 
	    {
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
		    dect_play_tone(p, PHONE_TONE_REORDER);
		}
		p->subs[SUB_REAL].inthreeway = 0;
	    }
#endif
	} else if (index == SUB_CALLWAIT) 
	{
	    /* Ditch the holding callwait call, and immediately make it availabe */
	    if (p->subs[SUB_CALLWAIT].inthreeway) 
	    {
		/* This is actually part of a three way, placed on hold.  Place the third part
		   on music on hold now */

		if (p->subs[SUB_THREEWAY].owner && ast_bridged_channel(p->subs[SUB_THREEWAY].owner))
		{
		    dect_stop_audio(p->subs[SUB_THREEWAY].owner, SUB_THREEWAY); /* XXX Need to check whether this line is needed */
		    dect_moh_start(p, SUB_THREEWAY);
		}

		p->subs[SUB_THREEWAY].inthreeway = 0;
		/* Make it the call wait now */
		swap_subs(p, SUB_CALLWAIT, SUB_THREEWAY);
		unalloc_sub(p, SUB_THREEWAY);
	    } 
	    else
	    {    
		/* If only the second call was hangup, send cc_disconnect to 
		   vf_dect for the callwait call */
 		dect_disconnect(p, index); 
	    }
	} 
	else if (index == SUB_THREEWAY) 
	{
	    ast_log(LOG_DEBUG, "remove channel %s from conference\n", 
		p->subs[index].owner->name);
	    dect_disconnect(p, index);
#if 0
	    if (p->subs[SUB_CALLWAIT].inthreeway) {
		/* The other party of the three way call is currently in a call-wait state.
		   Start music on hold for them, and take the main guy out of the third call */

		if (p->subs[SUB_CALLWAIT].owner && ast_bridged_channel(p->subs[SUB_CALLWAIT].owner))
		{
		    dect_stop_audio(p->subs[SUB_CALLWAIT].owner, SUB_CALLWAIT); /* XXX Need to check whether this line is needed */
		    dect_moh_start(p, SUB_CALLWAIT);
		}

		p->subs[SUB_CALLWAIT].inthreeway = 0;
	    }
	    p->subs[SUB_REAL].inthreeway = 0;
	    /* If this was part of a three way call index, let us make
	       another three way call */
	    unalloc_sub(p, SUB_THREEWAY);
#endif
	} else {
	    /* This wasn't any sort of call, but how are we an index? */
	    ast_log(LOG_WARNING, "Index found but not any type of call?\n");
	}
    }

    if (!p->subs[SUB_REAL].owner && !p->subs[SUB_CALLWAIT].owner && !p->subs[SUB_THREEWAY].owner) {

	p->owner = NULL;
	p->distinctivering = 0;
	p->confirmanswer = 0;
	p->outgoing = 0;
	p->onhooktime = time(NULL);
	p->faxdetected = p->modemdetected = 0;
	p->follow_call = 0;
	p->network_failure_disconnection = 0;

	/*
TODO: Decide how to disconnect the call: by sending DISCONNECT or 
playing  busy tone untill DISCONNECT is received from the handset.
	 */
#if 0
	ast_log(LOG_ERROR, "hangup; playing rerder");
	dect_play_tone(p, PHONE_TONE_REORDER);
#else
	if (p->subs[index].callref)
	    dect_disconnect(p, index);
	else
	    ast_log(LOG_DEBUG, "Not sending CC_DISCONNECT in hangup, callref is 0\n");
#endif
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
#if 0
	/* Provide a dial tone and start collecting digits */
	start_call(p);
#endif
    }

    return 0;
}

static int dect_answer(struct ast_channel *ast)
{
    struct dect_pvt *p = ast->tech_pvt;
    int res = 0;
    int index;
    ast_setstate(ast, AST_STATE_UP);
    ast_mutex_lock(&p->lock);
    index = dect_get_index(ast, p, 0);
    if (index < 0)
	index = SUB_REAL;

    /* Pick up the line */
    ast_log(LOG_DEBUG, "Took %s off hook\n", ast->name);
    dect_play_tone(p, PHONE_TONE_NONE);
    dect_start_audio(p->subs[index].owner, index);

    ast_mutex_unlock(&p->lock);
    return res;
}

static void dect_unlink(struct dect_pvt *slave, struct dect_pvt *master, int needlock)
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
		ast_log(LOG_DEBUG, "Unlinking slave %d from %d\n", master->slaves[x]->hs_id, master->hs_id);
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

static int dect_fixup(struct ast_channel *oldchan, struct ast_channel *newchan)
{
    struct dect_pvt *p = newchan->tech_pvt;
    int x;
    ast_mutex_lock(&p->lock);
    ast_log(LOG_DEBUG, "New owner for channel %d is %s\n", p->hs_id, newchan->name);
    if (p->owner == oldchan) {
	p->owner = newchan;
    }
    for (x = 0; x < 3; x++)
	if (p->subs[x].owner == oldchan) {
	    if (!x)
		dect_unlink(NULL, p, 0);
	    p->subs[x].owner = newchan;
	}
    if (newchan->_state == AST_STATE_RINGING) 
	dect_indicate(newchan, AST_CONTROL_RINGING);
    ast_mutex_unlock(&p->lock);
    return 0;
}

static void *ss_thread(void *data);

static struct ast_channel *dect_new(struct dect_pvt *, int, int, int, int);

/* TODO: Call waiting and conference */
#if 0 
static int attempt_transfer(struct dect_pvt *p)
{
    /* In order to transfer, we need at least one of the channels to
       actually be in a call bridge.  We can't conference two applications
       together (but then, why would we want to?) */
    if (ast_bridged_channel(p->subs[SUB_REAL].owner)) {
	/* The three-way person we're about to transfer to could still be in MOH, so
	   stop if now if appropriate */
	if (ast_bridged_channel(p->subs[SUB_THREEWAY].owner))
	    dect_moh_stop(p, SUB_THREEWAY);
	if (p->subs[SUB_THREEWAY].owner->_state == AST_STATE_RINGING) {
	    ast_indicate(ast_bridged_channel(p->subs[SUB_REAL].owner), AST_CONTROL_RINGING);
	}
	if (p->subs[SUB_REAL].owner->cdr) {
	    /* Move CDR from second channel to current one */
	    p->subs[SUB_THREEWAY].owner->cdr =
		ast_cdr_append(p->subs[SUB_THREEWAY].owner->cdr, p->subs[SUB_REAL].owner->cdr);
	    p->subs[SUB_REAL].owner->cdr = NULL;
	}
	if (ast_bridged_channel(p->subs[SUB_REAL].owner)->cdr) {
	    /* Move CDR from second channel's bridge to current one */
	    p->subs[SUB_THREEWAY].owner->cdr =
		ast_cdr_append(p->subs[SUB_THREEWAY].owner->cdr, ast_bridged_channel(p->subs[SUB_REAL].owner)->cdr);
	    ast_bridged_channel(p->subs[SUB_REAL].owner)->cdr = NULL;
	}
	/* Last chance to stop audio on the channel before it becomes a
	 * zombie */
	dect_stop_audio(p->subs[SUB_THREEWAY].owner, SUB_THREEWAY);
	if (ast_channel_masquerade(p->subs[SUB_THREEWAY].owner, ast_bridged_channel(p->subs[SUB_REAL].owner))) {
	    ast_log(LOG_WARNING, "Unable to masquerade %s as %s\n",
		ast_bridged_channel(p->subs[SUB_REAL].owner)->name, p->subs[SUB_THREEWAY].owner->name);
	    return -1;
	}
	/* Orphan the channel after releasing the lock */
	ast_mutex_unlock(&p->subs[SUB_THREEWAY].owner->lock);
	unalloc_sub(p, SUB_THREEWAY);
    } else if (ast_bridged_channel(p->subs[SUB_THREEWAY].owner)) {
	if (p->subs[SUB_REAL].owner->_state == AST_STATE_RINGING) {
	    ast_indicate(ast_bridged_channel(p->subs[SUB_THREEWAY].owner), AST_CONTROL_RINGING);
	}
	dect_moh_stop(p, SUB_THREEWAY);
	if (p->subs[SUB_THREEWAY].owner->cdr) {
	    /* Move CDR from second channel to current one */
	    p->subs[SUB_REAL].owner->cdr = 
		ast_cdr_append(p->subs[SUB_REAL].owner->cdr, p->subs[SUB_THREEWAY].owner->cdr);
	    p->subs[SUB_THREEWAY].owner->cdr = NULL;
	}
	if (ast_bridged_channel(p->subs[SUB_THREEWAY].owner)->cdr) {
	    /* Move CDR from second channel's bridge to current one */
	    p->subs[SUB_REAL].owner->cdr = 
		ast_cdr_append(p->subs[SUB_REAL].owner->cdr, ast_bridged_channel(p->subs[SUB_THREEWAY].owner)->cdr);
	    ast_bridged_channel(p->subs[SUB_THREEWAY].owner)->cdr = NULL;
	}
	/* Last chance to stop audio on the channel before it becomes a
	 * zombie */
	dect_stop_audio(p->subs[SUB_REAL].owner, SUB_REAL);
	if (ast_channel_masquerade(p->subs[SUB_REAL].owner, ast_bridged_channel(p->subs[SUB_THREEWAY].owner))) {
	    ast_log(LOG_WARNING, "Unable to masquerade %s as %s\n",
		ast_bridged_channel(p->subs[SUB_THREEWAY].owner)->name, p->subs[SUB_REAL].owner->name);
	    return -1;
	}
	/* Three-way is now the REAL */
	swap_subs(p, SUB_THREEWAY, SUB_REAL);
	ast_mutex_unlock(&p->subs[SUB_REAL].owner->lock);
	unalloc_sub(p, SUB_THREEWAY);
	/* Tell the caller not to hangup */
	return 1;
    } else {
	ast_log(LOG_DEBUG, "Neither %s nor %s are in a bridge, nothing to transfer\n",
	    p->subs[SUB_REAL].owner->name, p->subs[SUB_THREEWAY].owner->name);
	p->subs[SUB_THREEWAY].owner->_softhangup |= AST_SOFTHANGUP_DEV;
	return -1;
    }
    return 0;
}

#endif

static void start_conference(struct ast_channel *ast, struct dect_pvt *p)
{
    int otherindex = SUB_THREEWAY;

    ast_log(LOG_DEBUG, "Building conference on call on %s and %s\n",
	p->subs[SUB_THREEWAY].owner->name, p->subs[SUB_REAL].owner->name);
    
    /* Put them in the threeway, and flip */
    p->subs[SUB_THREEWAY].inthreeway = 1;
    p->subs[SUB_REAL].inthreeway = 1;
    if (ast->_state == AST_STATE_UP) 
    {
	ast_log(LOG_DEBUG, "swap subs\n");
	swap_subs(p, SUB_THREEWAY, SUB_REAL);
	otherindex = SUB_REAL;
    }
    else
	dect_play_tone(p, PHONE_TONE_NONE);

    if (p->subs[otherindex].owner &&
	ast_bridged_channel(p->subs[otherindex].owner))
    {
	dect_unhold_channel(p, ast, otherindex);
	dect_start_audio(p->subs[otherindex].owner, otherindex); /* XXX Need to check whether this line is needed */
    }

    p->owner = p->subs[SUB_REAL].owner;
    if (ast->_state == AST_STATE_RINGING) 
    {
	ast_log(LOG_DEBUG, "Enabling ringtone on real and threeway\n");
	dect_play_tone(p, PHONE_TONE_RING);
    }
}

#if 0

static void drop_last_party_from_conference(struct dect_pvt *p)
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
    dect_play_tone(p, PHONE_TONE_NONE);
    ast_softhangup_nolock(p->subs[SUB_THREEWAY].owner, AST_SOFTHANGUP_DEV);
    p->subs[SUB_REAL].inthreeway = 0;
    p->subs[SUB_THREEWAY].inthreeway = 0;
}
#endif

static void switch_calls(struct dect_pvt *p, int is_callwaiting)
{
    int other_sub = is_callwaiting ? SUB_CALLWAIT : SUB_THREEWAY;

    swap_subs(p, SUB_REAL, other_sub);
    p->owner = p->subs[SUB_REAL].owner;
    ast_log(LOG_DEBUG, "Making %s the new owner\n", p->owner->name);
    if (is_callwaiting && p->owner->_state == AST_STATE_RINGING) 
    {
	ast_setstate(p->owner, AST_STATE_UP);
	dect_queue_control(p->subs[SUB_REAL].owner, AST_CONTROL_ANSWER);
    }
}


#if 0
static void start_threeway_call(struct dect_pvt *p)
{
    struct ast_channel *chan;
    int res;
    pthread_t threadid;
    pthread_attr_t attr;
    char cid_num[256];
    char cid_name[256];

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    if (p->decttrcallerid && p->owner) {
	if (p->owner->cid.cid_num)
	    ast_copy_string(cid_num, p->owner->cid.cid_num, sizeof(cid_num));
	if (p->owner->cid.cid_name)
	    ast_copy_string(cid_name, p->owner->cid.cid_name, sizeof(cid_name));
    }

    /* Make new channel */
    chan = dect_new(p, AST_STATE_RESERVED, 0, SUB_THREEWAY, 0);
    if (p->decttrcallerid) {
	if (!p->origcid_num)
	    p->origcid_num = strdup(p->cid_num);
	if (!p->origcid_name)
	    p->origcid_name = strdup(p->cid_name);
	ast_copy_string(p->cid_num, cid_num, sizeof(p->cid_num));
	ast_copy_string(p->cid_name, cid_name, sizeof(p->cid_name));
    }
    /* Swap things around between the three-way and real call */
    swap_subs(p, SUB_THREEWAY, SUB_REAL);
    res = dect_play_tone(p, PHONE_TONE_STUTTER_DIAL);
    if (res)
	ast_log(LOG_WARNING, "Unable to start dial recall tone on channel %d\n", p->channel);
    p->owner = chan;
    if (!chan) {
	ast_log(LOG_WARNING, "Cannot allocate new structure on channel %d\n", p->channel);
    } else if (ast_pthread_create(&threadid, &attr, ss_thread, chan)) {
	ast_log(LOG_WARNING, "Unable to start simple switch on channel %d\n", p->channel);
	res = dect_play_tone(p, PHONE_TONE_REORDER);
	ast_hangup(chan);
    } else {
	if (option_verbose > 2)	
	    ast_verbose(VERBOSE_PREFIX_3 "Started three way call on channel %d\n", p->channel);

	/* Start music on hold if appropriate */
	if (ast_bridged_channel(p->subs[SUB_THREEWAY].owner))
	{
	    dect_stop_audio(p->subs[SUB_THREEWAY].owner, SUB_THREEWAY); /* XXX Need to check whether this line is needed */
	    dect_moh_start(p, SUB_THREEWAY);
	}
	else
	    ast_queue_hangup(p->subs[SUB_THREEWAY].owner);
    }		
}
#endif

void *dect_pvt_find(u8 hs_id)
{
    struct dect_pvt *i;

    ast_mutex_lock(&iflock);

    /* Find handset in list. */
    for (i = iflist; i && i->hs_id != hs_id; i = i->next);

    ast_mutex_unlock(&iflock);

    return i;
}

static void dect_pvt_set_pref_codec(struct dect_pvt *p, cc_message_t *msg)
{
    if (msg->codecs & DECT_CODEC_G722)
	p->codec = DECT_CODEC_G722;
    else if (msg->codecs & DECT_CODEC_G726)
	p->codec = DECT_CODEC_G726;
    else
    	p->codec = DECT_CODEC_G726;  
}

static int dect_attempt_signaled_transfer(struct ast_channel *ast,
					   int inthreeway)
{
    struct ast_frame f;
    /* XXX If transfer-target hasn't answered yet, 'target_chan' will be
    * NULL and transfer will not be performed. We should add code that
    * knows how to obtain 'target_chan' in this scenario, and perform
    * blind transfer. */
    struct ast_channel *target_chan;
    struct dect_pvt *p = ast->tech_pvt;
    int unlock_sub_index = SUB_THREEWAY;
    int res = -1;
    int index =  dect_get_index(ast, p ,0);
    ast_log(LOG_DEBUG,"Start signaled transfer procedure\n");
    if (index < 0)
	return res;

    /* If we are in a conference call, undo the swap performed by
    * start_conference. This is done in order to prevent the problem
    * described in B39277. */
    if (inthreeway) 
    {
	swap_subs(p, SUB_THREEWAY, SUB_REAL);
	unlock_sub_index = SUB_REAL;
    }

    target_chan = ast_bridged_channel(p->subs[SUB_REAL].owner);
    if (target_chan) 
    {
	/* Tell bridged channel to do
	* attended transfer */
	ast_log(LOG_DEBUG,"Attended transfer\n");
	memset(&f, 0 , sizeof(f));
	f.frametype = AST_FRAME_ATTENDEDTRANSFER;
	f.data = &target_chan;
	f.datalen = sizeof(struct ast_channel *);
	ast_queue_frame(p->subs[SUB_THREEWAY].owner, &f);
	
	p->subs[index].f.frametype = AST_FRAME_NULL;
	p->subs[index].f.subclass = 0;

	res = 0; /* success */
    }
    else /* blind transfer */
    {
	ast_log(LOG_DEBUG,"Blind transfer? We don't know how to do this\n");
	res = -1;
    }
    return res;
}

static void dect_transfer_cdrs(struct dect_pvt *p, int source_index,
			       int dest_index)
{
    if (p->subs[source_index].owner->cdr)
    {
	/* Move CDR from second channel to current one */
	p->subs[dest_index].owner->cdr =
	    ast_cdr_append(p->subs[dest_index].owner->cdr,
	    p->subs[source_index].owner->cdr);
	p->subs[source_index].owner->cdr = NULL;
    }
    if (ast_bridged_channel(p->subs[source_index].owner)->cdr)
    {
	/* Move CDR from second channel's bridge to current one */
	p->subs[dest_index].owner->cdr =
	    ast_cdr_append(p->subs[dest_index].owner->cdr,
	    ast_bridged_channel(p->subs[source_index].owner)->cdr);
	ast_bridged_channel(p->subs[source_index].owner)->cdr = NULL;
    }
}

static int dect_attempt_bridged_transfer(struct dect_pvt *p)
{
    /* In order to transfer, we need at least one of the channels to
    actually be in a call bridge.  We can't conference two applications
    together (but then, why would we want to?) */
    
    ast_log (LOG_DEBUG, "Start bridged transfer procedure\n");
    if (ast_bridged_channel(p->subs[SUB_REAL].owner)) 
    {
	 ast_log (LOG_DEBUG, "SUB_REAL is bridged- attended transfer\n");
	/* The three-way person we're about to transfer to could still be in
	MOH, so	stop if now if appropriate */
	if (ast_bridged_channel(p->subs[SUB_THREEWAY].owner))
	    dect_moh_stop(p, SUB_THREEWAY);
	
	if (p->subs[SUB_THREEWAY].owner->_state == AST_STATE_RINGING)
	{
	    ast_indicate(ast_bridged_channel(p->subs[SUB_REAL].owner),
		AST_CONTROL_RINGING);
	}
	
	dect_transfer_cdrs(p, SUB_REAL, SUB_THREEWAY);
	
	/* Last chance to stop audio on the channel before it becomes a
	* zombie 
	* There can't be any audio here- just for pre-caution  */
	dect_stop_audio(p->subs[SUB_THREEWAY].owner, SUB_THREEWAY);
	ast_log (LOG_DEBUG, "Masquerade %s as %s\n",
	    ast_bridged_channel(p->subs[SUB_REAL].owner)->name,
	    p->subs[SUB_THREEWAY].owner->name);
		
	if (ast_channel_masquerade(p->subs[SUB_THREEWAY].owner,
	    ast_bridged_channel(p->subs[SUB_REAL].owner)))
	{
	    ast_log(LOG_WARNING, "Unable to masquerade %s as %s\n",
		ast_bridged_channel(p->subs[SUB_REAL].owner)->name,
		p->subs[SUB_THREEWAY].owner->name);
	    return -1;
	}	
	swap_subs(p, SUB_THREEWAY, SUB_REAL);
    }
    else if (ast_bridged_channel(p->subs[SUB_THREEWAY].owner))
    {
	ast_log (LOG_DEBUG, "SUB_THREEWAY is bridged (but SUB_REAL still not)-"
	    "blind transfer\n");
	if (p->subs[SUB_REAL].owner->_state == AST_STATE_RINGING)
	{
	    ast_indicate(ast_bridged_channel(p->subs[SUB_THREEWAY].owner),
		AST_CONTROL_RINGING);
	}
	dect_moh_stop(p, SUB_THREEWAY);
	dect_transfer_cdrs(p, SUB_THREEWAY, SUB_REAL);
	
	/* Last chance to stop audio on the channel before it becomes a
	* zombie */
	dect_stop_audio(p->subs[SUB_REAL].owner, SUB_REAL);
	if (ast_channel_masquerade(p->subs[SUB_REAL].owner,
	    ast_bridged_channel(p->subs[SUB_THREEWAY].owner)))
	{
	    ast_log(LOG_WARNING, "Unable to masquerade %s as %s\n",
		ast_bridged_channel(p->subs[SUB_THREEWAY].owner)->name,
		p->subs[SUB_REAL].owner->name);
	    return -1;
	}
	/* Three-way is now the REAL */
	swap_subs(p, SUB_THREEWAY, SUB_REAL);
    }
    else
    {
	ast_log(LOG_DEBUG, "Neither %s nor %s are in a bridge,"
	    "nothing to transfer\n", p->subs[SUB_REAL].owner->name,
	    p->subs[SUB_THREEWAY].owner->name);
	return -1;
    }
    return 0;
}

static void dect_handle_transfer(struct ast_channel *ast, cc_message_t *msg)
{
    struct dect_pvt *p = ast->tech_pvt;

    ast_log(LOG_DEBUG,"Handle a transfer signal");
    ast_mutex_lock(&ast->lock);

    /* Here we have to retain the lock on both the main channel,
    * the 3-way channel, and
    * the private structure -- not especially easy or clean */
    while(p->subs[SUB_THREEWAY].owner &&
	ast_mutex_trylock(&p->subs[SUB_THREEWAY].owner->lock))
    {
	
	ast_mutex_unlock(&p->lock);
	ast_mutex_unlock(&ast->lock);
	usleep(1);
	ast_mutex_lock(&ast->lock);
	ast_mutex_lock(&p->lock);
	if (p->owner != ast) 
	{
	    ast_log(LOG_WARNING, "This isn't good...\n");
	    dect_cc_reject(msg->hs_id, msg->cr);
	    goto Exit;
	}
    }
    if ((!ast->pbx && (ast->_state != AST_STATE_UP)) ||
	!p->subs[SUB_THREEWAY].owner) 
    {
	dect_cc_reject(msg->hs_id, msg->cr);
	goto Unlock3wayAndExit;
    }

    if (p->transfermode != TRANSFER_OFF)
    {
	int inthreeway = p->subs[SUB_REAL].inthreeway &&
	    p->subs[SUB_THREEWAY].inthreeway;
	p->subs[SUB_REAL].inthreeway = 0;
	p->subs[SUB_THREEWAY].inthreeway = 0;
	
	if (!p->transfertobusy && ast->_state == AST_STATE_BUSY)
	{
	    dect_cc_reject(msg->hs_id, msg->cr);
	    goto Unlock3wayAndExit;
	}
	else if (p->transfermode == TRANSFER_BRIDGING)
	{
	    if (!dect_attempt_bridged_transfer(p))
	    {
		dect_cc_callstatus(msg->hs_id, msg->cr, msg->cr2,
		    CSTAT_TRANSFERRED);
		dect_set_release_timeout(p, dect_get_index(ast, p, 1));
	    }
	    else 
	    	dect_cc_reject(msg->hs_id, msg->cr);	    
	}
	else if (p->transfermode == TRANSFER_SIGNALLING)
	{
	    if (!dect_attempt_signaled_transfer(ast, inthreeway))
	    {
		dect_cc_callstatus(msg->hs_id, msg->cr, msg->cr2,
		    CSTAT_TRANSFERRED);
		dect_set_release_timeout(p, dect_get_index(ast, p, 1));
	    }
	    else
	    	dect_cc_reject(msg->hs_id ,msg->cr);	    
	}
    }
    else /* transfer disabled */
    {
	ast_log(LOG_DEBUG,"Call Transfer is disabled\n");
	dect_cc_reject(msg->hs_id ,msg->cr);
    }

Unlock3wayAndExit:
    if (p->subs[SUB_THREEWAY].owner)
    {
	ast_mutex_unlock(&p->subs[SUB_THREEWAY].owner->lock);
    }
    
Exit:
    ast_mutex_unlock(&ast->lock);
}

static void dect_handle_event(struct ast_channel *ast,	cc_message_t *msg)
{
    int index;
    struct dect_pvt *p = ast->tech_pvt;
    char dtmf_key;
    
    index = dect_get_index(ast, p, 0);
    if (index < 0)
	return; /* ignore */

    switch(msg->id) 
    {
    case CC_DIGIT:
	dtmf_key = msg->u.dialed_digits[0];
	ast_log(LOG_DEBUG, "DTMF pressed '%c'\n", dtmf_key);
	/* if 'R' key press */
	if (p->enable_flash_key && dtmf_key == CC_DIGIT_R_KEY)
	{
	    ast_log(LOG_DEBUG, "'R' key pressed\n");
	    if (p->subs[SUB_CALLWAIT].owner || !p->subs[SUB_REAL].inhold)
	    {
		/* put call on hold*/
		dect_hold_channel(p, ast, SUB_REAL);
		dect_cc_hold(p->hs_id, p->subs[SUB_REAL].callref);
	    }
	    else
	    {
		/* release call from hold*/
		dect_unhold_channel(p, ast, SUB_REAL);
    		dect_cc_unhold(p->hs_id, p->subs[SUB_REAL].callref);
	    }
	}
	else
	{
	    /* Sending DTMF_BEGIN and DTMF_END one after another will 
	       cause to minimal DTMF duration of 60 ms to be played */
	    dect_start_dtmf(ast, dtmf_key);
	}
	break;
    case CC_DISCONNECT:
	dect_play_tone(p, PHONE_TONE_NONE);
	dect_stop_audio(ast, index);
	/* XXX  Check if this call is required (considering conf call) */
	if (index == SUB_REAL && !p->subs[SUB_CALLWAIT].owner && 
	    !p->subs[SUB_THREEWAY].owner)
	{
	    dect_dsp_free(p);
	}
	dect_cc_release(p->hs_id, p->subs[index].callref, 
	    !p->subs[SUB_CALLWAIT].owner && !p->subs[SUB_THREEWAY].owner);
	p->subs[index].callref = 0;
	ast_setstate(ast, AST_STATE_DOWN);
	ast_queue_hangup(ast);
	break;
    case CC_SETUPACK:
    case CC_CONNECTACK:
	break;
    case CC_ALERTING:
	ast_setstate(ast, AST_STATE_RINGING);
	dect_queue_control(ast, AST_CONTROL_RINGING);
	break;
    case CC_CONNECT:
	/* At this point we already have shared DSP resources.
	 * We should do exclusive allocation now. */
	if (index == SUB_REAL)	/*not callwaiting*/
	{
	    dect_pvt_set_pref_codec(p, msg);

	    if (dect_dsp_exclusive_alloc(p, 0))
	    {
		/* Failed, the call was answered by someone else */
		/* TODO: Add release reason here */
		dect_cc_release(p->hs_id, p->subs[index].callref, 1);
		p->subs[index].callref = 0;

		ast_setstate(ast, AST_STATE_DOWN);
		ast_queue_hangup(ast);
		break;
	    }
	    p->real_dc_num = 0;

	    /* configure line according to dect capabilities */
	    dect_dsp_line_configure(p);
	}
	else
	{
	    if (index == SUB_CALLWAIT) /*in call waiting*/
	    {
		ast_log(LOG_DEBUG, "Got CC_CONNECT on call waiting. hs=%d;"
		    " switching calls...\n", p->hs_id);
		dect_callwait(p, 0);
		switch_calls(p, 1);
	    }
	}

	ast_log(LOG_DEBUG, "handset %u answered\n", p->hs_id);
	if (!p->confirmanswer) 
	{
	    ast_setstate(ast, AST_STATE_UP);
	    dect_queue_control(ast, AST_CONTROL_ANSWER);
	}
	break;
    case CC_HOLD:
	ast_log(LOG_DEBUG, "ast=%p, subs: %p, %p, %p\n", ast, p->subs[0].owner,
	    p->subs[1].owner, p->subs[2].owner);
	dect_hold_channel(p, ast, index);
	dect_cc_callstatus(p->hs_id, p->subs[index].callref, 0, CSTAT_HOLD);
	break;
    case CC_UNHOLD:
	ast_log(LOG_DEBUG, "p->owner=%p, ast=%p, subs: %p, %p, %p\n", p->owner,
	    ast, p->subs[0].owner, p->subs[1].owner, p->subs[2].owner);
	dect_cc_callstatus(p->hs_id, p->subs[index].callref, 0, CSTAT_CONNECT);

	/* if received unhold for the second call, switch to it */
	if (index != SUB_REAL) 
	{
	    ast_log(LOG_DEBUG, "Got CC_UNHOLD on active call. hs=%d;"
		" switching calls...\n", p->hs_id);
	    switch_calls(p, index == SUB_CALLWAIT);
	}

	/* unhold the active channel*/
	dect_unhold_channel(p, ast, SUB_REAL);
	dect_start_audio(ast, SUB_REAL);
	break;
    case CC_CALLSTATUS:
	ast_log(LOG_DEBUG, "Got CC_CALLSTATUS. hs=%d, cr=%d, status=%d\n",
	    p->hs_id, msg->cr, msg->u.call_status);
	if (!p->enable_flash_key)
	    break;
	/* call status is HOLD. happens after sending HOLD to handset*/
	if (msg->u.call_status == CSTAT_HOLD)
	{
	    /* if we have another call*/
	    if (p->subs[SUB_CALLWAIT].owner)
	    {
		/* if waiting call still not answered*/
		if (p->subs[SUB_CALLWAIT].owner->_state != AST_STATE_UP)
		{
		    ast_log(LOG_DEBUG, "connect waiting call. hs %d\n", 
			p->hs_id);
		    dect_cc_connect(p->hs_id,  p->subs[SUB_CALLWAIT].callref,
			p->codec);
		    dect_callwait(p, 0);
		    switch_calls(p, 1);
		    dect_start_audio(ast, SUB_REAL);
		}
		else
		{
		    ast_log(LOG_DEBUG, "make held call active. hs %d\n", 
			p->hs_id);
		    dect_cc_unhold(p->hs_id, p->subs[SUB_CALLWAIT].callref);
		    dect_unhold_channel(p, ast, SUB_CALLWAIT);
		}
	    }
	}
	/* call status is CONNECT. happens after sending UNHOLD to handset*/
	if (msg->u.call_status == CSTAT_CONNECT)
	{
	    /* stop playing call waiting tone - can happen when active call is
	    disconnected while the second call is held*/
	    if (p->subs[SUB_CALLWAIT].owner)
		switch_calls(p, 1);
	    dect_callwait(p, 0);
	    dect_start_audio(ast, SUB_REAL);
	}
	break;
    case CC_3PTY:
	if (p->threewayconference && 
	    p->subs[SUB_THREEWAY].owner && 
	    (p->subs[SUB_THREEWAY].owner->_state == AST_STATE_UP))
	{
	    ast_log(LOG_DEBUG, "allocting new dsp data channel\n");
	    if (dect_dc_exclusive_alloc(p, OTHER_DC_NUM(p->real_dc_num), 0))
	    {
		/* Failed to allocate data channel for
		* the conference, so just reject the conference request. */
		ast_log(LOG_DEBUG, "Failed to allocate data channel for conference. rejecting message\n");	
		dect_cc_reject(p->hs_id, msg->cr); 
	    }
	    else
	    {
		dect_cc_callstatus(p->hs_id, msg->cr, msg->cr2, CSTAT_CONFERENCE);
		start_conference(ast, p);		
	    }
	}
	else
	{
	    ast_log(LOG_DEBUG, "unable to start conference. rejecting message\n");	
	    dect_cc_reject(p->hs_id, msg->cr); 
	}
	break;
    case CC_TRANSFER:
	dect_handle_transfer(ast, msg);
	break;
    default:
	ast_log(LOG_DEBUG, "Dunno what to do with event \"%s\" on channel %d\n",/*jdsp_event2str(&ev)*/"CC_XX", p->hs_id);
    }
}

/*
   Handle CC_RELEASE, which means final call clear on DECT app side.
   This message is always sent as an answer to our CC_DISCONNECT or dect sent us
   a CC_TRANSFER event that was replied with CC_CALLSTATUS (CSTAT_TRANSFERRED), 
   it means that at this point our owner is always NULL. This is the time to
   switch to the waiting call or to release all DSP resources.
 */
static void dect_handle_release(struct dect_pvt *p, int index)
{
    int held_index;

    p->subs[index].callref = 0;
    
    /*if active call was released */
    if (index == SUB_REAL)
    {
	/*if has a second call, switch to it and start play call waiting 
	  tone until call is un-hold */
	if (p->subs[SUB_CALLWAIT].owner || p->subs[SUB_THREEWAY].owner)
	{
	    held_index = p->subs[SUB_CALLWAIT].owner? SUB_CALLWAIT : 
		SUB_THREEWAY;
	    ast_log(LOG_DEBUG, "active call hangup - switch to holded call\n");
	    /*  switch_calls(p, 1);*/
	    swap_subs(p, SUB_REAL, held_index);
	    unalloc_sub(p, held_index);	
	    p->owner = p->subs[SUB_REAL].owner;

	    /*now, the former call waiting is the only call we have. if
	      the call was not answered yet (still ringing), free the dsp line.
	    */
	    if (p->subs[SUB_REAL].owner->_state != AST_STATE_UP)
	    {
		ast_log(LOG_DEBUG, "second call still not answered. freeing "
		    "dsp line\n");
		dect_dsp_free(p);
	    }
	    else
	    {
		/* play call waiting tone*/
		dect_callwait(p, 1);
	    }
	}	
	else /*else, free dsp line*/
	{
	    ast_log(LOG_DEBUG, "no other sub channels - freeing dsp line\n");
	    dect_dsp_free(p);
	}
    }
    else
	unalloc_sub(p, index);
#if 0
    else if (p->subs[SUB_THREEWAY].owner) {
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
	    if (p->subs[SUB_THREEWAY].owner)
		ast_queue_hangup(p->subs[SUB_THREEWAY].owner);
	    p->subs[SUB_THREEWAY].owner->_softhangup |= AST_SOFTHANGUP_DEV;
	    ast_log(LOG_DEBUG, "Looks like a bounced flash, hanging up both calls on %d\n", p->channel);
	    ast_mutex_unlock(&p->subs[SUB_THREEWAY].owner->lock);
	} else if ((ast->pbx) || (ast->_state == AST_STATE_UP)) {
	    if (p->transfermode != TRANSFER_OFF) {
		int inthreeway = p->subs[SUB_REAL].inthreeway && p->subs[SUB_THREEWAY].inthreeway;

		/* In any case this isn't a threeway call anymore */
		p->subs[SUB_REAL].inthreeway = 0;
		p->subs[SUB_THREEWAY].inthreeway = 0;

		/* Only attempt transfer if the phone is ringing; why transfer to busy tone eh? */
		if (!p->transfertobusy && ast->_state == AST_STATE_BUSY) {
		    ast_mutex_unlock(&p->subs[SUB_THREEWAY].owner->lock);
		    /* Swap subs and dis-own channel */
		    swap_subs(p, SUB_THREEWAY, SUB_REAL);
		    p->owner = NULL;
		    /* Ring the phone */
		    dect_ring(p, 1);
		} else if (p->transfermode == TRANSFER_BRIDGING) {
		    if ((res = attempt_transfer(p)) < 0) {
			p->subs[SUB_THREEWAY].owner->_softhangup |= AST_SOFTHANGUP_DEV;
			if (p->subs[SUB_THREEWAY].owner)
			    ast_mutex_unlock(&p->subs[SUB_THREEWAY].owner->lock);
		    } else if (res) {
			/* Don't actually hang up at this point */
			if (p->subs[SUB_THREEWAY].owner)
			    ast_mutex_unlock(&p->subs[SUB_THREEWAY].owner->lock);
			break;
		    }
		} else if (p->transfermode == TRANSFER_SIGNALLING) {
		    struct ast_frame f;
		    /* XXX If transfer-target hasn't answered yet, 'target_chan' will be
		     * NULL and transfer will not be performed. We should add code that
		     * knows how to obtain 'target_chan' in this scenario, and perform
		     * blind transfer. */
		    struct ast_channel *target_chan;
		    int unlock_sub_index = SUB_THREEWAY;

		    /* If we are in a confernce call, undo the swap performed by
		     * start_conference. This is done in order to prevent the problem
		     * described in B39277. */
		    if (inthreeway) {
			swap_subs(p, SUB_THREEWAY, SUB_REAL);
			unlock_sub_index = SUB_REAL;
		    }

		    target_chan = ast_bridged_channel(p->subs[SUB_REAL].owner);
		    if (target_chan) {
			/* Tell bridged channel to do
			 * attended transfer */
			memset(&f, 0 , sizeof(f));
			f.frametype = AST_FRAME_ATTENDEDTRANSFER;
			f.data = &target_chan;
			f.datalen = sizeof(struct ast_channel *);
			ast_queue_frame(p->subs[SUB_THREEWAY].owner, &f);
			if (p->subs[unlock_sub_index].owner)
			    ast_mutex_unlock(&p->subs[unlock_sub_index].owner->lock);

			p->subs[index].f.frametype = AST_FRAME_NULL;
			p->subs[index].f.subclass = 0;

			return &p->subs[index].f;
		    } else {
			p->subs[SUB_THREEWAY].owner->_softhangup |= AST_SOFTHANGUP_DEV;
			if (p->subs[unlock_sub_index].owner)
			    ast_mutex_unlock(&p->subs[unlock_sub_index].owner->lock);
		    }
		}
	    } else {
		p->subs[SUB_THREEWAY].owner->_softhangup |= AST_SOFTHANGUP_DEV;
		if (p->subs[SUB_THREEWAY].owner)
		    ast_mutex_unlock(&p->subs[SUB_THREEWAY].owner->lock);
	    }
	} else {
	    ast_mutex_unlock(&p->subs[SUB_THREEWAY].owner->lock);
	    /* Swap subs and dis-own channel */
	    swap_subs(p, SUB_THREEWAY, SUB_REAL);
	    p->owner = NULL;
	    /* Ring the phone */
	    dect_ring(p, 1);
	}
    }
#endif
}

static void dect_handle_setup(struct dect_pvt *i, int index, cc_message_t *msg)
{
    pthread_t threadid;
    pthread_attr_t attr;
    struct ast_channel *chan = NULL;

    if(index == SUB_REAL)
    {
	dect_pvt_set_pref_codec(i, msg);

	if (dect_dsp_exclusive_alloc(i, 0))
	    goto Reject;

	/* configure line according to dect capabilities */
	dect_dsp_line_configure(i);

	i->real_dc_num = 0;
    }

    if (i->immediate || !ast_strlen_zero(msg->u.dialed_digits))
    {
	ast_log(LOG_DEBUG, "hs %u; imediate is %d; dialed is %s; "
	    "creating channel and running PBX\n", i->hs_id, i->immediate,
	    msg->u.dialed_digits);

	chan = dect_new(i, AST_STATE_RING, 0, index, 0);

	if (!chan) 
	    goto FreeAndReject;

	if (index == SUB_THREEWAY)
	{
	    switch_calls(i, 0);
	    index = SUB_REAL;	/*change index to real*/
	}

	if (!ast_strlen_zero(msg->u.dialed_digits))
	{
	    ast_copy_string(chan->exten, msg->u.dialed_digits,
		sizeof(chan->exten));
	    prepare_incoming_cid(i, chan);
	}

	if (ast_pbx_start(chan))
	{
	    ast_log(LOG_WARNING, "Unable to start PBX on %s\n", chan->name);
	    goto FreeAndReject;
	}
    }
    else 
    {
	/* Check for callerid, digits, etc */
	chan = dect_new(i, AST_STATE_RESERVED, 0, index, 0);
	if (!chan) 
	    goto FreeAndReject;

	if (index == SUB_THREEWAY)
	{
	    switch_calls(i, 0);
	    index = SUB_REAL;	/*change index to real*/
	}

	if (need_mwi_tone(i))
	    dect_play_tone(i, PHONE_TONE_MWI);
	else
	    dect_play_tone(i, i->dialtone);

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	if (ast_pthread_create(&threadid, &attr, ss_thread, chan)) 
	{
	    ast_log(LOG_ERROR, "Unable to start simple switch thread on hs "
		"%u\n", i->hs_id);
	    dect_play_tone(i, PHONE_TONE_REORDER);

	    goto FreeAndReject;
	}
    }

    dect_cc_setupack(i->hs_id, i->subs[index].callref,
	i->line->pcm_timeslot, i->codec);
    dect_cc_connect(i->hs_id, i->subs[index].callref, i->codec);

    return;

FreeAndReject:
    dect_dsp_free(i);
Reject:
    dect_cc_reject(i->hs_id, i->subs[index].callref);
    i->subs[index].callref = 0;
    if (chan)
	ast_hangup(chan);
}

/* If nobody owns us, absorb the event appropriately, otherwise
   we loop indefinitely.  This occurs when, during call waiting, the
   other end hangs up our channel so that it no longer exists, but we
   have neither FLASH'd nor ONHOOK'd to signify our desire to
   change to the other channel. 
 */
static void dect_no_owner_event_handle(struct dect_pvt *p, cc_message_t *msg)
{

    /* TODO: Call waiting support */
# if 0
    /* Switch to real if it is a PHONE_KEY_HOOK_OFF, a PHONE_KEY_HOOK_ON or a PHONE_KEY_FLASH event */
    if (ev.key == PHONE_KEY_HOOK_OFF || ev.key == PHONE_KEY_FLASH ||
	ev.key == PHONE_KEY_HOOK_ON)
    {
	ast_log(LOG_DEBUG, "Restoring owner of channel %d on event \"%s\"\n", p->channel, jdsp_event2str(&ev));
	p->owner = p->subs[SUB_REAL].owner;

	if (ev.key != DECT_CC_SETUP && p->owner && 
	    ast_bridged_channel(p->owner))
	{
	    dect_moh_stop(p, index);
	    dect_start_audio(p->subs[index].owner, index); /* XXX Need to check whether this line is needed */
	}

    }
    switch(ev.key) {
    case PHONE_KEY_HOOK_ON:
	dect_play_tone(p, PHONE_TONE_NONE);
	if (p->owner) {
	    if (option_verbose > 2) 
		ast_verbose(VERBOSE_PREFIX_3 "Channel %s still has call, ringing phone\n", p->owner->name);
	    dect_callwait(p, 0);
	    dect_ring(p, 1);
	    ast_setstate(p->owner, AST_STATE_RINGING);
	} else {
	    ast_log(LOG_WARNING, "Absorbed on hook, but nobody is left!?!?\n");
	    dect_dc_free(p, p->real_dc_num);
	}
	break;
    case PHONE_KEY_HOOK_OFF:
	if (p->owner && (p->owner->_state == AST_STATE_RINGING)) {
	    dect_ring(p, 0);
	    ast_setstate(p->owner, AST_STATE_UP);
	    p->subs[index].f.frametype = AST_FRAME_CONTROL;
	    p->subs[index].f.subclass = AST_CONTROL_ANSWER;
	}
	break;
    case PHONE_KEY_FLASH:
	p->flashtime = ast_tvfromboot();
	if (p->owner) {
	    if (option_verbose > 2) 
		ast_verbose(VERBOSE_PREFIX_3 "Channel %d flashed to other channel %s\n", p->channel, p->owner->name);
	    if (p->owner->_state != AST_STATE_UP) {
		/* Answer if necessary */
		usedindex = dect_get_index(p->owner, p, 0);
		if (usedindex > -1) {
		    p->subs[usedindex].needanswer = 1;
		}
		ast_setstate(p->owner, AST_STATE_UP);
	    }
	    dect_callwait(p, 0);
	    dect_play_tone(p, PHONE_TONE_NONE);

	} else
	    ast_log(LOG_WARNING, "Absorbed on hook, but nobody is left!?!?\n");
	break;
    default:
	ast_log(LOG_WARNING, "Don't know how to absorb event %s\n", jdsp_event2str(&ev));
    }
    f = &p->subs[index].f;
    return f;
#endif
}

/* 
   Handles messages that contains new call reference or REJECT.
   Returns index of sub channel that that the messages was intended
   for or -1. If -1 was returned, no further handling to be done.
 */
int dect_handle_new_callref(struct dect_pvt *p, cc_message_t *msg)
{
    int i = -1;

    if (!p->subs[SUB_REAL].callref)
	i = SUB_REAL;
    else if (p->subs[SUB_CALLWAIT].owner && !p->subs[SUB_CALLWAIT].callref)
    {
	ast_log(LOG_DEBUG, "hs %u; got %s for call waiting; "
	    "subchan real callref = %u\n", p->hs_id,
	    cc_msg_str_get(msg->id), p->subs[SUB_REAL].callref);
	i = SUB_CALLWAIT;
    }
    else
    {
	ast_log(LOG_DEBUG, "hs %u; got %s for \'threeway call\'; "
	    "callref = %u\n", p->hs_id, cc_msg_str_get(msg->id),
	    msg->cr);
	i = SUB_THREEWAY;
    }

    p->subs[i].callref = msg->cr;

    if (!p->subs[i].owner && msg->id == CC_SETUPACK)
    {
	/* we were disconnected before cr was set */
	dect_disconnect(p, i);
	ast_log(LOG_DEBUG, "Got CC_SETUPACK but owner is NULL; "
	    "sending CC_DISCONNECT\n");
	return -1;
    }

    return i;
}

static void dect_cc_handle(cc_message_t *msg)
{
    struct dect_pvt *p;
    int index = -1;

    p = dect_pvt_find(msg->hs_id);

    if (!p)
    {
	ast_log(LOG_ERROR, "%s: couldn't find pvt for hs_id %d\n",
	    cc_msg_str_get(msg->id), msg->hs_id);
	return;
    }
    ast_mutex_lock(&p->lock);

    ast_log(LOG_DEBUG, "hs_id %u: got msg %s; cr %u\n", msg->hs_id, 
	cc_msg_str_get(msg->id), msg->cr);

    if (msg->id == CC_SETUP || msg->id == CC_SETUPACK || msg->id == CC_REJECT)
    {
	if ((index = dect_handle_new_callref(p, msg)) < 0)
	    goto Exit;

	if (msg->id == CC_REJECT)
	{
	    dect_handle_release(p, index);
	    ast_queue_hangup(p->subs[index].owner);
	    ast_log(LOG_DEBUG, "dect %d; call on %s channel was rejected\n",
		p->hs_id, subnames[index]);
	    goto Exit;
	}
    }
    else
    {
	/* CC_DIGIT doesn't have callref since the button press can be related
	 * to any of the handset calls */
	if (msg->id == CC_DIGIT)
	    index = dect_get_index(p->owner, p, 0);
	else
	    index = dect_get_index_by_callref(p, msg->cr);

	if (index < 0)
	{
	    ast_log(LOG_ERROR, "%s: Didn't find channel with cr %d; "
		"pvt->cr %d\n", cc_msg_str_get(msg->id), msg->cr, 
		p->subs[SUB_REAL].callref);
	    goto Exit;
	}
    }

    if (msg->id == CC_SETUP)
    {
	dect_handle_setup(p, index, msg);
	goto Exit;
    }

    /* Subchannel owner can be NULL in following cases:
       1. Hangup called. In that situation we only wait for CC_RELEASE.
       2. Outgoing call, owner wasn't created yet.
       3. Transfer - channel was released
     */
    if (!p->subs[index].owner)
    {
	if (msg->id == CC_RELEASE) 
	{
	    void *timer_data = NULL;

	    /* Owner channel is already released, now finish 
	       releasing the call */
	    dect_handle_release(p, index);
	    ast_sched_del_and_get_data(sched, p->release_timer, &timer_data);
	    if (timer_data)
		free(timer_data);
	}
	else
	{
	    ast_log(LOG_ERROR, "%s: Owner is NULL for cr %d, index %d!\n",
		cc_msg_str_get(msg->id), msg->cr, index);
	}
	goto Exit;
    }

    if (!p->owner)
    {
	/* We can get here if the channel was hung up,
	   and we have waiting call */
	dect_no_owner_event_handle(p, msg);
	goto Exit;
    }

    dect_handle_event(p->subs[index].owner, msg);

Exit:
    ast_mutex_unlock(&p->lock);
}

static struct ast_frame *__dect_read(struct ast_channel *ast)
{
    struct dect_pvt *p = ast->tech_pvt;
    int res;
    int index;
    unsigned char *readbuf;
    phone_event_t ev;
    int fd = ast->fds[ast->fdno];
    int timestamp;

    index = dect_get_index(ast, p, 1);

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

    /* TODO: Call waiting support */
#if 0 
    if (p->subs[index].needanswer) {
	/* Send answer frame if requested */
	p->subs[index].needanswer = 0;
	p->subs[index].f.frametype = AST_FRAME_CONTROL;
	p->subs[index].f.subclass = AST_CONTROL_ANSWER;
	return &p->subs[index].f;
    }
#endif

    if (p->line && fd == p->line->fd)
    {
	if (dect_get_event(p->line->fd, &ev) < 0)
	{
	    ast_log(LOG_WARNING, "No event\n");
	    return NULL;
	}
	else
	{
	    ast_log(LOG_WARNING, "Unexpected DSP event %d; line %d(fd %d); "
		"hs_id %d\n", ev.key, p->line->lnum, p->line->fd, p->hs_id);
	}
	return &p->subs[index].f;
    }


    /* We got a voice packet */
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
    if (p->faxtxmethod == FAX_TX_T38_AUTO && p->faxdetected)
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
		jdsp_codec_rtp2ast(readbuf, &p->subs[index].f, res, readbuf);
    }

    return &p->subs[index].f;
}

struct ast_frame *dect_read(struct ast_channel *ast)
{
    struct dect_pvt *p = ast->tech_pvt;
    struct ast_frame *f;
    ast_mutex_lock(&p->lock);
    f = __dect_read(ast);
    ast_mutex_unlock(&p->lock);
    return f;
}

#if 0
static unsigned int calc_txstamp(struct dect_subchannel *s, struct timeval *delivery)
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

static int dect_write_voice_frame(struct dect_pvt *p, int index, struct ast_frame *frame)
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
	(p->subs[index].seqno + 1));
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

static int dect_write_modem_frame(struct dect_pvt *p, int index, struct ast_frame *frame)
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

static int dect_write(struct ast_channel *ast, struct ast_frame *frame)
{
    struct dect_pvt *p = ast->tech_pvt;
    int res = 0;
    int index;

    if (frame->frametype != AST_FRAME_CALLWAITING && frame->frametype != AST_FRAME_VOICE && frame->frametype != AST_FRAME_MODEM && frame->frametype != AST_FRAME_CNG)
	return 0;

    if (frame->frametype == AST_FRAME_CALLWAITING)
    {
	ast_log(LOG_DEBUG, "Got AST_FRAME_CALLWAITING on channel %s(%p)\n", 
	    ast->name, ast);
	ast_mutex_lock(&p->lock);
	if (frame->subclass == AST_CALLWAITING_STOP)
	    dect_callwait(p, 0);
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
	    dect_callwait(p, 1);
	}
	ast_mutex_unlock(&p->lock);
	return 0;
    }

    index = dect_get_index(ast, p, 0);
    if (index < 0) {
	ast_log(LOG_WARNING, "%s doesn't really exist?\n", ast->name);
	return -1;
    }

    /* Write a frame of (presumably voice) data */
    if (frame->frametype != AST_FRAME_VOICE && frame->frametype != AST_FRAME_MODEM && frame->frametype != AST_FRAME_CNG) {
	if (frame->frametype != AST_FRAME_IMAGE)
	    ast_log(LOG_WARNING, "Don't know what to do with frame type '%d'\n", frame->frametype);
	return 0;
    }
    /* Workaround for FAX/T.38, which happens to correspond to voice format G.723. */
    if ((frame->frametype == AST_FRAME_VOICE && !(frame->subclass & ast->nativeformats.audio_bits)) ||
	(frame->frametype == AST_FRAME_MODEM && frame->subclass == AST_MODEM_T38 &&
	p->faxtxmethod != FAX_TX_T38_AUTO)) {
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
	    dect_write_voice_frame(p, index, frame) :
	    dect_write_modem_frame(p, index, frame);
    }
    if (res < 0) {
	ast_log(LOG_WARNING, "write failed: %s\n", strerror(errno));
	return -1;
    } 
    return 0;
}

static int dect_indicate(struct ast_channel *chan, int condition)
{
    struct dect_pvt *p = chan->tech_pvt;
    int res = -1;
    int index;

    ast_mutex_lock(&p->lock);
    index = dect_get_index(chan, p, 0);
    ast_log(LOG_DEBUG, "Requested indication %d on channel %s\n", condition, chan->name);
    if (index == SUB_REAL) {
	switch(condition) {
	case AST_CONTROL_BUSY:
	    res = dect_play_tone(p, PHONE_TONE_BUSY);
	    break;
	case AST_CONTROL_RINGING:
	    res = dect_play_tone(p, PHONE_TONE_RING);
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
	    ast_log(LOG_DEBUG,"Received AST_CONTROL_PROGRESS on %s\n",chan->name);
	    dect_play_tone(p, PHONE_TONE_NONE);
	    /* don't continue in ast_indicate */
	    res = 0;
	    break;
	case AST_CONTROL_CONGESTION:
	case AST_CONTROL_NOANSWER:
	    chan->hangupcause = (condition ==  AST_CONTROL_NOANSWER) ?
		AST_CAUSE_NOANSWER : AST_CAUSE_CONGESTION;
	    res = dect_play_tone(p, PHONE_TONE_REORDER);
	    break;
	case -1:
	    res = dect_play_tone(p, PHONE_TONE_NONE);
	    break;
	}
    } else
	res = 0;
    ast_mutex_unlock(&p->lock);
    return res;
}

static struct ast_channel *dect_new(struct dect_pvt *i, int state, int startpbx, int index, int transfercapability)
{
    struct ast_channel *tmp;
    int deflaw = AST_FORMAT_ULAW;
    int x,y;
    if (i->subs[index].owner) {
	ast_log(LOG_WARNING, "Channel %d already has a %s call\n", i->hs_id,subnames[index]);
	return NULL;
    }
    tmp = ast_channel_alloc(1);
    if (tmp) {
	tmp->tech = &dect_tech;
	y = 1;
	do {
	    snprintf(tmp->name, sizeof(tmp->name), "dect/%d-%d", i->hs_id, y);
	    for (x = 0; x < 3; x++) {
		if ((index != x) && i->subs[x].owner && !strcasecmp(tmp->name, i->subs[x].owner->name))
		    break;
	    }
	    y++;
	} while (x < 3);
	tmp->type = type;
	if (index == SUB_REAL)
	{
	    tmp->fds[0] = i->subs[SUB_REAL].dfd;
	}
	if (index == SUB_THREEWAY)
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
	    ast_codec_pref_set2(&tmp->nativeformats, i->subs[SUB_REAL].owner->readformat);
	    pbx_builtin_setvar_helper(tmp, "_FORCECODEC", "");
	}
	else
	    memcpy(&tmp->nativeformats, &global_native_formats, sizeof(global_native_formats));

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
	if (startpbx) {
	    ast_log(LOG_DEBUG, "starting PBX...\n");
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

#if 0
static void openrg_cmd(char *cmd)
{
    manager_event(EVENT_FLAG_SYSTEM, "openrg_cmd", "%s\r\n", cmd);
}
#endif

#define CFWD_UNCONDITIONAL_ENTRY "call_forwarding_unconditional"
#define CFWD_BUSY_ENTRY "call_forwarding_on_busy"
#define CFWD_NO_ANSWER_ENTRY "call_forwarding_on_no_answer"

#if 0
static void configure_cfwd(int channel, char *entry, char *dest)
{
    char cmd[256];

    snprintf(cmd, sizeof(cmd), "conf set voip/line/%d/%s/enabled "
	"%d", channel - 1, entry, dest ? 1 : 0); 
    openrg_cmd(cmd);

    if (dest)
    {
	snprintf(cmd, sizeof(cmd), "conf set voip/line/%d/%s/"
	    "destination %s", channel - 1, entry, dest);
	openrg_cmd(cmd);
    }

    openrg_cmd("conf reconf 1");
}
#endif

#if 0
static int match_code(char *exten, char *code)
{
    return !ast_strlen_zero(code) && ast_extension_match(code, exten);
}

static char *get_cfwd_activate_type(struct dect_pvt *p, char *exten)
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

static char *get_cfwd_deactivate_type(struct dect_pvt *p, char *exten)
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
#endif

static void *ss_thread(void *data)
{
    struct ast_channel *chan = data;
    struct dect_pvt *p = chan->tech_pvt;
    char exten[AST_MAX_EXTENSION] = "";
    int end_dialing;
    int timeout;
#if 0
    char *getforward = NULL;
#endif
    int len = 0;
    int res;
    int index;
    int blindtransfer = 0;

    if (option_verbose > 2) 
    {
	ast_verbose( VERBOSE_PREFIX_3 "Starting simple switch on '%s'\n",
	    chan->name);
    }
    index = dect_get_index(chan, p, 1);
    if (index < 0)
    {
	ast_log(LOG_WARNING, "Huh?\n");
	ast_hangup(chan);
	return NULL;
    }

    /* Read the first digit */
    timeout = firstdigittimeout;

    /* If starting a threeway call, never timeout on the first digit so someone
       can use flash-hook as a "hold" feature */
    if (p->subs[SUB_THREEWAY].owner) 
	timeout = 999999;
    while(len < AST_MAX_EXTENSION-1) 
    {
#if 0
	char *cfwd_type = NULL;
#endif

	/* Read digit unless it's supposed to be immediate, in which case the
	   only answer is 's'. The 'immediate' flag affects only the
	   behavior after taking the phone off-hook, therefore it's
	   ignored in case of a follow call. */
	if ((p->immediate && !p->follow_call) || p->network_failure_disconnection)
	    res = 's';
	else
	    res = ast_waitfordigit(chan, timeout);
	end_dialing = 0;
	timeout = 0;
	if (res < 0) 
	{
	    ast_log(LOG_DEBUG, "waitfordigit returned < 0...\n");
	    res = dect_play_tone(p, PHONE_TONE_NONE);
	    ast_hangup(chan);
	    return NULL;
	}
	else if (res) 
	{
	    /* Don't treat # as dial terminator if it's the first dialed digit
	     * since some feature access codes use it */
	    if (res == '#' && len)
		end_dialing = 1;
	    else {
		exten[len++] = res;
		exten[len] = '\0';
	    }
	}
	else
	    ast_log(LOG_DEBUG, "waitfordigit: timeout expired\n");

	if (!ast_ignore_pattern(chan->context, exten))
	    dect_play_tone(p, PHONE_TONE_NONE);
	else
	    dect_play_tone(p, PHONE_TONE_DIAL);
	if (!strcmp(exten, "*98") && p->subs[SUB_THREEWAY].owner) 
	{
	    if (option_verbose > 2) 
	    {
		ast_verbose(VERBOSE_PREFIX_3 "Initiating blind transfer on "
		    "%s\n", chan->name);
	    }

	    /* Treat the next dialed number as a
	     * destination for blind transfer */
	    blindtransfer = 1;

	    /* XXX need secondary dial tone */
	    res = dect_play_tone(p, PHONE_TONE_DIAL); 			
	    if (res)
	    {
		ast_log(LOG_WARNING, "Unable to do dial recall on channel %s:"
		    "%s\n", chan->name, strerror(errno));
	    }
	    len = 0;
	    *exten = 0;
	    timeout = firstdigittimeout;
	}
#if 0
	else if ((cfwd_type = get_cfwd_activate_type(p, exten))) 
	{
	    dect_play_tone(p, PHONE_TONE_STUTTER_DIAL);
	    getforward = cfwd_type;
	    memset(exten, 0, sizeof(exten));
	    len = 0;
	}
	else if ((cfwd_type = get_cfwd_deactivate_type(p, exten))) 
	{
	    if (option_verbose > 2)
	    {
		ast_verbose(VERBOSE_PREFIX_3 "Cancelling call forwarding "
		    "unconditional on hs %d\n", p->hs_id);
	    }
	    dect_play_tone(p, PHONE_TONE_CONFIRM);
	    configure_cfwd(p->channel, cfwd_type, NULL); 
	    getforward = NULL;
	    memset(exten, 0, sizeof(exten));
	    len = 0;
	} 
	else if (match_code(exten, p->dnd_activate_code) ||
	    match_code(exten, p->dnd_deactivate_code)) 
	{
	    char cmd[256];

	    dect_play_tone(p, PHONE_TONE_CONFIRM);
	    snprintf(cmd, sizeof(cmd), "conf set "
		"voip/line/%d/do_not_disturb_enabled %d",
		p->channel - 1,
		match_code(exten, p->dnd_activate_code) ? 1 : 0); 
	    openrg_cmd(cmd);
	    openrg_cmd("conf reconf 1");

	    memset(exten, 0, sizeof(exten));
	    len = 0;
	}
#endif
	else if (end_dialing || ast_exists_extension(chan, chan->context, exten, 1, 
	    p->cid_num) || (!ast_strlen_zero(exten) && 
		strcmp(exten, ast_parking_ext()))) 
	{
	    if (end_dialing || !res || !ast_matchmore_extension(chan, chan->context, exten, 1, 
		p->cid_num))
	    {
#if 0
		if (getforward)
		{
		    /* Record this as the forwarding extension */
		    configure_cfwd(p->channel, getforward, exten); 
		    if (option_verbose > 2)
		    {
			ast_verbose(VERBOSE_PREFIX_3 "Setting call forward to "
			    "'%s' on channel %d\n", exten, p->channel);
		    }
		    dect_play_tone(p, PHONE_TONE_CONFIRM);
		    memset(exten, 0, sizeof(exten));
		    len = 0;
		    getforward = NULL;
		}
		else 
#endif
		if (blindtransfer) 
		{
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
		    timeout = firstdigittimeout;
		    dect_play_tone(p, PHONE_TONE_CONFIRM);
		}
		else
		{
		    res = dect_play_tone(p, PHONE_TONE_NONE);
		    ast_copy_string(chan->exten, exten, sizeof(chan->exten));
		    prepare_incoming_cid(p, chan);
		    ast_setstate(chan, AST_STATE_RING);
		    ast_log(LOG_DEBUG, "going to run PBX; exten \'%s\'\n",
			exten);
		    res = ast_pbx_run(chan);
		    if (res) 
		    {
			ast_log(LOG_WARNING, "PBX exited non-zero\n");
			res = dect_play_tone(p, PHONE_TONE_REORDER);
		    }
		    return NULL;
		}
	    } 
	    else
	    {
		/* It's a match, but they just typed a digit, and there is an
		 * ambiguous match, so just set the timeout to matchdigittimeout
		 * and wait some more */
		timeout = p->matchdigittimeout;
	    }
	} 
	else if (res == 0) 
	{
	    ast_log(LOG_DEBUG, "not enough digits (and no ambiguous "
		"match)...\n");
	    ast_hangup(chan);
	    return NULL;
	}
	else if (p->callwaiting && !strcmp(exten, "*70")) 
	{
	    if (option_verbose > 2) 
	    {
		ast_verbose(VERBOSE_PREFIX_3 "Disabling call waiting on %s\n",
		    chan->name);
	    }
	    /* Disable call waiting if enabled */
	    p->callwaiting = 0;
	    res = dect_play_tone(p, PHONE_TONE_DIAL /*DECT_TONE_DIALRECALL*/);
	    if (res) 
	    {
		ast_log(LOG_WARNING, "Unable to do dial recall on channel %s:"
		    "%s\n",  chan->name, strerror(errno));
	    }
	    len = 0;
	    memset(exten, 0, sizeof(exten));
	    timeout = firstdigittimeout;
	}
	else if (!strcmp(exten,ast_pickup_ext())) 
	{
	    /* Scan all channels and see if any there
	     * ringing channqels with that have call groups
	     * that equal this channels pickup group  
	     */
	    if (index == SUB_REAL) 
	    {
		/* Switch us from Third call to Call Wait */
		if (p->subs[SUB_THREEWAY].owner) 
		{
		    /* If you make a threeway call and the *8# a call, it should
		     * actually  look like a callwait */
		    swap_subs(p, SUB_CALLWAIT, SUB_THREEWAY);
		    unalloc_sub(p, SUB_THREEWAY);
		}
		if (ast_pickup_call(chan)) 
		{
		    ast_log(LOG_DEBUG, "No call pickup possible...\n");
		    res = dect_play_tone(p, PHONE_TONE_REORDER);
		}
		ast_hangup(chan);
		return NULL;
	    } 
	    else
	    {
		ast_log(LOG_WARNING, "Huh?  Got *8# on call not on real\n");
		ast_hangup(chan);
		return NULL;
	    }

	} 
	else if (!p->hidecallerid && !strcmp(exten, "*67")) 
	{
	    if (option_verbose > 2) 
	    {
		ast_verbose(VERBOSE_PREFIX_3 "Disabling Caller*ID on %s\n",
		    chan->name);
	    }

	    /* Disable Caller*ID if enabled */
	    p->hidecallerid = 1;
	    if (chan->cid.cid_num)
		free(chan->cid.cid_num);
	    chan->cid.cid_num = NULL;
	    if (chan->cid.cid_name)
		free(chan->cid.cid_name);
	    chan->cid.cid_name = NULL;
	    res = dect_play_tone(p, PHONE_TONE_DIAL /*DECT_TONE_DIALRECALL*/);
	    if (res) {
		ast_log(LOG_WARNING, "Unable to do dial recall on channel %s: %s\n", 
		    chan->name, strerror(errno));
	    }
	    len = 0;
	    memset(exten, 0, sizeof(exten));
	    timeout = firstdigittimeout;
	} 
	else if (p->callreturn && !strcmp(exten, "*69")) 
	{
	    res = 0;
	    if (!ast_strlen_zero(p->lastcid_num)) 
	    {
		res = ast_say_digit_str(chan, p->lastcid_num, "",
		    chan->language);
	    }
	    if (!res)
		res = dect_play_tone(p, PHONE_TONE_DIAL /*DECT_TONE_DIALRECALL*/);
	    goto Exit;
	} 
	else if ((p->transfermode == TRANSFER_BRIDGING || p->canpark) && 
	    !strcmp(exten, ast_parking_ext()) &&  p->subs[SUB_THREEWAY].owner &&
	    ast_bridged_channel(p->subs[SUB_THREEWAY].owner)) 
	{
	    /* This is a three way call, the main call being a real channel, 
	       and we're parking the first call. */
	    ast_masq_park_call(ast_bridged_channel(p->subs[SUB_THREEWAY].owner),
		chan, 0, NULL);
	    if (option_verbose > 2)
	    {
		ast_verbose(VERBOSE_PREFIX_3 "Parking call to '%s'\n",
		    chan->name);
	    }
	    goto Exit;
	} 
	else if (!ast_strlen_zero(p->lastcid_num) && !strcmp(exten, "*60")) 
	{
	    if (option_verbose > 2)
	    {
		ast_verbose(VERBOSE_PREFIX_3 "Blacklisting number %s\n",
		    p->lastcid_num);
	    }
	    res = ast_db_put("blacklist", p->lastcid_num, "1");
	    if (!res) 
	    {
		res = dect_play_tone(p, PHONE_TONE_DIAL /*DECT_TONE_DIALRECALL*/);
		memset(exten, 0, sizeof(exten));
		len = 0;
	    }
	} 
	else if (p->hidecallerid && !strcmp(exten, "*82")) 
	{
	    if (option_verbose > 2) 
	    {
		ast_verbose(VERBOSE_PREFIX_3 "Enabling Caller*ID on %s\n",
		    chan->name);
	    }

	    /* Enable Caller*ID if enabled */
	    p->hidecallerid = 0;
	    if (chan->cid.cid_num)
		free(chan->cid.cid_num);
	    chan->cid.cid_num = NULL;
	    if (chan->cid.cid_name)
		free(chan->cid.cid_name);
	    chan->cid.cid_name = NULL;
	    ast_set_callerid(chan, p->cid_num, p->cid_name, NULL);
	    res = dect_play_tone(p, PHONE_TONE_DIAL /*DECT_TONE_DIALRECALL*/);
	    if (res) 
	    {
		ast_log(LOG_WARNING, "Unable to do dial recall on channel %s:"
		    "%s\n", chan->name, strerror(errno));
	    }
	    len = 0;
	    memset(exten, 0, sizeof(exten));
	    timeout = firstdigittimeout;
	} 
	else if (!ast_canmatch_extension(chan, chan->context, exten, 1,
	    chan->cid.cid_num) && ((exten[0] != '*') || (strlen(exten) > 2))) 
	{
	    if (option_debug)
	    {
		ast_log(LOG_DEBUG, "Can't match %s from '%s' in context %s\n",
		    exten, chan->cid.cid_num ? chan->cid.cid_num : 
		    "<UnknownCaller>", chan->context);
	    }
	    goto Exit;
	}
	if (!timeout)
	    timeout = gendigittimeout;
	if (len && !ast_ignore_pattern(chan->context, exten))
	    dect_play_tone(p, PHONE_TONE_NONE);
    }

Exit:
    res = dect_play_tone(p, PHONE_TONE_REORDER);
    if (res < 0)
    {
	ast_log(LOG_WARNING, "Unable to play congestion tone on hs %u\n", 
	    p->hs_id);
    }
    ast_hangup(chan);
    return NULL;
}

#if 0
static void start_call(struct dect_pvt *i)
{
    pthread_t threadid;
    pthread_attr_t attr;
    struct ast_channel *chan;
    int res;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    /* Check for callerid, digits, etc */
    chan = dect_new(i, AST_STATE_RESERVED, 0, SUB_REAL, 0);
    if (chan) {
#if 0
	if (need_mwi_tone(i))
	    res = dect_play_tone(i, PHONE_TONE_MWI);
	else 
#endif
	    if (i->network_failure_disconnection)
	{
	    pbx_builtin_setvar_helper(chan, "SYSTEM_NETWORK_DISCONNECTION", "1");
	    res = dect_play_tone(i, PHONE_TONE_NONE);
	}
	else
	    res = dect_play_tone(i, i->dialtone);
	if (res < 0) 
	    ast_log(LOG_WARNING, "Unable to play dialtone on hs_id %d\n",
		i->hs_id);
	if (ast_pthread_create(&threadid, &attr, ss_thread, chan)) {
	    ast_log(LOG_WARNING, "Unable to start simple switch thread on channel %d\n", i->channel);
	    res = dect_play_tone(i, PHONE_TONE_REORDER);
	    if (res < 0)
		ast_log(LOG_WARNING, "Unable to play congestion tone on channel %d\n", i->channel);
	    ast_hangup(chan);
	}
    } else
	ast_log(LOG_WARNING, "Unable to create channel\n");
}
#endif

static void dect_handle_vmwi(struct dect_pvt *p)
{
    /* Save last state */
    p->msg_wait = p->next_msg_wait;
    /* send CC_MWI command to dect*/
    dect_cc_mwi(p->msg_wait, 0);
}

/* Update VMWI state for this line */
void dect_message_waiting_notify(char *username, int activate)
{
    struct dect_pvt *i;

    ast_mutex_lock(&iflock);

    /* Find username in list. */
    for (i = iflist; i; i = i->next)
    {
	if (i->mwi == MWI_EXTERNAL_GLOBAL || (i->mwi == MWI_EXTERNAL_PER_LINE &&
	    i->cid_num && !strcmp(i->cid_num, username)))
	{
	    i->next_msg_wait = activate;
	}

    }
    ast_mutex_unlock(&iflock);
}

static void *do_monitor(void *data)
{
    struct dect_pvt *i;
#if 0 // FXS
    int count, res, spoint, pollres = 0;
    time_t thispass = 0, lastpass = 0;
    int found;
    struct pollfd *pfds = NULL;
    int lastalloc = -1;
    /* This thread monitors all the frame relay interfaces which are not yet in use
       (and thus do not have a separate thread) indefinitely */
    /* From here on out, we die whenever asked */
#else
    int res;

    cc_read_id = ast_io_add(io, cc_sock, dect_cc_read_and_dispatch, AST_IO_IN,
	NULL);

#endif
    for(;;) {
#if 0 // FXS
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
	   dect_pvt that does not have an associated owner channel */
	count = 0;
	i = iflist;
	while(i) {
	    if ((i->jfd > -1)) {
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
#endif
	pthread_testcancel();
	res = ast_sched_wait(sched);
	if ((res < 0) || (res > 1000))
	    res = 1000;

#if 0 // FXS
	/* Wait at most a second for something to happen */
	res = poll(pfds, count, res);
	pthread_testcancel();
	/* Okay, poll has finished.  Let's see what happened.  */
	if (res < 0) {
	    if ((errno != EAGAIN) && (errno != EINTR))
		ast_log(LOG_WARNING, "poll return %d: %s\n", res, strerror(errno));
	    continue;
	}
#endif
	res = ast_io_wait(io, res);
	ast_mutex_lock(&monlock);
	ast_sched_runq(sched);
	ast_mutex_unlock(&monlock);
#if 0 // FXS
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
#endif		
	i = iflist;
	while(i) {
	    if (vmwi_changed(i)) {
		/* Update Visual Message Waiting Indication  */
		dect_handle_vmwi(i);
	    }
#if 0 //FXS
	    if ((i->jfd > -1)) {
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
		    if (dect_get_event(i->jfd, &ev) < 0)
			continue;

		    if (option_debug)
			ast_log(LOG_DEBUG, "Monitor got event %s on channel %d\n", jdsp_event2str(&ev), i->channel);
		    /* Don't hold iflock while handling init events -- race with chlock */
		    ast_mutex_unlock(&iflock);
		    handle_init_event(i, &ev);
		    ast_mutex_lock(&iflock);	
		}
	    }
#endif
	    i = i->next;
	}
#if 0 //FXS
	ast_mutex_unlock(&iflock);
#endif
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

static void dect_pvt_add(struct dect_pvt *p)
{
    struct dect_pvt **wlist;
    struct dect_pvt **wend;

    wlist = &iflist;
    wend = &ifend;

    /* nothing on the iflist */
    if (!*wlist) {
	*wlist = p;
	p->prev = NULL;
	p->next = NULL;
	*wend = p;
    } else {
	/* at least one member on the iflist */
	struct dect_pvt *working = *wlist;

	/* check if we maybe have to put it on the begining */
	if (working->hs_id > p->hs_id) {
	    p->next = *wlist;
	    p->prev = NULL;
	    (*wlist)->prev = p;
	    *wlist = p;
	} else {
	    /* go through all the members and put the member in the right place */
	    while (working) {
		/* in the middle */
		if (working->next) {
		    if (working->hs_id < p->hs_id && working->next->hs_id > p->hs_id) {
			p->next = working->next;
			p->prev = working;
			working->next->prev = p;
			working->next = p;
			break;
		    }
		} else {
		    /* the last */
		    if (working->hs_id < p->hs_id) {
			working->next = p;
			p->next = NULL;
			p->prev = working;
			*wend = p;
			break;
		    }
		}
		working = working->next;
	    }
	}
    }
}

static struct dect_pvt *mkintf(int hs_id)
{
    /* Make a dect_pvt structure for this interface (or CRV if "pri" is specified) */
    struct dect_pvt *tmp = NULL, *tmp2,  *prev = NULL;
    int here = 0;
    int x;
    struct dect_pvt **wlist;
    struct dect_pvt **wend;

    wlist = &iflist;
    wend = &ifend;

    tmp2 = *wlist;
    prev = NULL;

    while (tmp2) {
	if (tmp2->hs_id == hs_id) {
	    tmp = tmp2;
	    here = 1;
	    break;
	}
	if (tmp2->hs_id > hs_id) {
	    break;
	}
	prev = tmp2;
	tmp2 = tmp2->next;
    }

    ast_log(LOG_ERROR, "here %d; tmp2 0x%p; tmp 0x%p\n", here, tmp2, tmp);

    if (!here) {
	tmp = (struct dect_pvt*)malloc(sizeof(struct dect_pvt));
	if (!tmp) {
	    ast_log(LOG_ERROR, "MALLOC FAILED\n");
	    destroy_dect_pvt(&tmp);
	    return NULL;
	}
	ast_log(LOG_ERROR, "dect_pvt for hs %u allocated", hs_id);
	memset(tmp, 0, sizeof(struct dect_pvt));
	ast_mutex_init(&tmp->lock);
	ifcount++;
	tmp->line = NULL;
	tmp->offhookwarningschedid = -1;
	tmp->release_timer = -1;
	for (x = 0; x < 3; x++)
	{
	    tmp->subs[x].chan = -1;
	    tmp->subs[x].dfd = -1;
	}
	tmp->hs_id = hs_id;
	tmp->reload_count = global_reload_count;
	tmp->subs[SUB_REAL].dfd = dect_open_dsp();
	tmp->subs[SUB_THREEWAY].dfd = dect_open_dsp();
	dect_pvt_add(tmp);
    }
    else
	tmp->reload_count++;

    tmp->enabled = enabled;
    tmp->immediate = immediate;
    tmp->transfertobusy = transfertobusy;
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
    tmp->hs_id = hs_id;
    tmp->use_callerid = use_callerid;
    tmp->cid_signalling = cid_signalling;
    tmp->decttrcallerid = decttrcallerid;
    tmp->matchdigittimeout = matchdigittimeout;
    tmp->enable_flash_key = enable_flash_key;

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
    tmp->faxdetected = tmp->modemdetected = 0;
    tmp->follow_call = 0;
    tmp->network_failure_disconnection = 0;
    tmp->codec = DECT_CODEC_G726; /* default codec */

    return tmp;
}

static inline int available(struct dect_pvt *p, int devmatch, int groupmatch, int *busy, int *devmatched, int *groupmatched)
{
    /* First, check group matching */
    if (groupmatch) {
	if ((p->group & groupmatch) != groupmatch)
	    return 0;
	*groupmatched = 1;
    }
    /* Check to see if we have a hs_id match */
    if (devmatch != -1) {
	if (p->hs_id != devmatch)
	    return 0;
	*devmatched = 1;
    }
    /* We're at least busy at this point */
    if (busy) {
	*busy = 1;
    }
    /* If guard time, definitely not */
    if (p->guardtime && (time(NULL) < p->guardtime)) 
	return 0;

    if (!p->owner) 
    {
	if (p->subs[SUB_REAL].callref)
	    return 0; /* previous call wasn't released yet */

	return 1;
    }

    if (!p->callwaiting) {
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

static struct ast_channel *dect_request(const char *type, const struct ast_codec_pref *formats, void *data, int *cause)
{
    int groupmatch = 0;
    int channelmatch = -1;
    int roundrobin = 0;
    int callwait = 0;
    int busy = 0;
    struct dect_pvt *p;
    struct ast_channel *tmp = NULL;
    char *dest = NULL;
    int x;
    char *s;
    char opt = 0;
    int res = 0, y = 0;
    int backwards = 0;
    struct dect_pvt *exit, *start, *end;
    ast_mutex_t *lock;
    int channelmatched = 0;
    int groupmatched = 0;

    /* Assume we're locking the iflock */
    lock = &iflock;
    start = iflist;
    end = ifend;
    /* We do signed linear */
    if (data) {
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

	if (p && available(p, channelmatch, groupmatch, &busy, &channelmatched, &groupmatched)) {
	    if (option_debug)
		ast_log(LOG_DEBUG, "Using channel %d\n", p->hs_id);

	    callwait = (p->owner != NULL);
	    p->outgoing = 1;
	    tmp = dect_new(p, AST_STATE_RESERVED, 0, p->owner ? SUB_CALLWAIT : SUB_REAL, 0);
	    /* Make special notes */
	    ast_log(LOG_DEBUG, "new dect channel = %p\n", tmp);
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

    return tmp;
}

static int dect_destroy_channel(int fd, int argc, char **argv)
{
    int hs_id = 0;
    struct dect_pvt *tmp = NULL;
    struct dect_pvt *prev = NULL;

    if (argc != 4) {
	return RESULT_SHOWUSAGE;
    }
    hs_id = atoi(argv[3]);

    tmp = iflist;
    while (tmp) {
	if (tmp->hs_id == hs_id) {
	    destroy_channel(prev, tmp, 1);
	    return RESULT_SUCCESS;
	}
	prev = tmp;
	tmp = tmp->next;
    }
    return RESULT_FAILURE;
}

static char *alloc_stat_to_str(line_alloc_t stat)
{
    switch (stat)
    {
    case LINE_ALLOC_NONE: return "NONE";
    case LINE_ALLOC_SHARED: return "SHARED";
    case LINE_ALLOC_EXCLUSIVE: return "EXCLUSIVE";
    }

    return "";
}

static int dect_show_lines(int fd, int argc, char **argv)
{
#define FORMAT "%-12d %-10d %-15s %-13d"
#define FORMAT2 "%-12s %-10s %-15s %-13s%-20s\n"
    struct dect_pvt *tmp = NULL;
    int i;

    if (argc != 3)
	return RESULT_SHOWUSAGE;

    ast_cli(fd, FORMAT2, "Line num", "Timeslot", "Alloc status", "Ref. count", 
	"Referenced by");

    for (i = 0; i < MAX_DECT_DSP_LINES; i++)
    {
	ast_cli(fd, FORMAT, dsp_line[i].lnum, dsp_line[i].pcm_timeslot,
	    alloc_stat_to_str(dsp_line[i].alloc_status), dsp_line[i].ref_count);

	ast_mutex_lock(&iflock);
	tmp = iflist;
	while (tmp) {
	    if (tmp->line == &dsp_line[i])
		ast_cli(fd, "%u ", tmp->hs_id);
	    tmp = tmp->next;
	}
	ast_mutex_unlock(&iflock);

	ast_cli(fd, "\n");
    }

    return RESULT_SUCCESS;
#undef FORMAT
#undef FORMAT2
}

static int dect_play_tone_cmd(int fd, int argc, char **argv)
{
    int hs_id;
    int tone;
    struct dect_pvt *tmp = NULL;

    if (argc != 5)
	return RESULT_SHOWUSAGE;
    hs_id = atoi(argv[3]);
    tone = atoi(argv[4]);
    for (tmp = iflist; tmp && tmp->hs_id != hs_id; tmp = tmp->next);
    if (!tmp || !tmp->line)
	return RESULT_FAILURE;

    ioctl(tmp->line->fd, VOIP_LINE_TONE, tone);
    sleep(1);
    ioctl(tmp->line->fd, VOIP_LINE_TONE, PHONE_TONE_NONE);

    return RESULT_SUCCESS;
}

static int dect_show_channels(int fd, int argc, char **argv)
{
#define FORMAT "%7s %-10.10s %-15.15s %-10.10s %-20.20s\n"
#define FORMAT2 "%7s %-10.10s %-15.15s %-10.10s %-20.20s\n"
    struct dect_pvt *tmp = NULL;
    char tmps[20] = "";
    ast_mutex_t *lock;
    struct dect_pvt *start;

    lock = &iflock;
    start = iflist;

    if (argc != 3)
	return RESULT_SHOWUSAGE;

    ast_mutex_lock(lock);

    tmp = start;
    while (tmp) {
	snprintf(tmps, sizeof(tmps), "%d", tmp->hs_id);
	ast_cli(fd, FORMAT, tmps, tmp->exten, tmp->context, tmp->language, tmp->musicclass);
	tmp = tmp->next;
    }
    ast_mutex_unlock(lock);
    return RESULT_SUCCESS;
#undef FORMAT
#undef FORMAT2
}

static int dect_cli_cc_init(int fd, int argc, char **argv)
{
    dect_cc_init();
    is_cc_init = 1;

    ast_cli(fd, "DECT call control reinitialised\n");

    return RESULT_SUCCESS;
}

static int dect_show_hookstate(int fd, int argc, char **argv)
{
    struct dect_pvt *tmp = NULL;
    int isoffhook = 0;

    if (argc != 3)
	return RESULT_SHOWUSAGE;
    ast_mutex_lock(&iflock);
    for (tmp = iflist; tmp; tmp = tmp->next)
    {
#if 0
	if (ioctl(tmp->jfd, VOIP_SLIC_GET_HOOK, &isoffhook) < 0)
	    return RESULT_FAILURE;
#endif
	ast_cli(fd, "handset %d: %s hook\n", tmp->hs_id, isoffhook ? "off" : "on");
    }
    ast_mutex_unlock(&iflock);

    return RESULT_SUCCESS;
}

static char show_channels_usage[] =
"Usage: dect show channels\n"
"	Shows a list of available channels\n";

static char show_lines_usage[] =
"Usage: dect show lines\n"
"	Shows a list of DSP lines\n";

static char cc_init_usage[] =
"Usage: dect cc_init\n"
"	Reinitialize DECT call control\n";


static char destroy_channel_usage[] =
"Usage: dect destroy channel <chan num>\n"
"	DON'T USE THIS UNLESS YOU KNOW WHAT YOU ARE DOING.  Immediately removes a given channel, whether it is in use or not\n";

static char play_tone_usage[] =
"Usage: dect play tone <hs_id> <tone_num>\n";

static char show_hookstate_usage[] =
"Usage: dect show hookstate\n";

static struct ast_cli_entry dect_cli[] = {
    { {"dect", "show", "channels", NULL}, dect_show_channels,
	"Show active dect channels", show_channels_usage },
    { {"dect", "show", "lines", NULL}, dect_show_lines,
	"Show active dect channels", show_lines_usage },
    { {"dect", "destroy", "channel", NULL}, dect_destroy_channel,
	"Destroy a channel", destroy_channel_usage },
    { {"dect", "play", "tone", NULL}, dect_play_tone_cmd,
	"Play a tone", play_tone_usage },
    { {"dect", "show", "hookstate", NULL}, dect_show_hookstate,
	"Show hook state of all channels", show_hookstate_usage },
    { {"dect", "cc_init", NULL}, dect_cli_cc_init,
	"Destroy a channel", cc_init_usage },
};

#define TRANSFER	0
#define HANGUP		1

static int __unload_module(void)
{
    int x = 0;
    struct dect_pvt *p, *pl;
    memset(conf_file_md5, 0, MD5_DIGEST_LEN);
    ast_cli_unregister_multiple(dect_cli, sizeof(dect_cli) / sizeof(dect_cli[0]));
    ast_manager_unregister( "DECTDialOffhook" );
    ast_manager_unregister( "DECTHangup" );
    ast_manager_unregister( "DECTTransfer" );
    ast_manager_unregister( "DECTDNDoff" );
    ast_manager_unregister( "DECTDNDon" );
    ast_manager_unregister("DECTShowChannels");
    ast_channel_unregister(&dect_tech);
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

	    if (p->offhookwarningschedid > -1)
		ast_sched_del(sched, p->offhookwarningschedid);

	    if (p->release_timer > -1)
	    {
		void *data;

		ast_sched_del_and_get_data(sched, p->release_timer, &data);
		free(data);
	    }

	    pl = p;
	    p = p->next;
	    x++;
	    /* Free associated memory */
	    if(pl)
		destroy_dect_pvt(&pl);
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
    dect_cc_close();
    cc_sock = -1;
    ast_io_remove(io, cc_read_id);
    cc_read_id = NULL;
    for (x = 0; x < MAX_DECT_DSP_LINES; x++)
    {
	dect_close(dsp_line[x].fd);
	memset(&dsp_line[x], 0, sizeof(struct dect_dsp_line));
	dsp_line[x].fd = -1;
	dsp_line[x].lnum = -1;
    }

    return 0;
}

int unload_module()
{
    return __unload_module();
}

static struct dect_dsp_line *dsp_line_find(int lnum)
{
    int i;

    for (i = 0; i < MAX_DECT_DSP_LINES; i++)
	if (dsp_line[i].lnum == lnum)
	    return &dsp_line[i];

    return NULL;
}

static int dsp_line_parse_and_create(char *str, int timeslot, int reload)
{
    char *c;
    int lnum;
    struct dect_dsp_line *line;

    c = strsep(&str, ",");
    while(c)
    {
	if (!sscanf(c, "%d", &lnum)) 
	{
	    ast_log(LOG_ERROR, "Syntax error parsing line at '%s'\n", c);
	    return -1;
	}

	if (!reload) 
	{
	    /* Open new DSP line */
	    line = dsp_line_find(-1);
	    if (!line)
		return -1;
	    line->fd = dect_open_dsp_line(lnum);
	    if (line->fd < 0)
	    {
		ast_log(LOG_ERROR, "Unable to open DSP line %d: %s\n", lnum, 
		    strerror(errno));
		return -1;
	    }
	    line->lnum = lnum;
	}
	else /* Use existing line */
	    line = dsp_line_find(lnum); 

	if (line)
	{
	    line->pcm_timeslot = timeslot;

	    ast_log(LOG_DEBUG, "DECT DSP line %d initialized; fd %d; pcm %d\n",
		lnum, line->fd, line->pcm_timeslot);
	}
	else
	    ast_log(LOG_WARNING, "DECT DSP line %d ignored\n", lnum);

	c = strsep(&str, ",");
    }

    return 0;
}

static int handset_parse_and_create(char *str)
{
    struct dect_pvt *tmp;
    char *hs_id;
    int start, finish,x;

    hs_id = strsep(&str, ",");
    while(hs_id)
    {
	if (sscanf(hs_id, "%d-%d", &start, &finish) == 2) 
	{
	    /* Range */
	} else if (sscanf(hs_id, "%d", &start)) 
	{
	    /* Just one */
	    finish = start;
	}
	else
	{
	    ast_log(LOG_ERROR, "Syntax error parsing handset_id at '%s'\n", 
		hs_id);
	    return -1;
	}
	if (finish < start)
	{
	    ast_log(LOG_WARNING, "Sillyness: %d < %d\n", start, finish);
	    x = finish;
	    finish = start;
	    start = x;
	}
	for (x = start; x <= finish; x++) 
	{
	    tmp = mkintf(x);

	    if (tmp) 
	    {
		if (option_verbose > 2) 
		    ast_verbose(VERBOSE_PREFIX_3 "Registered handset hs_id %d\n", x);
	    }
	    else 
	    {
		ast_log(LOG_ERROR, "Unable to create handset %d\n", x);
		return -1;
	    }
	}
	hs_id = strsep(&str, ",");
    }

    return 0;
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

static int setup_dect(int reload)
{
    struct ast_config *cfg;
    struct ast_variable *v;
    int timeslot = -1;
    struct dect_pvt *curr, *tmp;

    cfg = ast_config_load(config);

    /* We *must* have a config file otherwise stop immediately */
    if (!cfg) {
	ast_log(LOG_ERROR, "Unable to load config %s\n", config);
	return -1;
    }

    memset(&global_native_formats, 0 , sizeof(global_native_formats));

    if (ast_mutex_lock(&iflock)) {
	/* It's a little silly to lock it, but we mind as well just to be sure */
	ast_log(LOG_ERROR, "Unable to lock interface list???\n");
	return -1;
    }
    global_reload_count++;

    v = ast_variable_browse(cfg, "dsp_lines");
    while(v) {
	if (!strcasecmp(v->name, "line")) {
	    if (dsp_line_parse_and_create(v->value, timeslot, reload)) {
		ast_config_destroy(cfg);
		ast_mutex_unlock(&iflock);
		return -1;
	    }
	} else if (!strcasecmp(v->name, "timeslot")) {
	    if (sscanf(v->value, "%d", &timeslot) != 1) {
		ast_log(LOG_ERROR, "Invalid timeslot: %s\n", v->value);
	    }
	}
	v = v->next;
    }

    v = ast_variable_browse(cfg, "handsets");
    while(v) {
	/* TODO: Delete handsets that were removed from conf */
	/* Create the interface list */
	if (!strcasecmp(v->name, "handset_id")) {
	    if (handset_parse_and_create(v->value))
	    {
		ast_config_destroy(cfg);
		ast_mutex_unlock(&iflock);
		return -1;
	    }
	}
	else if (!strcasecmp(v->name, "enabled")) {
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
	    if (!strcasecmp(v->value, "none"))
		dialtone = PHONE_TONE_NONE;
	    else if (!strcasecmp(v->value, "stutter"))
		dialtone = PHONE_TONE_STUTTER_DIAL;
	    else if (!strcasecmp(v->value, "normal"))
		dialtone = PHONE_TONE_DIAL;
	    else
		ast_log(LOG_WARNING, "Unknown dialtone type '%s'\n", v->value);
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
	} else if (!strcasecmp(v->name, "mailbox")) {
	    ast_copy_string(mailbox, v->value, sizeof(mailbox));
	} else if (!strcasecmp(v->name, "faxtxmethod")) {
	    if (!strcasecmp(v->value, "none"))
		faxtxmethod = FAX_TX_NONE;
	    else if (!strcasecmp(v->value, "t38_auto"))
		faxtxmethod = FAX_TX_T38_AUTO;
	    else if (!strcasecmp(v->value, "passthrough_auto"))
		faxtxmethod = FAX_TX_PASSTHROUGH_AUTO;
	    else if (!strcasecmp(v->value, "passthrough_force"))
		faxtxmethod = FAX_TX_PASSTHROUGH_FORCE;
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
	} else if (!strcasecmp(v->name, "transfertobusy")) {
	    transfertobusy = ast_true(v->value);
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
	} else if (!strcasecmp(v->name, "useincomingcalleridondecttransfer")) {
	    decttrcallerid = ast_true(v->value);
	} else if (!strcasecmp(v->name, "accountcode")) {
	    ast_copy_string(accountcode, v->value, sizeof(accountcode));
	} else if (!strcasecmp(v->name, "dialingtimeout")) {
	    matchdigittimeout = atoi(v->value);
	} else 
	    ast_log(LOG_WARNING, "Ignoring %s\n", v->name);
	v = v->next;
    }

    /* Destroy pvts of handsets that were removed from the conf */
    curr = iflist;
    while (curr)
    {
	tmp = curr;
	curr = curr->next;
	if (tmp->reload_count < global_reload_count)
	{
	    /* we expect from dect to disconnect the call before unregistering
	     * the handset */
	    if (tmp->owner || tmp->line || tmp->release_timer > -1 || 
		tmp->offhookwarningschedid > -1) 
	    {
		ast_log(LOG_ERROR, "Trying to remove handset %u in call!\n",
		    tmp->hs_id);
	    }
	    else
		destroy_channel(tmp->prev, tmp, 1);
	}
    }

    ast_mutex_unlock(&iflock);
    ast_config_destroy(cfg);

    if (!reload)
	cc_sock = dect_cc_open(dect_cc_handle);

    /* Initialize DECT call control only once when there are any DECT channels
     * exist */
    if (iflist && !is_cc_init)
    {
	dect_cc_init();
	is_cc_init = 1;
    }

    /* And start the monitor for the first time */
    restart_monitor();
    return 0;
}

int load_module(void)
{
    int res, i;

    sched = sched_context_create();
    if (!sched) {
	ast_log(LOG_ERROR, "Unable to create schedule context\n");
	return -1;
    }

    io = io_context_create();
    if (!io) {
	ast_log(LOG_WARNING, "Unable to create I/O context\n");
    }

    for (i = 0; i < MAX_DECT_DSP_LINES; i++)
    {
	memset(&dsp_line[i], 0, sizeof(struct dect_dsp_line));
	dsp_line[i].fd = -1;
	dsp_line[i].lnum = -1;
    }

    res = setup_dect(0);
    /* Make sure we can register our DECT channel type */
    if(res) {
	return -1;
    }
    if (ast_channel_register(&dect_tech)) {
	ast_log(LOG_ERROR, "Unable to register channel class %s\n", type);
	__unload_module();
	return -1;
    }
    ast_cli_register_multiple(dect_cli, sizeof(dect_cli) / sizeof(dect_cli[0]));

    memset(round_robin, 0, sizeof(round_robin));
    return res;
}

int _reload(int check_conf_file)
{
    int res = 0;

    if (!ast_config_file_md5_update(config, conf_file_md5) && check_conf_file)
    {
	ast_log(LOG_DEBUG, "Skipping reload since %s was not changed\n", config);
	return 0;
    }

    res = setup_dect(1);
    if (res) {
	ast_log(LOG_WARNING, "Reload of chan_dect.so is unsuccessful!\n");
	return -1;
    }
    return 0;
}

int reload(void)
{
    _reload(0);
}

int reload_if_changed(void)
{
    _reload(1);
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

static void dect_pre_bridge(struct ast_channel *chan)
{
    struct dect_pvt *p = chan->tech_pvt;
    int index;

    ast_mutex_lock(&p->lock);
    index = dect_get_index(chan, p, 0);

    if (index == SUB_REAL || p->subs[index].inthreeway)
	dect_start_audio(chan, index);
    ast_mutex_unlock(&p->lock);
}

static void dect_post_bridge(struct ast_channel *chan)
{
    struct dect_pvt *p = chan->tech_pvt;
    int index;

    ast_mutex_lock(&p->lock);
    index = dect_get_index(chan, p, 0);
    if (index == SUB_REAL || p->subs[index].inthreeway)
	dect_stop_audio(chan, index);
    ast_mutex_unlock(&p->lock);
}
