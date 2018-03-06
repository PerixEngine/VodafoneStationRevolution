/****************************************************************************
 *
 * rg/pkg/l2tp/rg.c
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

#include "l2tp.h"
#include "rg_ipc.h"
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>

#include <be_api_gpl.h>
#include <util/openrg_gpl.h>
#include <util/rg_openlog.h>
#include <kos_chardev_id.h>
#include <util/alloc.h>
#include <util/str.h>
#include <ppp/kernel/be_l2tp.h>

#define HANDLER_NAME "jungo"
#define MAX_IPC_SEND_RETRY 5

typedef struct rg_avp_t {
    struct rg_avp_t *next;
    uint32_t id;
    uint32_t size;
    void *value;
} rg_avp_t;

typedef struct {
    uint32_t id;
    rg_avp_t *avp;
    int closed_by_rg;
} rg_session_context_t;

typedef struct {
    uint16_t tun_id;
    uint32_t cookie;
    uint32_t id_count;
    uint32_t *id;		/* as received in the open message */
    enum {
	PEER_ACTIVE = 0,
	PEER_WAITING_CLOSE = 1,
    } state;
} rg_peer_context_t;

typedef struct rg_ipc_send_info_t {
    uint16_t port;
    l2tpd_cmd_t cmd;
    uint32_t retry;
    uint32_t buf_size;
    char buf[0];
} rg_ipc_send_info_t;

typedef struct l2tp_params_t {
    struct in_addr peer_addr;
    struct in_addr rg_addr;
    u8 secret[MAX_SECRET_LEN];
    u32 secret_len;
    int start_rtx_delay;
    int start_max_retry;
    int hello_delay;
    int hello_rtx_delay;
    int hello_max_retry;
    char *avp;
    int avp_size;
} l2tp_params_t;

static int rg_peer_set_tunnel_data(rg_peer_context_t **ctx, u32 cookie,
    uint16_t tun_id);
static int rg_peer_add_session_id(rg_peer_context_t **ctx, uint32_t session_id);
static int rg_session_close(EventSelector *es, uint32_t id, int close_tunnel);
static int rg_alloc_peer_context(rg_peer_context_t **ctx);
static int rg_create_tunnel(EventSelector *es, uint32_t cookie,
    l2tp_params_t *params);
static void rg_delete_tunnel(EventSelector *es, uint32_t cookie);
static int rg_session_open(EventSelector *es, uint32_t tun_id, uint32_t ses_id,
    int avp_size, char *avp);
static int establish_session(l2tp_session *ses);
static void close_session(l2tp_session *ses, char const *reason);
static void rg_tunnel_close_cb(l2tp_tunnel *tunnel);
static void handle_frame(l2tp_session *ses, unsigned char *buf, size_t len);
static void rg_session_add_dgram_avp(l2tp_dgram *dgram, l2tp_session *ses);
static int l2tp_peer_is_equal_session_id(l2tp_peer *peer, void *param);
static void rg_session_cont_free(rg_session_context_t *cont);
static int rg_send_ipc(l2tpd_cmd_t cmd, EventSelector *es, void *buf,
    uint32_t buf_size);
static int rg_new_request_notify(l2tp_tunnel *tunnel, uint32_t unaccept_id,
    uint32_t remote, uint32_t local);
static l2tp_peer *rg_get_lns_peer(l2tp_tunnel *tunnel, struct in_addr *peer_ip,
    uint16_t port);

static l2tp_call_ops my_ops = {
    establish_session,
    close_session,
    handle_frame,
    rg_session_add_dgram_avp,
};

static l2tp_lac_handler my_lac_handler = {
    NULL,
    HANDLER_NAME,
    &my_ops
};

static l2tp_lns_handler my_lns_handler = {
    NULL,
    HANDLER_NAME,
    &my_ops
};

static int ipc_sock = -1;
static EventHandler *ipc_event = NULL;
static char l2tp_server_secret[MAX_SECRET_LEN];
static uint32_t l2tp_server_secret_len;

