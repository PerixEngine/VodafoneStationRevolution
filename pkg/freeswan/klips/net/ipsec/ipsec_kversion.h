#ifndef _FREESWAN_KVERSIONS_H
/*
 * header file for FreeS/WAN library functions
 * Copyright (C) 1998, 1999, 2000  Henry Spencer.
 * Copyright (C) 1999, 2000, 2001  Richard Guy Briggs
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Library General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/lgpl.txt>.
 * 
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
 * License for more details.
 *
 */
#define	_FREESWAN_KVERSIONS_H	/* seen it, no need to see it again */

/*
 * this file contains a series of atomic defines that depend upon
 * kernel version numbers. The kernel versions are arranged
 * in version-order number (which is often not chronological)
 * and each clause enables or disables a feature.
 */

/*
 * First, assorted kernel-version-dependent trickery.
 */
#include <linux/version.h>
#ifndef KERNEL_VERSION
#define KERNEL_VERSION(x,y,z) (((x)<<16)+((y)<<8)+(z))
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,1,0)
#define HEADER_CACHE_BIND_21
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,0)
#define SPINLOCK
#define PROC_FS_21
#define NETLINK_SOCK
#define NET_21
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,1,19)
#define net_device_stats enet_statistics
#endif                                                                         

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,0)
#define SPINLOCK_23
#define NETDEV_23
#  ifndef CONFIG_IP_ALIAS
#  define CONFIG_IP_ALIAS
#  endif
#include <linux/socket.h>
#include <linux/skbuff.h>
#include <linux/netlink.h>
#  ifdef NETLINK_XFRM
#  define NETDEV_25
#  endif
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,25)
#define PROC_FS_2325
#undef  PROC_FS_21
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
#define ipsec_proc_net_remove(a) proc_net_remove(&init_net, a)
#define PROC_NET init_net.proc_net
#else
#define ipsec_proc_net_remove(a) proc_net_remove(a)
#define PROC_NET proc_net
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,30)
#define PROC_NO_DUMMY
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,35)
#define SKB_COPY_EXPAND
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,37)
#define IP_SELECT_IDENT
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
#if defined(CONFIG_NF_CONNTRACK) || defined(CONFIG_NF_CONNTRACK_MODULE)
#define SKB_RESET_NFCT
#endif
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,50)) && defined(CONFIG_NETFILTER)
#define SKB_RESET_NFCT
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,2)
#define IP_SELECT_IDENT_NEW
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,4)
#define IPH_is_SKB_PULLED
#define SKB_COW_NEW
#define PROTO_HANDLER_SINGLE_PARM
#define IP_FRAGMENT_LINEARIZE 1
#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,4) */
#  ifdef REDHAT_BOGOSITY
#  define IP_SELECT_IDENT_NEW
#  define IPH_is_SKB_PULLED
#  define SKB_COW_NEW
#  define PROTO_HANDLER_SINGLE_PARM
#  endif /* REDHAT_BOGOSITY */
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,4) */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,9)
#define MALLOC_SLAB
#define LINUX_KERNEL_HAS_SNPRINTF
#endif                                                                         

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#define HAVE_NETDEV_PRINTK 1
#define NET_26
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,8)
#define NEED_INET_PROTOCOL
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,12)
#define HAVE_SOCK_ZAPPED
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
#define NET_26_24_SKALLOC
#else
#define NET_26_12_SKALLOC
#endif
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,13)
#define HAVE_SOCK_SECURITY

/* skb->nf_debug disappared completely in 2.6.13 */
#define HAVE_SKB_NF_DEBUG
#endif

#define SYSCTL_IPSEC_DEFAULT_TTL sysctl_ip_default_ttl                      
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,14)
/* skb->stamp changed to skb->tstamp in 2.6.14 */
#define HAVE_TSTAMP
#define HAVE_INET_SK_SPORT
#undef  SYSCTL_IPSEC_DEFAULT_TTL
#define SYSCTL_IPSEC_DEFAULT_TTL IPSEC_DEFAULT_TTL
#else
#define HAVE_SKB_LIST
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22)
#define HAVE_KERNEL_TSTAMP
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35)
#define HAVE_SOCKET_WQ
#endif

#ifdef HAVE_SOCKET_WQ
#define ipsec_sk_fasync_list(sock) ((sock)->wq->fasync_list)
#else
#define ipsec_sk_fasync_list(sock) ((sock)->fasync_list)
#define sk_sleep(sock) ((sock)->sk_sleep)
#endif

