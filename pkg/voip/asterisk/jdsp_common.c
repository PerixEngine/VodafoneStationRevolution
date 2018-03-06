/****************************************************************************
 *
 * rg/pkg/voip/asterisk/jdsp_common.c
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

#include <stdio.h>
#include <stdlib.h>
#include "asterisk/jdsp_common.h"
#include "asterisk/channel.h"
#include "asterisk/pbx.h"
#include "asterisk/utils.h"
#include "asterisk/file.h"
#if defined(T38_SUPPORT)
#include "asterisk/udptl.h"
#endif

code2str_t detection_methods[] = {
    {FAX_DETECTION_ORIGINATING, "originating"},
    {FAX_DETECTION_TERMINATING, "terminating"},
    {FAX_DETECTION_BOTH, "both"},
    {FAX_DETECTION_NONE, "none"},
    {-1}
};

code2str_t fax_methods[] = {
    {FAX_T38_AUTO, "t38_auto"},
    {FAX_NONE, "none"},
    {FAX_PASSTHROUGH_AUTO, "passthrough_auto"},
    {FAX_PASSTHROUGH_FORCE, "passthrough_force"},
    {-1}
};

static int codec_rtp2ast[][2] = {
	{ JRTP_PAYLOAD_PCMU, AST_FORMAT_ULAW },
	{ JRTP_PAYLOAD_PCMA, AST_FORMAT_ALAW },
	{ JRTP_PAYLOAD_G729, AST_FORMAT_G729A },
	{ JRTP_PAYLOAD_G726_32, AST_FORMAT_G726 },
	{ JRTP_PAYLOAD_G723, AST_FORMAT_G723_1 },
	{ JRTP_PAYLOAD_G722, AST_FORMAT_G722 },
	{ JRTP_PAYLOAD_LINPCM, AST_FORMAT_SLINEAR },
	{ JRTP_PAYLOAD_CN, AST_FORMAT_CN},
	{ -1, -1 }
};

static int ratemgt_udptl2ast[][2] = {
	{ JUDPTL_LOC_TCF, UDPTL_LOC_TCF},
	{ JUDPTL_TRANS_TCF, UDPTL_TRANS_TCF},
	{-1, -1}
};

static int conversionmode_udptl2ast[][2] = {
	{ JUDPTL_CONVERSION_NONE, UDPTL_CONVERSION_NONE },
	{ JUDPTL_FILL_BIT_REMOVAL, UDPTL_FILL_BIT_REMOVAL},
	{ JUDPTL_TRANSCODING_MMR, UDPTL_TRANSCODING_MMR},
	{ JUDPTL_TRANSCODING_JBIG, UDPTL_TRANSCODING_JBIG},
	{-1, -1}
};



static int ecmode_udptl2ast[][2] = {
	{ JUDPTL_EC_NONE, UDPTL_ERROR_CORRECTION_NONE },
	{ JUDPTL_EC_FEC, UDPTL_ERROR_CORRECTION_FEC },
	{ JUDPTL_EC_REDUNDANCY, UDPTL_ERROR_CORRECTION_REDUNDANCY },
	{ -1, -1 }
};

static char *events[] = {
	[PHONE_KEY_0] = "DTMF key 0",
	[PHONE_KEY_1] = "DTMF key 1",
	[PHONE_KEY_2] = "DTMF key 2",
	[PHONE_KEY_3] = "DTMF key 3",
	[PHONE_KEY_4] = "DTMF key 4",
	[PHONE_KEY_5] = "DTMF key 5",
	[PHONE_KEY_6] = "DTMF key 6",
	[PHONE_KEY_7] = "DTMF key 7",
	[PHONE_KEY_8] = "DTMF key 8",
	[PHONE_KEY_9] = "DTMF key 9",
	[PHONE_KEY_A] = "DTMF key A",
	[PHONE_KEY_B] = "DTMF key B",
	[PHONE_KEY_C] = "DTMF key C",
	[PHONE_KEY_D] = "DTMF key D",
	[PHONE_KEY_ASTERISK] = "DTMF key *",
	[PHONE_KEY_POUND] = "DTMF key #",

	[PHONE_KEY_HOOK_ON] = "Hook on",
	[PHONE_KEY_HOOK_OFF] = "Hook off",
	[PHONE_KEY_FLASH] = "Flash",
	[PHONE_KEY_FAX_CNG] = "Local fax calling tone",
	[PHONE_KEY_FAXMODEM_CED] = "Local fax / modem answering tone",
	[PHONE_KEY_RING_ON] = "Ring started",
	[PHONE_KEY_RING_OFF] = "Ring stopped",
	[PHONE_KEY_RING_TRYING] = "Ring trying",
	[PHONE_KEY_FAX_DIS] = "Fax Dis tone",
	[PHONE_KEY_FAX_NET_CNG] = "Net fax calling tone",
	[PHONE_KEY_FAXMODEM_AM] = "Amplitue modulation answering tone",
	[PHONE_KEY_FAXMODEM_PR] = "Phase reversal answering tone",
	[PHONE_KEY_FAXMODEM_NET_CED] = "Net fax calling tone",
	[PHONE_KEY_MODEM_CNG] = "Local modem calling tone",
	[PHONE_KEY_PORT_ATTACHED] = "Phone is connected to FXS port",
	[PHONE_KEY_PORT_DETACHED] = "Phone is not connected to FXS port",
};

static int jdsp_get_generic_bool_var(struct ast_channel *ast, char *var);

int jdsp_is_vad_disabled(int codec, char *annexb, struct ast_channel *ast, 
	int default_disable_vad)
{
	int ret;

	if (jdsp_get_generic_bool_var(ast, "DISABLE_VAD"))
	        return 1;

	if (codec == JRTP_PAYLOAD_G729)
		ret = annexb && !strcmp(annexb, "NO");
	/* Our bridge (Usually SIP) performs the CN negotiation, so we check it for
	   format existance */
	else if (ast_bridged_channel(ast) &&
		ast_bridged_channel(ast)->nativeformats.audio_bits & AST_FORMAT_CN)
	{
		ret = 0;
	}
	else
		ret = default_disable_vad;

	ast_log(LOG_DEBUG, "Disable Vad - %d\n", ret);

	return ret;
}