static int add_avp(rg_session_context_t *ctx, int avp_id, void *avp_value)
{
    rg_avp_t *avp;

    if (avp_id < 0)
	return -1;

    if (!(avp = malloc_l(sizeof(*avp))))
	return -1;

    avp->size = strlen(avp_value);
    if (!(avp->value = malloc_l(avp->size)))
    {
	free(avp);
	return -1;
    }
    avp->id = avp_id;
    memcpy(avp->value, avp_value, avp->size);

    avp->next = ctx->avp;
    ctx->avp = avp;
    return 0;
}

static int avp_name_to_id(char *name)
{
    static struct {
	char *name;
	int code;
    } avp_codes[] = {
	{ "called_number", AVP_CALLED_NUMBER },
	{ "calling_number", AVP_CALLING_NUMBER },
	{ NULL, -1 }
    };
    int i;

    for (i = 0; avp_codes[i].name && strcmp(avp_codes[i].name, name); i++);

    return avp_codes[i].code;
}

static int rg_session_ctx_add_avp(rg_session_context_t *ctx, char *avp,
    int avp_size)
{
    char *avp_begin = avp;
    char *name = NULL, *value = NULL;

    if (!avp_size)
	return 0;

    while (avp < avp_begin + avp_size)
    {
	str_cpy(&name, avp);
	avp += strlen(name) + 1;

	str_cpy(&value, avp);
	avp += strlen(value) + 1;

	if (add_avp(ctx, avp_name_to_id(name), value))
	    break;
    }
    str_free(&name);
    str_free(&value);

    return avp == avp_begin + avp_size ? 0 : -1;
}

static void rg_session_add_dgram_avp(l2tp_dgram *dgram, l2tp_session *ses)
{
    rg_avp_t *avp;
    rg_session_context_t *ctx = ses->private;

    if (!ctx)
	return;

    for (avp = ctx->avp; avp; avp = avp->next)
    {
	l2tp_dgram_add_avp(dgram, ses->tunnel, MANDATORY, avp->size,
	    VENDOR_IETF, avp->id, avp->value);
    }
}

static l2tp_session *session_find_by_rg_id(l2tp_tunnel *tunnel, uint32_t id)
{
    void *dummy;
    l2tp_session *ses = l2tp_tunnel_first_session(tunnel, &dummy);

    while (ses && ses->private && 
	((rg_session_context_t *)ses->private)->id != id)
    {
	ses = l2tp_tunnel_next_session(tunnel, &dummy);
    }

    return ses;
}

static void rg_peer_remove_session_id(rg_peer_context_t *cont, uint32_t id)
{
    int i;

    if (!cont)
	return;

    for (i = 0; i < cont->id_count && cont->id[i] != id; ++i);
    if (i == cont->id_count)
    {
	l2tp_db(DBG_CONTROL, "\tSession %u not found in peer", id);
	return;
    }
    if (i < cont->id_count - 1)
    {
	memmove(cont->id + i, cont->id + i + 1, (cont->id_count - i - 1) *
	    sizeof(uint32_t));
    }
    --cont->id_count;
}

/* If any error return true to reset the tunnel. */
static uint32_t rg_is_all_sessions_idle(l2tp_tunnel *tunnel, uint32_t min_idle)
{
    int fd, ret;
    /* Argument: out - tunnel ID, in - tunnel idle time. */
    uint32_t arg = (uint32_t)tunnel->my_id;

    if ((fd = gpl_sys_rg_chrdev_open(KOS_CDT_L2TP_BACKEND, O_RDWR)) < 0)
    {
	rg_error(LERR, "Can't open ppp chardevice");
	return 1;
    }
    ret = ioctl(fd, L2TPGETIDLETIME, &arg);
    close(fd);

    return (ret < 0 || !arg) ? 1 : arg >= min_idle;
}

static int l2tp_wait_for_new_req_reply = 0;

/* called by l2tpd main event loop when there is something to read from ipc
 * socket */