#ifdef NET_21
#  include <linux/in6.h>
#else
     /* old kernel in.h has some IPv6 stuff, but not quite enough */
#  define	s6_addr16	s6_addr
#  define	AF_INET6	10
#  define uint8_t __u8
#  define uint16_t __u16 
#  define uint32_t __u32 
#  define uint64_t __u64 
#endif

#ifdef NET_21
# define ipsec_kfree_skb(a) kfree_skb(a)
#else /* NET_21 */
# define ipsec_kfree_skb(a) kfree_skb(a, FREE_WRITE)
#endif /* NET_21 */

#ifndef NETDEV_23
#define net_device device
#endif

#ifdef NET_26
#define KLIPS_INC_USE /* nothing */
#define KLIPS_DEC_USE /* nothing */
#define KLIPS_MOD_INC_USE /* nothing */
#define KLIPS_MOD_DEC_USE /* nothing */
#endif

#ifndef NET_26
#define sk_receive_queue 	receive_queue
#define sk_destruct		destruct
#define sk_reuse		reuse
#define sk_zapped		zapped
#define sk_family		family
#define sk_protocol		protocol
#define sk_protinfo		protinfo
#define sk_sleep		sleep
#define sk_state_change		state_change
#define sk_shutdown		shutdown
#define sk_err			err
#define sk_stamp		stamp
#define sk_socket		socket
#define sk_sndbuf		sndbuf
#define sock_flag(sk, flag)		sk->dead
#define sk_for_each(sk, node, plist)	for(sk=*plist; sk!=NULL; sk = sk->next)

#define KLIPS_INC_USE MOD_INC_USE_COUNT
#define KLIPS_DEC_USE MOD_DEC_USE_COUNT
#define KLIPS_MOD_INC_USE __MOD_INC_USE_COUNT
#define KLIPS_MOD_DEC_USE __MOD_DEC_USE_COUNT
#endif

#ifdef NEED_INET_PROTOCOL
#define inet_protocol net_protocol
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 23)
/* Try using the new klips encaps hook for nat-t, instead of udp.c */
#define HAVE_UDP_ENCAP_CONVERT 1
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
#define HAVE_NETDEV_HEADER_OPS 1
#define dev_header_ops_create(dev) ((dev)->header_ops ? \
    (dev)->header_ops->create : NULL)
#define dev_header_ops_rebuild(dev) ((dev)->header_ops ? \
    (dev)->header_ops->rebuild : NULL)
#define dev_header_ops_cache_update(dev) ((dev)->header_ops ? \
    (dev)->header_ops->cache_update : NULL)
#else
#define dev_header_ops_create(dev) ((dev)->hard_header)
#define dev_header_ops_rebuild(dev) ((dev)->rebuild_header)
#define dev_header_ops_cache_update(dev) ((dev)->header_cache_update)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 25)
# define ip_chk_addr(a) inet_addr_type(&init_net, a)
#else
# define ip_chk_addr inet_addr_type
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27)
#define HAVE_CURRENT_UID
#endif

#if !defined(HAVE_CURRENT_UID)
#define current_uid() (current->uid)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)
#define USE_NETDEV_OPS 1
#define dev_ops_set_mac_address(dev) ((dev)->netdev_ops->ndo_set_mac_address)
#define dev_ops_start_xmit(dev) ((dev)->netdev_ops->ndo_start_xmit)
#define dev_ops_get_stats(dev) ((dev)->netdev_ops->ndo_get_stats)
#else
#define dev_ops_set_mac_address(dev) ((dev)->set_mac_address)
#define dev_ops_start_xmit(dev) ((dev)->hard_start_xmit)
#define dev_ops_get_stats(dev) ((dev)->get_stats)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 33)
#define CTL_NAME(n) .ctl_name = n,
#else
#define CTL_NAME(n)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22)
#include <linux/ip.h>
#else
#define skb_tail_pointer(skb)  ((skb)->tail)
#define skb_end_pointer(skb)  ((skb)->end)
#define skb_network_header(skb)  ((skb)->nh.raw)
#define skb_set_network_header(skb, off)  ((skb)->nh.raw = (skb)->data + \
      						     (off))
#define tcp_hdr(skb)  ((skb)->h.th)
#define udp_hdr(skb)  ((skb)->h.uh)
#define ip_hdr(skb)   ((skb)->nh.iph)
#define skb_transport_header(skb)  ((skb)->h.raw)
#define skb_network_offset(skb)  ((skb)->nh.raw - (skb)->data)
#define skb_set_transport_header(skb, off)  ((skb)->h.raw = (skb)->data + \
							      (off))