char *jdsp_event2str(phone_event_t *ev)
{
	static char buf[256];
	char *event_name = NULL;

	if (ev->key >= 0 && ev->key < sizeof(events)/sizeof(events[0]))
		event_name = events[ev->key];

	/* Pay attention not all elements of events[] are initialized */
	if (!event_name)
		event_name = "Unknown";

	sprintf(buf, "%s(%d) %s", event_name, ev->key, ev->pressed ? "pressed" :
		"released");
	if (ev->key <= PHONE_KEY_POUND)
		logger_hide_numbers(buf, 1);

	return buf;
}

char jdsp_key2char(phone_key_t key)
{
	switch (key)
	{
	case PHONE_KEY_ASTERISK:
		return '*';
	case PHONE_KEY_POUND:
		return '#';
	case PHONE_KEY_A:
	case PHONE_KEY_B:
	case PHONE_KEY_C:
	case PHONE_KEY_D:
		return key - PHONE_KEY_A + 'A';
	default:
		/* We assume that argument "key" is one of the DTMF tones */
		return key - PHONE_KEY_0 + '0';
	}
}

int jdsp_codec_ast2rtp(int ast_format)
{
	int i;

	for (i = 0; codec_rtp2ast[i][0] != -1; i++ )
	{
		if (codec_rtp2ast[i][1] == ast_format)
			return codec_rtp2ast[i][0];
	}
	ast_log(LOG_ERROR, "Unknown AST format %d\n", ast_format);
	/* In the error case return default value (PCMU) */
	return 0;
}