static void rg_input(EventSelector *es, int sock, unsigned int flags,
    void *data)
{
    u32 size;
    void *buf = NULL;
    int fd, rc = -1;
    l2tpd_cmd_t cmd;

    /* read message */
    if ((fd = ipc_accept(sock)) < 0)
	return;
	
    if ((rc = ipc_u32_read(fd, &cmd)) || (rc = ipc_u32_read(fd, &size)))
	goto Error;

    if (size)
    {
	if (!(buf = zalloc_l(size)))
	    goto Error;
	if ((rc = ipc_read(fd, buf, size)))
	{
	    free(buf);
	    goto Error;
	}
    }
    ipc_server_close(fd, rc);

    /* handle message */
    switch (cmd)
    {
    case L2TP_RG2D_CREATE_TUNNEL:
	{
	    l2tp_create_tunnel_t *tun = buf;
	    l2tp_params_t params = {
		.peer_addr = tun->server_ip,
		.rg_addr = tun->rg_ip,
		.start_rtx_delay = tun->start_rtx_delay,
		.start_max_retry = tun->start_max_retry,
		.hello_delay = tun->hello_delay,
		.hello_rtx_delay = tun->hello_rtx_delay,
		.hello_max_retry = tun->hello_max_retry,
		.secret_len = tun->secret_len,
	    };

	    if (params.secret_len)
		memcpy(params.secret, tun->secret, params.secret_len);
	    rg_create_tunnel(es, tun->cookie, &params);
	    break;
	}
    case L2TP_RG2D_DELETE_TUNNEL:
	rg_delete_tunnel(es, (((l2tp_delete_tunnel_t *)buf)->cookie));
	break;
    case L2TP_RG2D_CLIENT_CONNECT:
	{
	    l2tp_client_connect_t *clnt_conn = buf;

	    /* open new session */
	    rg_session_open(es, clnt_conn->tunnel_id, clnt_conn->rg_session_id,
		clnt_conn->avp_size,
		clnt_conn->avp_size ? clnt_conn->avp : NULL);
	    break;
	}
    case L2TP_RG2D_CLOSE:
        {
	    l2tp_close_t *ses= buf;

	    /* close session */
	    rg_session_close(es, ses->rg_session_id, ses->close_tunnel);
	    break;
        }
    case L2TP_RG2D_SERVER_CONNECT:
	{
	    l2tp_server_connect_t *serv_conn = buf;
	    rg_peer_context_t *ctx = NULL;

	    l2tp_wait_for_new_req_reply = 0;

	    if (rg_peer_set_tunnel_data(&ctx, 0, serv_conn->unaccept_id) ||
		rg_peer_add_session_id(&ctx, serv_conn->rg_session_id))
	    {
		free(ctx);
		tunnel_reject_SCCRQ(serv_conn->unaccept_id);
		break;
	    }
	    tunnel_accept_SCCRQ(serv_conn->unaccept_id, ctx);
	    break;
	}
    case L2TP_RG2D_SERVER_START:
        {
	    l2tp_server_start_t *serv_start = buf;

	    if (!size)
	    {
		l2tp_server_secret_len = 0;
		memset(l2tp_server_secret, 0, sizeof(l2tp_server_secret));
	    }
	    else
	    {
		l2tp_server_secret_len = serv_start->secret_len;
		memset(l2tp_server_secret, 0, sizeof(l2tp_server_secret));
		memcpy(l2tp_server_secret, serv_start->secret,
		    serv_start->secret_len);
	    }
	}

	/* Set L2TP server callbacks. */
	tunnel_set_new_request_cb(rg_new_request_notify);
	tunnel_set_get_lns_peer(rg_get_lns_peer);

	break;
    case L2TP_RG2D_SERVER_STOP:
	/* Stop accept new requests. */
	tunnel_set_new_request_cb(NULL);
	tunnel_set_get_lns_peer(NULL);
	/* No stop active incoming connection at this point - main task downs
	 * all appropriated devices.
	 */
	break;
    case L2TP_RG2D_REJECT_REQUEST:
	{
	    l2tp_server_reject_request_t *rej_req = buf;

	    l2tp_wait_for_new_req_reply = 0;

	    tunnel_reject_SCCRQ(rej_req->unaccept_id);
	}
	break;
    case L2TP_RG2D_UPDATE_IPS:
	{
	    l2tp_update_ips_t *upd_ips = buf;
	    uint32_t num = 0;
	    struct in_addr *srcs = NULL;

	    if (size)
	    {
		srcs = upd_ips->ids;
		num = upd_ips->num;
	    }
	    l2tp_socket_init(es, srcs, num);
	    break;
	}
    default:
	break;
    }
    free(buf);
    return;

Error:
    ipc_server_close(fd, rc);
}

