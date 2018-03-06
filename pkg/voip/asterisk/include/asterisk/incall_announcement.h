/****************************************************************************
 *
 * rg/pkg/voip/asterisk/include/asterisk/incall_announcement.h
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

#ifndef INCALL_ANNOUNCEMENT_H_
#define INCALL_ANNOUNCEMENT_H_

#include <asterisk/channel.h>

int incall_announcement_start(struct ast_channel **pvt_owner,
    ast_mutex_t *pvt_lock, void (*modify_audio_proc)(struct ast_channel *, int),
    int modify_audio_param, struct sched_context *sched);

int incall_announcement_start_indicated(struct ast_channel *ast);

int incall_announcement_is_running(void);

#endif