int jdsp_ratemgt_ast2udptl(int ast_ratemgt)
{
	int i;

	for (i = 0; ratemgt_udptl2ast[i][0] != -1; i++ )
	{
		if (ratemgt_udptl2ast[i][1] == ast_ratemgt)
			return ratemgt_udptl2ast[i][0];
	}
	ast_log(LOG_ERROR, "Unknown rate management function %d\n", ast_ratemgt);

	/* if error, return transfered function */
	return 1;
}

int jdsp_conversionmode_ast2udptl(int ast_conversionMode)
{
	int i;
	int retVal = 0;

	for (i = 0; conversionmode_udptl2ast[i][0] != -1; i++ )
	{
		if (conversionmode_udptl2ast[i][1] & ast_conversionMode)
			retVal |= conversionmode_udptl2ast[i][1]; 
	}

	if(!retVal)
		ast_log(LOG_ERROR, "Unknown conversion mode %d\n", ast_conversionMode);

	return retVal;
}


int jdsp_ecmode_ast2udptl(int ast_ecMode)
{
	int i;

	for (i = 0; ecmode_udptl2ast[i][0] != -1; i++ )
	{
		if (ecmode_udptl2ast[i][1] == ast_ecMode)
			return ecmode_udptl2ast[i][0];
	}
	ast_log(LOG_ERROR, "Unknown Error correction mode %d\n", ast_ecMode);

	/* if error, return ec none */
	return 0;
}

int jdsp_codec_rtp2ast(unsigned char *rtpheader, struct ast_frame *f,
    int read_size, unsigned char *readbuf)
{
	int i;
	int payloadtype;

	payloadtype = (ntohl(*(unsigned int *)rtpheader) & 0x7f0000) >> 16;

	if (payloadtype == JRTP_PAYLOAD_CN)
	{
	    f->frametype = AST_FRAME_CNG;
	    f->subclass = payloadtype;
	}
	else
	{
	    for (i = 0; codec_rtp2ast[i][0] != -1; i++)
	    {
		if (codec_rtp2ast[i][0] == payloadtype)
		{
		    f->subclass = codec_rtp2ast[i][1];
		    break;
		}
	    }
	    if (codec_rtp2ast[i][0] == -1)
	    {
		/* This is not really error case. Received RTP packet can contain non voice
		 * data */
		ast_log(LOG_DEBUG, "Unknown RTP payload type %d\n", payloadtype);
		return -1;
	    }
	    f->frametype = AST_FRAME_VOICE;
	    f->datalen = read_size - RTP_HEADER_SIZE;
	    f->offset = AST_FRIENDLY_OFFSET + RTP_HEADER_SIZE;
	    f->data = readbuf + RTP_HEADER_SIZE;
	    f->samples = ast_codec_get_samples(f);
	}

	return 0;
}

/* Returns a unique call id in pre-allocated string "callid" */
void jdsp_build_callid(char *callid, int len)
{
	int res;
	int val;
	int x;

	for (x = 0; x < 4 && len > 0; x++) {
		val = rand();
		res = snprintf(callid, len, "%08x", val);
		len -= res;
		callid += res;
	}
}

fax_detection_method_t jdsp_fax_detection_method_parse(char *value)
{
	return str2code(detection_methods, value);
}

faxmethod_t jdsp_fax_method_parse(char *value)
{
	return str2code(fax_methods, value);
}

int jdsp_is_internal_call_leg(struct ast_channel *transferee,
	struct ast_channel *transferer)
{
	if (!transferee)
	{
		ast_log(LOG_ERROR, "no bridge channel for held call - something wrong"
			" happend!\n");
		return 0;
	}

	if (strncasecmp(transferee->type, "jdsp",
		sizeof("jdsp")-1) && strncasecmp(transferee->type, "capi",
		sizeof("capi")-1) && !pbx_builtin_getvar_helper(transferer,
		"TO_EXTENSION"))
	{
		ast_log(LOG_DEBUG, "neither of the call legs are internal\n");
		return 0;
	}
	return 1;
}