static int l2tp_ipc_open(EventSelector *es)
{
    ipc_sock = ipc_bind_listen_port(htons(RG_IPC_PORT_MT_2_L2TPD));
    if (ipc_sock < 0)
    {
	l2tp_set_errmsg("could not open ipc socket");
	return -1;
    }

    ipc_event = Event_AddHandler(es, ipc_sock, EVENT_FLAG_READABLE, rg_input,
	NULL);
    
    return 0;
}

static void l2tp_ipc_close(EventSelector *es)
{
    if (ipc_event)
    {
	Event_DelHandler(es, ipc_event);
	ipc_event = NULL;
    }

    if (ipc_sock < 0)
	return;
    
    close(ipc_sock);
    ipc_sock = -1;
}

static void rg_send_tunnel_status_notify(l2tp_tunnel *t, u32 cookie,
    int establish)
{
    l2tp_tunnel_status_t tun_status = {
	.cookie = cookie,
	.tunnel_id = t->my_id,
	.connected = establish
    };

    rg_send_ipc(L2TP_D2RG_TUNNEL_STATUS, t->es, &tun_status,
	sizeof(tun_status));
}

static void rg_tunnel_state_cb(l2tp_tunnel *t, int establish)
{
    rg_send_tunnel_status_notify(t,
	((rg_peer_context_t *)t->peer->private)->cookie, establish);
}

int rg_init(EventSelector *es)
{
    rg_openlog("", LOG_PID | LOG_NDELAY | LOG_CONS, LOG_DAEMON);
    l2tp_session_register_lac_handler(&my_lac_handler);
    l2tp_session_register_lns_handler(&my_lns_handler);
    tunnel_set_tunnel_state_cb(rg_tunnel_state_cb);
    tunnel_set_sessions_idle_cb(rg_is_all_sessions_idle);

    return l2tp_ipc_open(es);
}

static int l2tp_peer_is_equal_session_id(l2tp_peer *peer, void *param)
{
    rg_peer_context_t *cont = peer->private;
    int i;

    if (!cont)
	return 0;

    for (i = 0; i < cont->id_count && cont->id[i] != *(uint32_t *)param; ++i);
    return i < cont->id_count;
}

static int is_last_session(void *ctx)
{
    return !ctx || !((rg_peer_context_t *)ctx)->id_count;
}

/* Parameter close_tunnel: L2TP server tunnel is not managed by separate L2TP
 * tunnel device, so when L2TP server session is closing, its tunnel should be
 * closed too.
 */
