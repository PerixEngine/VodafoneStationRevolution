/****************************************************************************
 *
 * rg/pkg/voip/asterisk/incall_announcement.c
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
#include <sys/stat.h>
#include "asterisk/incall_announcement.h"
#include "asterisk/jdsp_common.h"
#include "asterisk/manager.h"

#define MAX_PLAY_FILE_READ_RETRIES 10
#define PLAY_FILE_READ_RETRY_GAP 300 /* ms */

#define PLAY_FILE "/etc/asterisk/diagnostics.play"

static struct ctx_t {
    struct ast_channel *chan;
    struct ast_channel **pvt_owner;
    ast_mutex_t *pvt_lock;
    void (*modify_audio_proc)(struct ast_channel *, int);
    int modify_audio_param;
    int retries_left;
} ctx = {};

int incall_announcement_is_running(void)
{
    return (ctx.chan != NULL);
}

static char *get_play_string(void)
{
    FILE *fp;
    char *buf;
    struct stat st = {};

    if (!(fp = fopen(PLAY_FILE, "r")))
        return NULL;

    stat(PLAY_FILE, &st);
    buf = malloc(st.st_size + 1);
    fgets(buf, st.st_size + 1, fp);
    fclose(fp);

    return buf;
}

static void end_cb(struct ast_channel *ast)
{
    if (ctx.modify_audio_proc)
    {
	ast_clear_flag(ctx.chan, AST_FLAG_DISALLOW_AUDIO_JUMPERS);
	ctx.modify_audio_proc(ctx.chan, ctx.modify_audio_param);
    }

    ctx.chan = NULL;
}

static int start_cb(void *data)
{
    char *play_str;
    int pvt_locked = 0;
    int chan_locked = 0;
    struct ast_channel *ast = ctx.chan;

    if (!(play_str = get_play_string()))
    {
	if (ctx.retries_left--)
	    return 1;
	goto Error;
    }

    ast_mutex_lock(ctx.pvt_lock);
    pvt_locked = 1;

    /* The channel has been destroyed. */
    if (ast != *ctx.pvt_owner)
	goto Error;

    ast_mutex_lock(&ast->lock);
    chan_locked = 1;

    if (ast_indicate(ast_bridged_channel(ast), AST_CONTROL_PLAY_DIAGNOSTICS))
	goto Error;

    if (ctx.modify_audio_proc)
    {
	ast_set_flag(ast, AST_FLAG_DISALLOW_AUDIO_JUMPERS);
	ctx.modify_audio_proc(ast, ctx.modify_audio_param);
    }

    jdsp_install_play_generator(ast, play_str, end_cb);
    goto Exit;

Error:
    ctx.chan = NULL;
Exit:
    if (play_str)
        free(play_str);
    if (chan_locked)
        ast_mutex_unlock(&ast->lock);
    if (pvt_locked)
	ast_mutex_unlock(ctx.pvt_lock);
    return 0;
}

int incall_announcement_start(struct ast_channel **pvt_owner,
    ast_mutex_t *pvt_lock,
    void (*modify_audio_proc)(struct ast_channel *, int),
    int modify_audio_param, struct sched_context *sched)
{
    struct ast_channel *ast = *pvt_owner;

    if (incall_announcement_is_running() || ast->generatordata ||
	ast_bridged_channel(ast)->generatordata)
    {
	return -1;
    }
 
    ctx = (struct ctx_t) { .chan = ast, .pvt_owner = pvt_owner,
	.pvt_lock = pvt_lock,
	.modify_audio_proc = modify_audio_proc,
	.modify_audio_param = modify_audio_param,
	.retries_left = MAX_PLAY_FILE_READ_RETRIES };

    unlink(PLAY_FILE);
    manager_event(EVENT_FLAG_CALL, "DiagIVRWrite", "unused");

    /* We wait for the play file to be ready. */
    ast_sched_add(sched, PLAY_FILE_READ_RETRY_GAP, start_cb, NULL);
    return 0;
}

int incall_announcement_start_indicated(struct ast_channel *ast)
{
    char *play_str;

    if (!(play_str = get_play_string()))
	return -1;

    jdsp_install_play_generator(ast, play_str, NULL);

    free(play_str);
    return 0;
}