static int jdsp_get_generic_bool_var(struct ast_channel *ast, char *var)
{
    char *retval;
    
    if (!ast || !var)
	return 0;

    retval = pbx_builtin_getvar_helper(ast, var);

    if (!retval)
	return 0;

    return !strcmp(retval, "YES") || !strcmp(retval, "1");
}

int jdsp_is_force_immediate(struct ast_channel *ast)
{
    return jdsp_get_generic_bool_var(ast, "FORCE_IMMEDIATE");
}

int jdsp_is_call_hold_enabled(struct ast_channel *ast)
{
    return !jdsp_get_generic_bool_var(ast, "DISABLE_CALL_HOLD");
}

int jdsp_is_call_waiting_enabled(struct ast_channel *ast)
{
    return !jdsp_get_generic_bool_var(ast, "DISABLE_CALL_WAITING");
}

int jdsp_is_supplementry_enabled(struct ast_channel *ast)
{
    return !jdsp_get_generic_bool_var(ast, "DISABLE_SUPPLEMENTRY");
}

exten_num_list_t *jdsp_create_exten_num_list_from_conf(struct ast_variable *v)
{
    exten_num_list_t *head = NULL, **it = &head;

    for (; v; v = v->next, it = &(*it)->next)
    {
	*it = malloc(sizeof(exten_num_list_t));
	memset(*it, 0, sizeof(exten_num_list_t));
	(*it)->ext_num = strdup(v->value);
    }

    return head;
}

void jdsp_free_exten_num_list(exten_num_list_t *list)
{
    exten_num_list_t *tmp;

    while (list)
    {
	tmp = list;
	free(list->ext_num);
	list = list->next;
	free(tmp);
    }
}

int jdsp_is_exten_num(exten_num_list_t *list, char *exten)
{
    char *cur_ext;
    char cur_ext_pound[AST_MAX_EXTENSION];

    for (; list; list = list->next)
    {
	cur_ext = list->ext_num;

	snprintf(cur_ext_pound, sizeof(cur_ext_pound), "%s#", cur_ext);
        
	if (!strcmp(cur_ext, exten) || !strcmp(cur_ext_pound, exten))
	    return 1;
    }

    return 0;
}

phone_tone_t jdsp_get_dialtone_from_str(char *tone_str)
{
    if (!strcasecmp(tone_str, "none"))
	return PHONE_TONE_NONE;
    else if (!strcasecmp(tone_str, "stutter"))
	return PHONE_TONE_STUTTER_DIAL;
    else if (!strcasecmp(tone_str, "normal"))
	return PHONE_TONE_DIAL;
    else if (!strcasecmp(tone_str, "backup"))
	return PHONE_TONE_3G_BACKUP;
    else if (!strcasecmp(tone_str, "reorder"))
	return PHONE_TONE_REORDER;
    else if (!strcasecmp(tone_str, "reorder_short"))
	return PHONE_TONE_REORDER_SHORT;
    else if (!strcasecmp(tone_str, "cfwd"))
	return PHONE_TONE_CFWD;

    ast_log(LOG_WARNING, "Unknown dialtone type '%s'\n", tone_str);

    return PHONE_TONE_DIAL;
}

typedef struct
{
    char *files;
    void (*finish_cb)(struct ast_channel *chan);
} play_gen_params;

typedef struct {
    char *files;
    char *files_orig;
    int post_delay;
    int sched_id;
    int phase;
    void (*finish_cb)(struct ast_channel *chan);
} play_gen_ctx;

static void *play_gen_alloc(struct ast_channel *chan, void *gen_params)
{
    play_gen_ctx *ctx;
    play_gen_params *params = (play_gen_params *)gen_params;
 
    ctx = malloc(sizeof(play_gen_ctx));

    memset(ctx, 0, sizeof(play_gen_ctx));
    ctx->files_orig = ctx->files = strdup(params->files);
    ctx->finish_cb = params->finish_cb;
    ctx->sched_id = -1;

    ast_log(LOG_DEBUG, "play generator: started on %s with file list: %s\n",
	chan->name, ctx->files);

    return ctx;
}