static int rg_session_close(EventSelector *es, uint32_t id, int close_tunnel)
{
    l2tp_tunnel *tunnel;
    l2tp_peer *peer;
    rg_peer_context_t *cont;
    l2tp_session *ses;
    int free_tunnel_now;

    /* search matching session */
    peer = l2tp_peer_find_by_func(l2tp_peer_is_equal_session_id, &id);
    if (!peer || !peer->private)
    {
	l2tp_set_errmsg("session %u not found", id);
	return -1;
    }
    cont = peer->private;
    if (!cont)
	return -1;
    switch (cont->state)
    {
    case PEER_ACTIVE:
	tunnel = tunnel_find_bypeer(peer);
	if (!tunnel)
	{
	    l2tp_set_errmsg("no tunnel found for session %u to %s", id,
	       	inet_ntoa(peer->addr.sin_addr));
	    peer_release(peer);
	    peer_free(peer);
	    return -1;
	}
	ses = session_find_by_rg_id(tunnel, id);
	if (ses)
	{
	    ((rg_session_context_t *)ses->private)->closed_by_rg = 1;
	    /* Session is removed right after sending CDN message */
	    l2tp_session_send_CDN(ses, RESULT_GENERAL_REQUEST, ERROR_OK,
		"Closed by OpenRG");
	    ses = NULL;
	}

	rg_peer_remove_session_id(peer->private, id);

	if (!close_tunnel || !is_last_session(peer->private))
	    break;

	peer_release(peer);
	cont->state = PEER_WAITING_CLOSE;

	free_tunnel_now = (tunnel->state == TUNNEL_WAIT_CTL_REPLY);
	if (free_tunnel_now)
	    l2tp_tunnel_free_tunnel(tunnel);
	else
	    l2tp_tunnel_stop_tunnel(tunnel, "Closed by OpenRG");

	DBG(l2tp_db(DBG_FLOW, "tunnel %s for session %u to %s",
	    free_tunnel_now ? "freed" : "stopped", id,
	    inet_ntoa(peer->addr.sin_addr)));
	break;
    case PEER_WAITING_CLOSE:
	/* should never be reached */
	l2tp_set_errmsg("session %u to %s already closing", id, 
	    inet_ntoa(peer->addr.sin_addr));
	return -1;
    }

    return 0;
}

static void rg_peer_close(l2tp_peer *p)
{
    if (!p->private)
	return;

    nfree(((rg_peer_context_t *)p->private)->id);
    free(p->private);
    p->private = NULL;
}

static l2tp_peer *rg_get_lns_peer(l2tp_tunnel *tunnel, struct in_addr *peer_ip,
    uint16_t port)
{
    /* Peer's context is unknown at this time and will be updated later. */
    l2tp_peer *p = l2tp_peer_insert_and_fill(0, HANDLER_NAME, peer_ip,
	port, &tunnel->local_addr, l2tp_server_secret, l2tp_server_secret_len,
	NULL, rg_peer_close);
    if (!p)
	return NULL;
    tunnel->close = rg_tunnel_close_cb;
    return p;
}

static int rg_peer_add_session_id(rg_peer_context_t **ctx, uint32_t session_id)
{
    uint32_t *tmp;

    if (!*ctx && rg_alloc_peer_context(ctx))
	return -1;

    tmp = malloc(((*ctx)->id_count + 1) * sizeof(uint32_t));
    if (!tmp)
    {
	l2tp_set_errmsg("\tcould not allocate for id %u", session_id);
	return -1;
    }
    if ((*ctx)->id)
	memcpy(tmp + 1, (*ctx)->id, (*ctx)->id_count * sizeof(uint32_t));
    tmp[0] = session_id;
    free((*ctx)->id);
    (*ctx)->id = tmp;
    ++(*ctx)->id_count;

    return 0;
}

static int rg_peer_set_tunnel_data(rg_peer_context_t **ctx, u32 cookie,
    uint16_t tun_id)
{
    if (!*ctx && rg_alloc_peer_context(ctx))
	return -1;
    (*ctx)->cookie = cookie;
    (*ctx)->tun_id = tun_id;
    return 0;
}

static int rg_alloc_peer_context(rg_peer_context_t **ctx)
{
    *ctx = malloc(sizeof(**ctx));

    if (!*ctx)
	return -1;
    memset(*ctx, 0, sizeof(**ctx));
    (*ctx)->state = PEER_ACTIVE;

    return 0;
}

static int rg_find_peer_by_cookie_cb(l2tp_peer *peer, void *param)
{
    return peer->private && ((rg_peer_context_t *)peer->private)->cookie ==
	(uint32_t)param;
}

