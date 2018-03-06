/****************************************************************************
 *
 * rg/pkg/voip/asterisk/include/asterisk/jdsp_common.h
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

#include <voip/dsp/phone.h>
#include <asterisk/frame.h>
#include <asterisk/channel.h>

typedef struct exten_num_list_t {
    struct exten_num_list_t *next;
    char *ext_num;
} exten_num_list_t;

typedef struct out_call_prefix_list_t {
    struct out_call_prefix_list_t *next;
    char *prefix;
} out_call_prefix_list_t;

#define RTP_HEADER_SIZE 12

/*! Chunk size to read -- we use 20ms chunks to make things happy.  */   
#define READ_SIZE (160 + RTP_HEADER_SIZE)


/* Returns the name of the event (for logs) */
char *jdsp_event2str(phone_event_t *ev);

/* Returns the char code of a DTMF event */
char jdsp_key2char(phone_key_t key);

int jdsp_codec_ast2rtp(int ast_format);

int jdsp_ecmode_ast2udptl(int ast_ecmode);

int jdsp_is_vad_disabled(int codec, char *annexb, struct ast_channel *ast, 
	int default_disable_vad);

int jdsp_conversionmode_ast2udptl(int ast_conversionMode);

int jdsp_ratemgt_ast2udptl(int ast_ratemgt);

int jdsp_codec_rtp2ast(unsigned char *rtpheader, struct ast_frame *f,
    int read_size, unsigned char *readbuf);

void jdsp_build_callid(char *callid, int len);

faxmethod_t jdsp_fax_method_parse(char *value);

fax_detection_method_t jdsp_fax_detection_method_parse(char *value);

int jdsp_is_internal_call_leg(struct ast_channel *transferee,
	struct ast_channel *transferer);

int jdsp_is_force_immediate(struct ast_channel *ast);

int jdsp_is_call_hold_enabled(struct ast_channel *ast);

int jdsp_is_call_waiting_enabled(struct ast_channel *ast);

int jdsp_is_supplementry_enabled(struct ast_channel *ast);

exten_num_list_t *jdsp_create_exten_num_list_from_conf(struct ast_variable *v);

void jdsp_free_exten_num_list(exten_num_list_t *exten_num_lst);

int jdsp_is_exten_num(exten_num_list_t *exten_num_lst, char *exten);

phone_tone_t jdsp_get_dialtone_from_str(char *tone_str);

/* Installs on the channel a generator that plays audio files.
 * 'files' is a list of ampersand separated files, after each of which
 * it is possible to specify an ms delay, using a comma.
 * The delay is performed after playing of the respective file is done.
 * e.g.: "file1&file2,3000&file3,1500&file4".
 * Upon finish, 'finish_cb' is called, if supplied. */
void jdsp_install_play_generator(struct ast_channel *chan, char *files,
    void (*finish_cb)(struct ast_channel *));

out_call_prefix_list_t *jdsp_create_out_call_prefix_list_from_conf(
    struct ast_variable *v);

void jdsp_free_out_call_prefix_list(out_call_prefix_list_t *prefix_list);

int jdsp_has_out_call_prefix(out_call_prefix_list_t *prefix_list, char *exten);