#ifdef NET_SKBUFF_DATA_USES_OFFSET
#define skb_reset_transport_header(skb) ((skb)->h.raw = (skb)->data - \
      						  (skb)->head)
#define skb_reset_network_header(skb) ((skb)->nh.raw = (skb)->data - \
      						  (skb)->head)
#else
#define skb_reset_transport_header(skb) ((skb)->h.raw = (skb)->data)
#define skb_reset_network_header(skb) ((skb)->nh.raw = (skb)->data)
#endif
#define skb_mac_header(skb)  ((skb)->mac.raw)
#define skb_set_mac_header(skb, off)  ((skb)->mac.raw = (skb)->data + (off))
#endif
/* turn a pointer into an offset for above macros */
#define ipsec_skb_offset(skb, ptr) (((unsigned char *)(ptr)) - (skb)->data)

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 31)
#define skb_dst_drop(s)        ({ \
					 if ((s)->dst) \
						 dst_release((s)->dst); \
					 (s)->dst = NULL; \
			       })
#define skb_dst_set(s, p)      ((s)->dst = (p))
#define skb_dst(s)             ((s)->dst)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 33)
#define        inet_sport      sport
#define        inet_dport      dport
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
#define        ipsec_route_dst(x)      (x)->dst
#else
#define        ipsec_route_dst(x)      (x)->u.dst
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
#define NIPQUAD_FMT "%pI4"
#define NIPQUAD(ip) (&(ip))
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37)
#define PRIVATE_ARP_BROKEN_OPS
#endif

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 38)
#define flowi_tos nl_u.ip4_u.tos
#define flowi_proto proto
#define flowi_mark mark
#define flowi_oif oif
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39)
#define nl_u u
#define ip4_u ip4
#define ip6_u ip6
#endif

#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_ipv6.h>
#ifdef NF_IP_LOCAL_OUT
#define LSW_NF_INET_LOCAL_OUT  NF_IP_LOCAL_OUT
#endif
#ifndef LSW_NF_INET_LOCAL_OUT
#define LSW_NF_INET_LOCAL_OUT NF_INET_LOCAL_OUT
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 1, 0)
#define HAVE_NETIF_QUEUE
#endif

#ifndef SPINLOCK
#  include <linux/bios32.h>
     /* simulate spin locks and read/write locks */
     typedef struct {
       volatile char lock;
     } spinlock_t;

     typedef struct {
       volatile unsigned int lock;
     } rwlock_t;                                                                     

#  define spin_lock_init(x) { (x)->lock = 0;}
#  define rw_lock_init(x) { (x)->lock = 0; }

#  define spin_lock(x) { while ((x)->lock) barrier(); (x)->lock=1;}
#  define spin_lock_irq(x) { cli(); spin_lock(x);}
#  define spin_lock_irqsave(x,flags) { save_flags(flags); spin_lock_irq(x);}

#  define spin_unlock(x) { (x)->lock=0;}
#  define spin_unlock_irq(x) { spin_unlock(x); sti();}
#  define spin_unlock_irqrestore(x,flags) { spin_unlock(x); restore_flags(flags);}

#  define read_lock(x) spin_lock(x)
#  define read_lock_irq(x) spin_lock_irq(x)
#  define read_lock_irqsave(x,flags) spin_lock_irqsave(x,flags)

#  define read_unlock(x) spin_unlock(x)
#  define read_unlock_irq(x) spin_unlock_irq(x)
#  define read_unlock_irqrestore(x,flags) spin_unlock_irqrestore(x,flags)

#  define write_lock(x) spin_lock(x)
#  define write_lock_irq(x) spin_lock_irq(x)
#  define write_lock_irqsave(x,flags) spin_lock_irqsave(x,flags)

#  define write_unlock(x) spin_unlock(x)
#  define write_unlock_irq(x) spin_unlock_irq(x)
#  define write_unlock_irqrestore(x,flags) spin_unlock_irqrestore(x,flags)
#endif /* !SPINLOCK */

#ifndef SPINLOCK_23
#  define spin_lock_bh(x)  spin_lock_irq(x)
#  define spin_unlock_bh(x)  spin_unlock_irq(x)

#  define read_lock_bh(x)  read_lock_irq(x)
#  define read_unlock_bh(x)  read_unlock_irq(x)

#  define write_lock_bh(x)  write_lock_irq(x)
#  define write_unlock_bh(x)  write_unlock_irq(x)
#endif /* !SPINLOCK_23 */