static void rg_delete_tunnel(EventSelector *es, uint32_t cookie)
{
    int free_tunnel_now;
    l2tp_tunnel *tunnel;
    l2tp_peer *p = l2tp_peer_find_by_func(rg_find_peer_by_cookie_cb,
	(void *)cookie);

    if (!p)
	return;

    peer_release(p);
    ((rg_peer_context_t *)p->private)->state = PEER_WAITING_CLOSE;

    if (!(tunnel = l2tp_tunnel_find_for_peer(p, es)))
	return;
    
    free_tunnel_now = (tunnel->state == TUNNEL_WAIT_CTL_REPLY);
    if (free_tunnel_now)
	l2tp_tunnel_free_tunnel(tunnel);
    else
	l2tp_tunnel_stop_tunnel(tunnel, "Closed by OpenRG");

    DBG(l2tp_db(DBG_FLOW, "tunnel to %s %s",
	inet_ntoa(p->addr.sin_addr), free_tunnel_now ? "freed" : "stopped"));
}

static int rg_create_tunnel(EventSelector *es, uint32_t cookie,
    l2tp_params_t *params)
{
    l2tp_peer *p;
    l2tp_tunnel *tun;
    int ret = -1;


    p = l2tp_peer_insert_and_fill(1, HANDLER_NAME, &params->peer_addr,
	L2TP_PORT, &params->rg_addr, params->secret, params->secret_len, NULL,
	rg_peer_close);
    if (!p)
    {
	l2tp_set_errmsg("could not create peer for tunnel %u to %s",
	    cookie, inet_ntoa(params->peer_addr));
	goto Exit;
    }

    p->start_rtx_delay = params->start_rtx_delay;
    p->start_max_retry = params->start_max_retry;
    p->hello_delay = params->hello_delay;
    p->hello_rtx_delay = params->hello_rtx_delay;
    p->hello_max_retry = params->hello_max_retry;

    p->retain_tunnel = 1;
    
    if (!(tun = l2tp_tunnel_find_for_peer(p, es)))
    {
	l2tp_set_errmsg("could not call peer of tunnel to %s",
	    inet_ntoa(p->addr.sin_addr));
	goto Exit;
    }

    if (rg_peer_set_tunnel_data((rg_peer_context_t **)&p->private, cookie,
	tun->my_id))
    {
	goto Exit;
    }

    tun->close = rg_tunnel_close_cb;

    ret = 0;

Exit:
    if (ret)
    {
	free(p->private);
	p->private = NULL;
	peer_release(p);
	peer_free(p);
    }
    return ret;
}

static int rg_find_peer_by_tun_id_cb(l2tp_peer *peer, void *param)
{
    return peer->private && ((rg_peer_context_t *)peer->private)->tun_id ==
	(uint16_t)(uint32_t)param;
}

static int rg_session_open(EventSelector *es, uint32_t tun_id, uint32_t ses_id,
    int avp_size, char *avp)
{
    l2tp_peer *p;
    l2tp_session *ses;
    rg_session_context_t *ses_cont = NULL;

    l2tp_db(DBG_CONTROL, "opening session %u for tunnel %u", ses_id, tun_id);

    p = l2tp_peer_find_by_func(rg_find_peer_by_tun_id_cb, (void *)tun_id);
    if (!p)
    {
	l2tp_set_errmsg("could not create session %u for tunnel %u: "
	    "peer not found", ses_id, tun_id);
	return  -1;
    }

    if (rg_peer_add_session_id((rg_peer_context_t **)&p->private, ses_id))
	return -1;
    
    ses_cont = zalloc_l(sizeof(rg_session_context_t));
    if (!ses_cont)
    {
	l2tp_set_errmsg("unable to allocate session private memory");
	goto Error;
    }

    ses_cont->id = ses_id;

    if (rg_session_ctx_add_avp(ses_cont, avp, avp_size))
    {
	l2tp_set_errmsg("invalid session AVP");
	goto Error;
    }

    ses = l2tp_session_call_lns(p, "foobar", es, ses_cont);
    if (!ses)
    {
	l2tp_set_errmsg("could not call peer of session %u to %s", ses_id,
	    inet_ntoa(p->addr.sin_addr));
	goto Error;
    }
    ses->tunnel->close = rg_tunnel_close_cb;

    return 0;

Error:
    rg_session_cont_free(ses_cont);
    rg_peer_remove_session_id(p->private, ses_id);
    return -1;
}