static void play_gen_release(struct ast_channel *chan, void *data)
{
    play_gen_ctx *ctx = data;

    ast_log(LOG_DEBUG, "play generator: finished on %s\n", chan->name);

    if (chan->sched && ctx->sched_id != -1)
	ast_sched_del(chan->sched, ctx->sched_id);
    ast_stopstream(chan);
    if (ctx->finish_cb)
	ctx->finish_cb(chan);
    free(ctx->files_orig);
    free(ctx);
}

static int play_gen_delay_cb(void *data)
{
    play_gen_ctx *ctx = (play_gen_ctx *)data;

    ctx->sched_id = -1;
    ctx->phase = 0;

    return 0;
}

static int play_gen_generate(struct ast_channel *chan, void *data, int len,
    int samples)
{
    char *c, *file;
    play_gen_ctx *ctx = (play_gen_ctx *)data;

    switch (ctx->phase)
    {
    /* starting to play a new file */
    case 0:
	/* no more files to play */
	if (!(file = ctx->files))
	    return -1;

	if ((c = strchr(file, '&')))
	{
	    *c = '\0';
	    ctx->files = c + 1;
	}
	else
	    ctx->files = NULL;

	if ((c = strchr(file, ',')))
	{
	    *c = '\0';
	    ctx->post_delay = atoi(c + 1);
	}
	else
	    ctx->post_delay = 0;

	ast_log(LOG_DEBUG, "play generator: playing %s on %s with delay "
	    "of %dms\n", file, chan->name, ctx->post_delay);
	ast_stopstream(chan);
	if (ast_streamfile(chan, file, chan->language))
	{
	    ast_log(LOG_WARNING, "play generator: ast_streamfile failed on "
		"%s for %s\n", chan->name, file);
	    return -1;
	}
	ctx->phase = 1;
	break;
    /* playing... */
    case 1:
        if (chan->streamid == -1)
	{ /* we are done playing the current file */
	    ast_stopstream(chan);
	    if (ctx->post_delay)
	    {
		ctx->sched_id = ast_sched_add(chan->sched, ctx->post_delay,
		    play_gen_delay_cb, ctx);
		ctx->phase = 2;
		ast_log(LOG_DEBUG, "play generator: entering a delay of "
		    "%dms on %s\n", ctx->post_delay, chan->name);
	    }
	    else
		ctx->phase = 0;
	}
	break;
    /* waiting for the post-delay to expire */
    case 2:
	break;
    }

    ast_sched_runq(chan->sched);
    return 0;
}

struct ast_generator play_gen = {
    .alloc = play_gen_alloc,
    .release = play_gen_release,
    .generate = play_gen_generate,
};

void jdsp_install_play_generator(struct ast_channel *chan, char *files,
    void (*finish_cb)(struct ast_channel *))
{
    play_gen_params gp = { };

    gp.files = files;
    gp.finish_cb = finish_cb;
    ast_activate_generator(chan, &play_gen, &gp);
}

out_call_prefix_list_t *jdsp_create_out_call_prefix_list_from_conf(
    struct ast_variable *v)
{
    out_call_prefix_list_t *head = NULL, **it = &head;

    for (; v; v = v->next, it = &(*it)->next)
    {
	*it = calloc(1, sizeof(out_call_prefix_list_t));
	(*it)->prefix = strdup(v->value);
    }

    return head;
}

void jdsp_free_out_call_prefix_list(out_call_prefix_list_t *list)
{
    out_call_prefix_list_t *tmp;

    while (list)
    {
	tmp = list;
	free(list->prefix);
	list = list->next;
	free(tmp);
    }
}

int jdsp_has_out_call_prefix(out_call_prefix_list_t *list, char *exten)
{
    for (; list; list = list->next)
    {
	if (!strncmp(exten, list->prefix, strlen(list->prefix)))
	    return 1;
    }

    return 0;
}