#endif /* _FREESWAN_KVERSIONS_H */

/*
 * $Log: ipsec_kversion.h,v $
 * Revision 1.6.4.18  2015/07/20 14:54:40  itamarh
 * B191157 Don't use freeswan NAT-T kernel changes in linux-3.x
 *
 * Revision 1.6.2.18  2015/07/20 14:51:10  itamarh
 * B191157 Don't use freeswan NAT-T kernel changes in linux-3.x
 *
 * Revision 1.6.4.17  2015/07/15 13:28:42  itamarh
 * B191157 Define HAVE_NETIF_QUEUE in freeswan for linux-3.x
 *
 * Revision 1.6.2.17  2015/07/15 13:26:10  itamarh
 * B191157 Define HAVE_NETIF_QUEUE in freeswan for linux-3.x
 *
 * Revision 1.6.4.16  2015/07/15 13:21:40  itamarh
 * B191157 Fix freeswan to check correct configs for skb->nfct existence
 *
 * Revision 1.6.2.16  2015/07/15 13:20:00  itamarh
 * B191157 Fix freeswan to check correct configs for skb->nfct existence
 *
 * Revision 1.6.4.15  2015/07/15 12:52:38  itamarh
 * B191157 Use compatible code in freeswan to get current uid
 *
 * Revision 1.6.2.15  2015/07/15 12:51:21  itamarh
 * B191157 Use compatible code in freeswan to get current uid
 *
 * Revision 1.6.4.14  2015/07/15 12:46:08  itamarh
 * B191157 Use compatible code in freeswan to access socket waitqueue
 *
 * Revision 1.6.2.14  2015/07/15 12:44:29  itamarh
 * B191157 Use compatible code in freeswan to access socket waitqueue
 *
 * Revision 1.6.4.13  2015/07/13 16:53:09  itamarh
 * B191157 Fix freeswan sysctl compilation for linux-3.x
 *
 * Revision 1.6.2.13  2015/07/13 16:51:49  itamarh
 * B191157 Fix freeswan sysctl compilation for linux-3.x
 *
 * Revision 1.6.4.12  2015/07/13 16:48:11  itamarh
 * B191157 Fix sk_alloc calls in freeswan for linux-3.x
 *
 * Revision 1.6.2.12  2015/07/13 16:46:50  itamarh
 * B191157 Fix sk_alloc calls in freeswan for linux-3.x
 *
 * Revision 1.6.4.11  2015/07/13 16:41:50  itamarh
 * B191157 Don't change ops to arp_broken_ops if not exported
 *
 * Revision 1.6.2.11  2015/07/13 16:39:34  itamarh
 * B191157 Don't change ops to arp_broken_ops if not exported
 *
 * Revision 1.6.4.10  2015/07/13 16:34:57  itamarh
 * B191157 Use compatible code in freeswan to access inet sock port fields
 *
 * Revision 1.6.2.10  2015/07/13 16:28:24  itamarh
 * B191157 Use compatible code in freeswan to access inet sock port fields
 *
 * Revision 1.6.4.9  2015/07/13 16:05:03  itamarh
 * B191157 Fix freeswan ip_chk_addr macro for linux-3.x
 *
 * Revision 1.6.2.9  2015/07/13 16:02:34  itamarh
 * B191157 Fix freeswan ip_chk_addr macro for linux-3.x
 *
 * Revision 1.6.4.8  2015/07/13 15:44:52  itamarh
 * B191157 Fix freeswan ipsec devices xmit function to be compatible with linux-3.x
 *
 * Revision 1.6.2.8  2015/07/13 15:41:47  itamarh
 * B191157 Fix freeswan ipsec devices xmit function to be compatible with linux-3.x
 *
 * Revision 1.6.4.7  2015/07/13 15:27:38  itamarh
 * B191157 Use compatible code in freeswan to access skb dst
 *
 * Revision 1.6.2.7  2015/07/13 15:26:03  itamarh
 * B191157 Use compatible code in freeswan to access skb dst
 *
 * Revision 1.6.4.6  2015/07/13 15:11:19  itamarh
 * B191157 Use compatible code in freeswan to access skb data pointers
 *
 * Revision 1.6.2.6  2015/07/13 15:08:12  itamarh
 * B191157 Use compatible code in freeswan to access skb data pointers
 *
 * Revision 1.6.4.5  2015/07/13 14:41:05  itamarh
 * B191157 Fix freeswan handling of skb timestamps in linux-3.x
 *
 * Revision 1.6.2.5  2015/07/13 14:39:32  itamarh
 * B191157 Fix freeswan handling of skb timestamps in linux-3.x
 *
 * Revision 1.6.4.4  2015/07/13 14:31:25  itamarh
 * B191157 Compatible code to print IPv4 addresses in printk in freeswan
 *
 * Revision 1.6.2.4  2015/07/13 14:29:50  itamarh
 * B191157 Compatible code to print IPv4 addresses in printk in freeswan
 *
 * Revision 1.6.4.3  2015/07/13 13:37:28  itamarh
 * B191157 Support netdev_ops in freeswan ipsec devices
 *
 * Revision 1.6.2.3  2015/07/13 13:34:54  itamarh
 * B191157 Support netdev_ops in freeswan ipsec devices
 *
 * Revision 1.6.4.2  2015/07/13 13:26:39  itamarh
 * B191157 Change freeswan ipsec devices header ops to be constant, and support header_ops for new kernels
 *
 * Revision 1.6.2.2  2015/07/13 13:24:43  itamarh
 * B191157 Change freeswan ipsec devices header ops to be constant, and support header_ops for new kernels
 *
 * Revision 1.6.4.1  2015/07/13 13:09:19  itamarh
 * B191157 Change freeswan proc API so it's compatible with later kernels as well
 *
 * Revision 1.6.2.1  2015/07/13 13:04:29  itamarh
 * B191157 Change freeswan proc API so it's compatible with later kernels as well
 *
 * Revision 1.6  2006/06/15 21:57:22  itay
 * B33147 Upgrade kernel to linux-2.6.16.14
 * OPTIONS: novalidate
 *
 * Revision 1.5.2.1  2006/06/14 11:36:56  itay
 * B33147 Upgrade kernel to linux-2.6.16.14 and add support for broadcom 6358 (merge from branch-4_2_3_2)
 * OPTIONS: novalidate
 *
 * Revision 1.5  2006/02/23 17:36:43  sergey
 * AUTO MERGE: 1 <- branch-4_2
 * B6059: implement (OpenSWAN based) IPSec NAT-Traversal.
 * NOTIFY: automerge
 *
 * Revision 1.4.2.1  2006/02/23 17:32:13  sergey
 * B6059: implement (OpenSWAN based) IPSec NAT-Traversal.
 *
 * Revision 1.4  2005/11/14 10:54:29  sergey
 * AUTO MERGE: 1 <- branch-4_1
 * B26997: enable IPSec with linux 2.6.
 * NOTIFY: automerge
 *
 * Revision 1.3.172.1  2005/11/14 10:47:20  sergey
 * B26997: enable IPSec with linux 2.6.
 *
 * Revision 1.3  2003/09/21 20:23:15  igork
 * merge branch-dev-2421 into dev
 *
 * Revision 1.1.1.1.34.1  2003/09/16 13:34:20  ron
 * merge from branch-3_1 to merge-dev-branch-dev-2421 into branch-dev-2421
 *
 * Revision 1.2  2003/09/11 11:37:56  yoavp
 * AUTO MERGE: 1 <- branch-3_2
 * R7234: add NONE to range-types, remove RCSIDs from freeswan and some cosmetics
 * NOTIFY: automerge
 *
 * Revision 1.1.1.1.36.1  2003/09/11 11:34:41  yoavp
 * R7234: add NONE to range-types, remove RCSIDs from freeswan and some cosmetics
 *
 * Revision 1.1.1.1  2003/02/19 11:46:31  sergey
 * upgrading freeswan to ver. 1.99.
 *
 * Revision 1.2.2.1  2002/04/12 03:21:43  mcr
 *    fixes for compilation on RH 7.1
 *
 * Revision 1.3  2002/04/12 03:21:17  mcr
 * 	three parameter version of ip_select_ident appears first
 * 	in 2.4.2 (RH7.1) not 2.4.4.
 *
 * Revision 1.2  2002/03/08 21:35:22  rgb
 * Defined LINUX_KERNEL_HAS_SNPRINTF to shut up compiler warnings after
 * 2.4.9.  (Andreas Piesk).
 *
 * Revision 1.1  2002/01/29 02:11:42  mcr
 * 	removal of kversions.h - sources that needed it now use ipsec_param.h.
 * 	updating of IPv6 structures to match latest in6.h version.
 * 	removed dead code from freeswan.h that also duplicated kversions.h
 * 	code.
 *
 *
 */