static void rg_ipc_send_msg_free(EventSelector *es,
    rg_ipc_send_info_t *msg_info)
{
    free(msg_info);
}

static void rg_send_ipc_try(EventSelector *es, int fd_dummy,
    unsigned int flags_dummy, void *data)
{
    rg_ipc_send_info_t *msg_info = data;
    int rc = -1, fd = -1;

    l2tp_ipc_close(es);

    if (msg_info->retry < MAX_IPC_SEND_RETRY &&
	((fd = ipc_connect(htons(msg_info->port))) < 0 ||
	(rc = ipc_u32_write(fd, msg_info->cmd)) ||
	(rc = ipc_u32_write(fd, msg_info->buf_size)) ||
	(msg_info->buf_size && (rc = ipc_write(fd, msg_info->buf,
	msg_info->buf_size)))))
    {
	struct timeval t;

	/* schedule retry 0.1 seconds from now */
	t.tv_sec = 0;
	t.tv_usec = 100000;
	msg_info->retry++;
	Event_AddTimerHandler(es, t, rg_send_ipc_try, msg_info);
	goto Exit;
    }
    rg_ipc_send_msg_free(es, msg_info);

Exit:
    if (fd >= 0)
	ipc_client_close(fd, rc);
    l2tp_ipc_open(es);
}

static uint16_t get_ipc_port(l2tpd_cmd_t cmd)
{
    static struct {
	uint16_t port;
	l2tpd_cmd_t cmd;
    } cmd2port[] = {
	{ RG_IPC_PORT_L2TPD_2_TUN, L2TP_D2RG_TUNNEL_STATUS },
	{ RG_IPC_PORT_L2TPD_2_TASK, L2TP_D2RG_NEW_REQUEST },
	{ RG_IPC_PORT_L2TPD_2_MT, L2TP_D2RG_CONNECTED },
	{ RG_IPC_PORT_L2TPD_2_MT, L2TP_D2RG_DISCONNECTED },
	{ 0, 0 }
    };
    int i;

    for (i = 0; cmd2port[i].port && cmd2port[i].cmd != cmd; i++);

    return cmd2port[i].port;
}

static int rg_send_ipc(l2tpd_cmd_t cmd, EventSelector *es, void *buf,
    uint32_t buf_size)
{
    rg_ipc_send_info_t *msg_info;
    uint16_t port = get_ipc_port(cmd);

    if (!port)
    {
	l2tp_set_errmsg("Invalid IPC command %u", cmd);
	return -1;
    }

    msg_info = zalloc_l(sizeof(*msg_info) + buf_size);
    if (!msg_info)
	return -1;

    msg_info->cmd = cmd;
    msg_info->port = port;
    msg_info->buf_size = buf_size;
    if (buf_size)
	memcpy(msg_info->buf, buf, buf_size);

    rg_send_ipc_try(es, 0, 0, msg_info);

    return 0;
}

static void rg_session_cont_free(rg_session_context_t *cont)
{
    if (!cont)
	return;

    while (cont->avp)
    {
	rg_avp_t *avp = cont->avp;

	cont->avp = avp->next;
	free(avp->value);
	free(avp);
    }
    free(cont);
}

static int rg_new_request_notify(l2tp_tunnel *tunnel, uint32_t unaccept_id,
    uint32_t remote, uint32_t local)
{
    l2tp_new_request_t req = {
	.unaccept_id = unaccept_id,
	.remote_ip = (struct in_addr){ .s_addr = remote },
	.local_ip = (struct in_addr){ .s_addr = local }
    };

    /* Temporary hack until B37933 is fixed.
     * Do not accept any requests from client until main_task send answer about
     * l2tp process' previous new request notification. Without this fix both
     * main_task and l2tp process may send ipc message at same time that can
     * stuck OpenRG.
     */
    if (l2tp_wait_for_new_req_reply)
	return -1;
    l2tp_wait_for_new_req_reply = 1;

    /* update caller that session is open */
    return rg_send_ipc(L2TP_D2RG_NEW_REQUEST, tunnel->es, &req, sizeof(req));
}

