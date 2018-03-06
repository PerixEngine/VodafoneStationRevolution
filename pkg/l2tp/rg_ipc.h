/****************************************************************************
 *
 * rg/pkg/l2tp/rg_ipc.h
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

#ifndef _RG_IPC_H_
#define _RG_IPC_H_

#include <ipc.h>
#define OS_INCLUDE_NETIF
#include <os_includes.h>

#define L2TP_MAX_SECRET_LEN 96

typedef enum {
    /* Start connect to specified server.
     * Data type: l2tp_client_connect_t
     */
    L2TP_RG2D_CLIENT_CONNECT = 1,
    /* Close active connection.
     * Data type: l2tp_close_t
     */
    L2TP_RG2D_CLOSE = 2,
    /* Start negotiation with unaccepted requester. No need to send shared
     * secret, because it is one for all incoming L2TP connections and is set at
     * L2TP_RG2D_SERVER_START message.
     * Data type: l2tp_server_connect_t
     */
    L2TP_RG2D_SERVER_CONNECT = 3,
    /* Start L2TP server.
     * Data type: l2tp_server_start_t
     */
    L2TP_RG2D_SERVER_START = 4,
    /* Stop L2TP server.
     * No parameters.
     */
    L2TP_RG2D_SERVER_STOP = 5,
    /* New connection request arrives notification.
     * The command is sent to L2TP task.
     * Data type: l2tp_new_request_t
     */
    L2TP_D2RG_NEW_REQUEST = 6,
    /* Reject incoming request.
     * The command is sent by L2TP server task to the daemon.
     * Data type: l2tp_server_reject_request_t
     */
    L2TP_RG2D_REJECT_REQUEST = 7,
    /* Establish session (connection) notification.
     * The command is sent to L2TP plugin.
     * Data type: l2tp_connected_t
     */
    L2TP_D2RG_CONNECTED = 8,
    /* Update bounded IPs
     * Data type: l2tp_update_ips_t
     */
    L2TP_RG2D_UPDATE_IPS = 9,
    /* Closed connection notification.
     * The command is sent to L2TP plugin.
     * Data type: l2tp_connected_t
     */
    L2TP_D2RG_DISCONNECTED = 10,
    /* Add/delete tunnel */
    L2TP_RG2D_CREATE_TUNNEL = 11,
    L2TP_RG2D_DELETE_TUNNEL = 12,
    /* Connect/disconnect tunnel notification.
     * Data type: l2tp_tunnel_status_t
     */
    L2TP_D2RG_TUNNEL_STATUS = 13,
} l2tpd_cmd_t;

typedef struct {
    u32 cookie;
    struct in_addr server_ip;
    struct in_addr rg_ip;
    int start_rtx_delay;
    int start_max_retry;
    int hello_delay;
    int hello_rtx_delay;
    int hello_max_retry;
    int secret_len;
    char secret[L2TP_MAX_SECRET_LEN];
} l2tp_create_tunnel_t;

typedef struct {
    u32 cookie;
} l2tp_delete_tunnel_t;

typedef struct {
    u32 tunnel_id;
    u32 rg_session_id;
    u32 avp_size;
    char avp[0];
} l2tp_client_connect_t;

typedef struct {
    u32 rg_session_id;
    struct in_addr src;
    struct in_addr dst;
    u16 tun_out_id;
    u16 tun_in_id;
    u16 ses_out_id;
    u16 ses_in_id;
    u16 src_port;
    u16 dst_port;
} l2tp_connected_t;

typedef struct {
    u32 rg_session_id;
    int close_tunnel;
} l2tp_close_t;

typedef struct {
    u32 unaccept_id;
    struct in_addr remote_ip;
    struct in_addr local_ip;
} l2tp_new_request_t;

typedef struct {
    u32 unaccept_id;
    u32 rg_session_id;
} l2tp_server_connect_t;

typedef struct {
    int secret_len;
    char secret[L2TP_MAX_SECRET_LEN];
} l2tp_server_start_t;

typedef struct {
    u32 unaccept_id;
} l2tp_server_reject_request_t;

typedef struct {
    u32 num;
    struct in_addr ids[0];
} l2tp_update_ips_t;

typedef struct {
    u32 cookie;
    u32 tunnel_id;
    int connected;
} l2tp_tunnel_status_t;

#endif
