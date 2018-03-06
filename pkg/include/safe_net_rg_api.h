/****************************************************************************
 *
 * rg/pkg/include/safe_net_rg_api.h
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

#ifndef _SAFE_NET_RG_API_H_
#define _SAFE_NET_RG_API_H_

struct net_device;
struct sk_buff;

typedef enum {
    SAFE_NET_ROUTE_IGNORE = 0,
    SAFE_NET_ROUTE_INCOMING = 1,
    SAFE_NET_ROUTE_OUTGOING = 2,
} safe_net_route_type_t; 

safe_net_route_type_t is_safe_net_route(struct net_device *in_dev,
    struct net_device *out_dev);

/* Clear fastpath info to prevent creation of fastpath entry for connection */
void skb_no_fastpath(struct sk_buff *skb);

#endif