static int establish_session(l2tp_session *ses)
{
    l2tp_connected_t ses_conn = {
	.src.s_addr = ses->tunnel->local_addr.s_addr,
	.dst.s_addr = ses->tunnel->peer_addr.sin_addr.s_addr,
	.tun_out_id = ses->tunnel->assigned_id,
	.tun_in_id = ses->tunnel->my_id,
	.ses_out_id = ses->assigned_id,
	.ses_in_id = ses->my_id,
	.dst_port = ses->tunnel->peer_addr.sin_port,
	.src_port = htons(L2TP_PORT)
    };

    /* If private is not exist then this is L2TP server session. Session was
     * created during negotiation and this callback is the entry point of
     * session to RG context.
     */
    if (!ses->private)
    {
	if (!(ses->private = zalloc_l(sizeof(rg_session_context_t))))
	    return -1;
	((rg_session_context_t *)ses->private)->id =
	    ((rg_peer_context_t *)ses->tunnel->peer->private)->id[0];
    }

    ses_conn.rg_session_id = ((rg_session_context_t *)ses->private)->id;

    /* update caller that session is open */
    return rg_send_ipc(L2TP_D2RG_CONNECTED, ses->tunnel->es, &ses_conn,
	sizeof(ses_conn));
}

/* close_session() is called just before ses is freed */
static void close_session(l2tp_session *ses, char const *reason)
{
    if (!ses || !ses->private)
	return;
    l2tp_db(DBG_CONTROL, "session %u to %s closed. reason %s",
	((rg_session_context_t *)ses->private)->id,
       	inet_ntoa(ses->tunnel->peer->addr.sin_addr), reason ? reason : "-");

    if (!((rg_session_context_t *)ses->private)->closed_by_rg)
    {
	l2tp_connected_t disconn = {
	    .rg_session_id = ((rg_session_context_t *)ses->private)->id
	};

	rg_send_ipc(L2TP_D2RG_DISCONNECTED, ses->tunnel->es, &disconn,
	    sizeof(disconn));
    }

    rg_session_cont_free(ses->private);
    ses->private = NULL;
}

/* rg_tunnel_close_cb() is called just before tunnel is freed */
static void rg_tunnel_close_cb(l2tp_tunnel *tunnel)
{
    rg_peer_context_t *cont = tunnel->peer->private;

    if (!cont)
	return;

    DBG(l2tp_db(DBG_TUNNEL, "tunnel to %s closed",
	inet_ntoa(tunnel->peer->addr.sin_addr)));

    switch (cont->state)
    {
    case PEER_ACTIVE:
	peer_release(tunnel->peer);
	/* If peer active then the tunnel is closed by remote host - notify
	 * OpenRG about tunnel closing.
	 */
	rg_send_tunnel_status_notify(tunnel, cont->cookie, 0);
	/* fall through */
    case PEER_WAITING_CLOSE:
	/* peer already released, either here or in rg_session_close() */
	peer_free(tunnel->peer);
	break;
    }
}

static void handle_frame(l2tp_session *ses, unsigned char *buf, size_t len)
{
#if 0
    int n;
    rg_session_context_t *cont = ses->private;

    /* chardev was not attached yet */
    if (!cont)
	return;
    
    /* TODO: Add error checking */
    n = write(cont->fd, buf, len);

    DBG(l2tp_db(DBG_XMIT_RCV,
	"handle_frame() sent to pppd %d octets of %d (%d-%s):\n",
	n, len, errno, strerror(errno)));
    DBG(l2tp_db(DBG_XMIT_RCV_DUMP, "orig: %s\n", dump(buf, len)));
#endif
}


