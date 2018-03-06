/****************************************************************************
 *
 * rg/pkg/ppp/kernel/be_pppos.c * 
 * 
 * Modifications by Jungo Ltd. Copyright (C) 2008 Jungo Ltd.  
 * All Rights reserved. 
 * 
 * This program is free software; 
 * you can redistribute it and/or modify it under 
 * the terms of the GNU General Public License version 2 as published by 
 * the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty 
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
 * See the GNU General Public License v2 for more details.
 * 
 * You should have received a copy of the GNU General Public License 
 * along with this program. 
 * If not, write to the Free Software Foundation, 
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA, 
 * or contact Jungo Ltd. 
 * at http://www.jungo.com/openrg/opensource_ack.html
 * 
 * 
 * 
 */

/*
 * PPP async serial channel driver for Linux.
 *
 * Copyright 1999 Paul Mackerras.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 *
 * This driver provides the encapsulation and framing for sending
 * and receiving PPP frames over async serial lines.  It relies on
 * the generic PPP layer to give it frames to send and to process
 * received frames.  It implements the PPP line discipline.
 *
 * Part of the code in this driver was inspired by the old async-only
 * PPP driver, written by Michael Callahan and Al Longyear, and
 * subsequently hacked by Paul Mackerras.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/tty.h>
#include <linux/netdevice.h>
#include <linux/poll.h>
#include <linux/crc-ccitt.h>
#include <linux/ppp_defs.h>
#include <linux/if_ppp.h>
#include <linux/ppp_channel.h>
#include <linux/spinlock.h>
#include <linux/init.h>
/* linux git a6b7a407865aab9f849dd99a71072b7cd1175116 */
#ifdef CONFIG_RG_OS_LINUX_3X
#include <linux/interrupt.h>
#endif
#include <linux/jiffies.h>
/* linux git 5a0e3ad6af8660be21ca98a971cd00f331318c05 */
#ifdef CONFIG_RG_OS_LINUX_3X
#include <linux/slab.h>
#endif
#include <asm/uaccess.h>
#include <asm/string.h>

#include <be_api_gpl.h>

#define PPP_VERSION	"2.4.2"

/* Upstream commit 208f2037ae4a2f23fe5f232d25f4030b3a35c3ed */
#define OBUFSIZE	4096

/* Structure for storing local state. */
struct asyncppp {
	/* Generic PPP context. */
	struct ppp_softc *ppp_sc;
	rwlock_t sc_lock;

	struct tty_struct *tty;
	unsigned int	flags;
	unsigned int	state;
	unsigned int	rbits;
	int		mru;
	spinlock_t	xmit_lock;
	spinlock_t	recv_lock;
	unsigned long	xmit_flags;
	u32		xaccm[8];
	u32		raccm;
	unsigned int	bytes_sent;
	unsigned int	bytes_rcvd;

	struct sk_buff	*tpkt;
	int		tpkt_pos;
	u16		tfcs;
	unsigned char	*optr;
	unsigned char	*olim;
	unsigned long	last_xmit;

	struct sk_buff	*rpkt;
	int		lcp_fcs;
	struct sk_buff_head rqueue;

	struct tasklet_struct tsk;

	atomic_t	refcnt;
	struct semaphore dead_sem;
	unsigned char	obuf[OBUFSIZE];
};

/* Bit numbers in xmit_flags */
#define XMIT_WAKEUP	0
#define XMIT_FULL	1
#define XMIT_BUSY	2

/* State bits */
#define SC_TOSS		1
#define SC_ESCAPE	2
#define SC_PREV_ERROR	4

/* Bits in rbits */
#define SC_RCV_BITS	(SC_RCV_B7_1|SC_RCV_B7_0|SC_RCV_ODDP|SC_RCV_EVNP)

static int flag_time = HZ;
module_param(flag_time, int, 0);
MODULE_PARM_DESC(flag_time, "ppp_async: interval between flagged packets (in clock ticks)");
MODULE_LICENSE("GPL");
MODULE_ALIAS_LDISC(N_PPP);

static ppp_ops_t ppposerial_ops;

/*
 * Prototypes.
 */
static int ppp_async_encode(struct asyncppp *ap);
static ppp_send_res_t ppp_async_send(void *ctx, struct sk_buff **m);
static int ppp_async_push(struct asyncppp *ap);
static void ppp_async_flush_output(struct asyncppp *ap);
static void ppp_async_input(struct asyncppp *ap, const unsigned char *buf,
			    char *flags, int count);
static void ppp_async_process(unsigned long arg);

static void async_lcp_peek(struct asyncppp *ap, unsigned char *data,
			   int len, int inbound);

/*
 * Routines implementing the PPP line discipline.
 */

/*
 * We have a potential race on dereferencing tty->disc_data,
 * because the tty layer provides no locking at all - thus one
 * cpu could be running ppp_asynctty_receive while another
 * calls ppp_asynctty_close, which zeroes tty->disc_data and
 * frees the memory that ppp_asynctty_receive is using.  The best
 * way to fix this is to use a rwlock in the tty struct, but for now
 * we use a single global rwlock for all ttys in ppp line discipline.
 *
 * FIXME: this is no longer true. The _close path for the ldisc is
 * now guaranteed to be sane.
 */
static DEFINE_RWLOCK(disc_data_lock);

static struct asyncppp *ap_get(struct tty_struct *tty)
{
	struct asyncppp *ap;

	read_lock(&disc_data_lock);
	ap = tty->disc_data;
	if (ap != NULL)
		atomic_inc(&ap->refcnt);
	read_unlock(&disc_data_lock);
	return ap;
}

static void ap_put(struct asyncppp *ap)
{
	if (atomic_dec_and_test(&ap->refcnt))
		up(&ap->dead_sem);
}

/*
 * Called when a tty is put into PPP line discipline. Called in process
 * context.
 */
static int
ppp_asynctty_open(struct tty_struct *tty)
{
	struct asyncppp *ap;
	int err;

	/* linux git f34d7a5b7010b82fe97da95496b9971435530062 */
#ifdef CONFIG_RG_OS_LINUX_3X
	if (tty->ops->write == NULL)
	    return -EOPNOTSUPP;
#endif

	err = -ENOMEM;
	ap = kmalloc(sizeof(*ap), GFP_KERNEL);
	if (ap == 0)
		goto out;

	/* initialize the asyncppp structure */
	memset(ap, 0, sizeof(*ap));
	ap->tty = tty;
	ap->mru = PPP_MRU;
	spin_lock_init(&ap->xmit_lock);
	spin_lock_init(&ap->recv_lock);
	ap->xaccm[0] = ~0U;
	ap->xaccm[3] = 0x60000000U;
	ap->raccm = ~0U;
	ap->optr = ap->obuf;
	ap->olim = ap->obuf;
	ap->lcp_fcs = -1;
	rwlock_init(&ap->sc_lock);

	skb_queue_head_init(&ap->rqueue);
	tasklet_init(&ap->tsk, ppp_async_process, (unsigned long) ap);

	atomic_set(&ap->refcnt, 1);
	/* linux git 0bce198b7688911e072bd65032ab9278b577ee93 */
#ifdef CONFIG_RG_OS_LINUX_26
	init_MUTEX_LOCKED(&ap->dead_sem);
#elif defined(CONFIG_RG_OS_LINUX_3X)
	sema_init(&ap->dead_sem, 0);
#endif

	tty->disc_data = ap;
	tty->receive_room = 65536;
	return 0;

  out:
	return err;
}

static void __ppposerial_detach(struct asyncppp *ap)
{
    struct ppp_softc *ppp_sc;

    write_lock_bh(&ap->sc_lock);
    ppp_sc = ap->ppp_sc;
    ap->ppp_sc = NULL;
    write_unlock_bh(&ap->sc_lock);

    if (!ppp_sc)
	return;

    ppposerial_ops.be_detach(ppp_sc);
}

/*
 * Called when the tty is put into another line discipline
 * or it hangs up.  We have to wait for any cpu currently
 * executing in any of the other ppp_asynctty_* routines to
 * finish before we can call ppp_unregister_channel and free
 * the asyncppp struct.  This routine must be called from
 * process context, not interrupt or softirq context.
 */
static void
ppp_asynctty_close(struct tty_struct *tty)
{
	struct asyncppp *ap;

	write_lock_irq(&disc_data_lock);
	ap = tty->disc_data;
	tty->disc_data = NULL;
	write_unlock_irq(&disc_data_lock);
	if (ap == 0)
		return;

	/* We may reach here when physically disconnecting USB, while PPP is
	 * connected, in which case OpenRG did not have time to receive the
	 * notification and free the sc itself.
	 * We must do it now to prevent usage of asyncppp ptr after it's freed.
	 * Performing detach again after this is ok.
	 */
	__ppposerial_detach(ap);

	/*
	 * We have now ensured that nobody can start using ap from now
	 * on, but we have to wait for all existing users to finish.
	 * Note that ppp_unregister_channel ensures that no calls to
	 * our channel ops (i.e. ppp_async_send/ioctl) are in progress
	 * by the time it returns.
	 */
	if (!atomic_dec_and_test(&ap->refcnt))
		down(&ap->dead_sem);
	tasklet_kill(&ap->tsk);

	if (ap->ppp_sc)
		panic("Closing N_PPP ldisc while PPP is attached!\n");

	if (ap->rpkt != 0)
		kfree_skb(ap->rpkt);
	skb_queue_purge(&ap->rqueue);
	if (ap->tpkt != 0)
		kfree_skb(ap->tpkt);
	kfree(ap);
}

/*
 * Called on tty hangup in process context.
 */
static int ppp_asynctty_hangup(struct tty_struct *tty)
{
	return 0;
}

/*
 * Read does nothing - no data is ever available this way.
 * Pppd reads and writes packets via /dev/ppp instead.
 */
static ssize_t
ppp_asynctty_read(struct tty_struct *tty, struct file *file,
		  unsigned char __user *buf, size_t count)
{
	return -EAGAIN;
}

/*
 * Write on the tty does nothing, the packets all come in
 * from the ppp generic stuff.
 */
static ssize_t
ppp_asynctty_write(struct tty_struct *tty, struct file *file,
		   const unsigned char *buf, size_t count)
{
	return -EAGAIN;
}

/*
 * Called in process context only. May be re-entered by multiple
 * ioctl calling threads.
 */

static int
ppp_asynctty_ioctl(struct tty_struct *tty, struct file *file,
		   unsigned int cmd, unsigned long arg)
{
	struct asyncppp *ap = ap_get(tty);
	int err, val;
	int __user *p = (int __user *)arg;

	if (ap == 0)
		return -ENXIO;
	err = -EFAULT;
	switch (cmd) {
	/* linux git d0127539ea9b5fcfe1a1d7d4d57f12384da5190c */
#ifdef CONFIG_RG_OS_LINUX_26
	case TCGETS:
	case TCGETA:
		err = n_tty_ioctl(tty, file, cmd, arg);
		break;
#endif

	case TCFLSH:
		/* flush our buffers and the serial port's buffer */
		if (arg == TCIOFLUSH || arg == TCOFLUSH)
			ppp_async_flush_output(ap);
	/* linux git d0127539ea9b5fcfe1a1d7d4d57f12384da5190c */
#ifdef CONFIG_RG_OS_LINUX_26
		err = n_tty_ioctl(tty, file, cmd, arg);
#elif defined(CONFIG_RG_OS_LINUX_3X)
		err = tty_perform_flush(tty, arg);
#endif
		break;

	case FIONREAD:
		val = 0;
		if (put_user(val, p))
			break;
		err = 0;
		break;

	default:
	/* linux git d0127539ea9b5fcfe1a1d7d4d57f12384da5190c */
#ifdef CONFIG_RG_OS_LINUX_26
		err = -ENOIOCTLCMD;
#elif defined(CONFIG_RG_OS_LINUX_3X)
		/* Try the various mode ioctls */
		err = tty_mode_ioctl(tty, file, cmd, arg);
#endif
	}

	ap_put(ap);
	return err;
}

/* No kernel lock - fine */
static unsigned int
ppp_asynctty_poll(struct tty_struct *tty, struct file *file, poll_table *wait)
{
	return 0;
}

/*
 * This can now be called from hard interrupt level as well
 * as soft interrupt level or mainline.
 */
static void
ppp_asynctty_receive(struct tty_struct *tty, const unsigned char *buf,
		  char *cflags, int count)
{
	struct asyncppp *ap = ap_get(tty);
	unsigned long flags;

	if (ap == 0)
		return;
	spin_lock_irqsave(&ap->recv_lock, flags);
	ppp_async_input(ap, buf, cflags, count);
	spin_unlock_irqrestore(&ap->recv_lock, flags);
	if (!skb_queue_empty(&ap->rqueue))
		tasklet_schedule(&ap->tsk);
	ap_put(ap);
	/* linux git 39c2e60f8c584c1b29b5c4375dd49df7995386bb */
#ifdef CONFIG_RG_OS_LINUX_26
	if (test_and_clear_bit(TTY_THROTTLED, &tty->flags)
	    && tty->driver->unthrottle)
		tty->driver->unthrottle(tty);
#elif defined(CONFIG_RG_OS_LINUX_3X)
	tty_unthrottle(tty);
#endif
}

static void
ppp_asynctty_wakeup(struct tty_struct *tty)
{
	struct asyncppp *ap = ap_get(tty);

	clear_bit(TTY_DO_WRITE_WAKEUP, &tty->flags);
	if (ap == 0)
		return;
	set_bit(XMIT_WAKEUP, &ap->xmit_flags);
	tasklet_schedule(&ap->tsk);
	ap_put(ap);
}


/* linux git a352def21a642133758b868c71bee12ab34ad5c5 */
#ifdef CONFIG_RG_OS_LINUX_26
static struct tty_ldisc ppp_ldisc = {
#elif defined(CONFIG_RG_OS_LINUX_3X)
static struct tty_ldisc_ops ppp_ldisc = {
#endif
	.owner  = THIS_MODULE,
	.magic	= TTY_LDISC_MAGIC,
	.name	= "ppp",
	.open	= ppp_asynctty_open,
	.close	= ppp_asynctty_close,
	.hangup	= ppp_asynctty_hangup,
	.read	= ppp_asynctty_read,
	.write	= ppp_asynctty_write,
	.ioctl	= ppp_asynctty_ioctl,
	.poll	= ppp_asynctty_poll,
	.receive_buf = ppp_asynctty_receive,
	.write_wakeup = ppp_asynctty_wakeup,
};

/*
 * This is called at softirq level to deliver received packets
 * to the ppp_generic code, and to tell the ppp_generic code
 * if we can accept more output now.
 */
static void ppp_async_process(unsigned long arg)
{
	struct asyncppp *ap = (struct asyncppp *) arg;
	struct sk_buff *skb;

	read_lock(&ap->sc_lock);
	if (!ap->ppp_sc)
	    goto Exit;

	/* process received packets */
	while ((skb = skb_dequeue(&ap->rqueue)) != NULL) {
#if 0
		if (skb->cb[0])
			ppp_input_error(&ap->chan, 0);
#endif
		ppposerial_ops.input(ap->ppp_sc, skb);
	}

	/* try to push more stuff out */
	if (test_bit(XMIT_WAKEUP, &ap->xmit_flags))
		ppp_async_push(ap);

Exit:
	read_unlock(&ap->sc_lock);
}

/*
 * Procedures for encapsulation and framing.
 */

/*
 * Procedure to encode the data for async serial transmission.
 * Does octet stuffing (escaping), puts the address/control bytes
 * on if A/C compression is disabled, and does protocol compression.
 * Assumes ap->tpkt != 0 on entry.
 * Returns 1 if we finished the current frame, 0 otherwise.
 */

#define PUT_BYTE(ap, buf, c, islcp)	do {		\
	if ((islcp && c < 0x20) || (ap->xaccm[c >> 5] & (1 << (c & 0x1f)))) {\
		*buf++ = PPP_ESCAPE;			\
		*buf++ = c ^ 0x20;			\
	} else						\
		*buf++ = c;				\
} while (0)

static int
ppp_async_encode(struct asyncppp *ap)
{
	int fcs, i, count, c, proto;
	unsigned char *buf, *buflim;
	unsigned char *data;
	int islcp;

	buf = ap->obuf;
	ap->olim = buf;
	ap->optr = buf;
	i = ap->tpkt_pos;
	data = ap->tpkt->data;
	count = ap->tpkt->len;
	fcs = ap->tfcs;
	proto = (data[0] << 8) + data[1];

	/*
	 * LCP packets with code values between 1 (configure-reqest)
	 * and 7 (code-reject) must be sent as though no options
	 * had been negotiated.
	 */
	islcp = proto == PPP_LCP && 1 <= data[2] && data[2] <= 7;

	if (i == 0) {
		if (islcp)
			async_lcp_peek(ap, data, count, 0);

		/*
		 * Start of a new packet - insert the leading FLAG
		 * character if necessary.
		 */
		if (islcp || flag_time == 0
		    || time_after_eq(jiffies, ap->last_xmit + flag_time))
			*buf++ = PPP_FLAG;
		ap->last_xmit = jiffies;
		fcs = PPP_INITFCS;

		/*
		 * Put in the address/control bytes if necessary
		 */
		if ((ap->flags & SC_COMP_AC) == 0 || islcp) {
			PUT_BYTE(ap, buf, 0xff, islcp);
			fcs = PPP_FCS(fcs, 0xff);
			PUT_BYTE(ap, buf, 0x03, islcp);
			fcs = PPP_FCS(fcs, 0x03);
		}
	}

	/*
	 * Once we put in the last byte, we need to put in the FCS
	 * and closing flag, so make sure there is at least 7 bytes
	 * of free space in the output buffer.
	 */
	buflim = ap->obuf + OBUFSIZE - 6;
	while (i < count && buf < buflim) {
		c = data[i++];
		if (i == 1 && c == 0 && (ap->flags & SC_COMP_PROT))
			continue;	/* compress protocol field */
		fcs = PPP_FCS(fcs, c);
		PUT_BYTE(ap, buf, c, islcp);
	}

	if (i < count) {
		/*
		 * Remember where we are up to in this packet.
		 */
		ap->olim = buf;
		ap->tpkt_pos = i;
		ap->tfcs = fcs;
		return 0;
	}

	/*
	 * We have finished the packet.  Add the FCS and flag.
	 */
	fcs = ~fcs;
	c = fcs & 0xff;
	PUT_BYTE(ap, buf, c, islcp);
	c = (fcs >> 8) & 0xff;
	PUT_BYTE(ap, buf, c, islcp);
	*buf++ = PPP_FLAG;
	ap->olim = buf;

	kfree_skb(ap->tpkt);
	ap->tpkt = NULL;
	return 1;
}

/*
 * Transmit-side routines.
 */

/*
 * Send a packet to the peer over an async tty line.
 * Returns 1 iff the packet was accepted.
 * If the packet was not accepted, we will call ppp_output_wakeup
 * at some later time.
 */
static ppp_send_res_t
ppp_async_send(void *ctx, struct sk_buff **skb)
{
	struct asyncppp *ap = ctx;

	ppp_async_push(ap);

	if (ap->tty->flags & (1UL << TTY_HUPPED))
		return PPP_PACKET_DROPPED;
	if (test_and_set_bit(XMIT_FULL, &ap->xmit_flags))
		return PPP_RETRY_LATER;	/* already full */
	ap->tpkt = *skb;
	ap->tpkt_pos = 0;

	ppp_async_push(ap);
	return PPP_PACKET_SENT;
}

/*
 * Push as much data as possible out to the tty.
 */
static int
ppp_async_push(struct asyncppp *ap)
{
	int avail, sent, done = 0;
	struct tty_struct *tty = ap->tty;
	int tty_stuffed = 0;

	/*
	 * We can get called recursively here if the tty write
	 * function calls our wakeup function.  This can happen
	 * for example on a pty with both the master and slave
	 * set to PPP line discipline.
	 * We use the XMIT_BUSY bit to detect this and get out,
	 * leaving the XMIT_WAKEUP bit set to tell the other
	 * instance that it may now be able to write more now.
	 */
	if (test_and_set_bit(XMIT_BUSY, &ap->xmit_flags))
		return 0;
	spin_lock_bh(&ap->xmit_lock);
	for (;;) {
		if (test_and_clear_bit(XMIT_WAKEUP, &ap->xmit_flags))
			tty_stuffed = 0;
		if (!tty_stuffed && ap->optr < ap->olim) {
			avail = ap->olim - ap->optr;
			set_bit(TTY_DO_WRITE_WAKEUP, &tty->flags);
			/* linux git f34d7a5b7010b82fe97da95496b9971435530062 */
#ifdef CONFIG_RG_OS_LINUX_26
			sent = tty->driver->write(tty, ap->optr, avail);
#elif defined(CONFIG_RG_OS_LINUX_3X)
			sent = tty->ops->write(tty, ap->optr, avail);
#endif
			if (sent < 0)
				goto flush;	/* error, e.g. loss of CD */
			ap->optr += sent;
			if (sent < avail)
				tty_stuffed = 1;
			continue;
		}
		if (ap->optr >= ap->olim && ap->tpkt != 0) {
			if (ppp_async_encode(ap)) {
				/* finished processing ap->tpkt */
				clear_bit(XMIT_FULL, &ap->xmit_flags);
				done = 1;
			}
			continue;
		}
		/*
		 * We haven't made any progress this time around.
		 * Clear XMIT_BUSY to let other callers in, but
		 * after doing so we have to check if anyone set
		 * XMIT_WAKEUP since we last checked it.  If they
		 * did, we should try again to set XMIT_BUSY and go
		 * around again in case XMIT_BUSY was still set when
		 * the other caller tried.
		 */
		clear_bit(XMIT_BUSY, &ap->xmit_flags);
		/* any more work to do? if not, exit the loop */
		if (!(test_bit(XMIT_WAKEUP, &ap->xmit_flags)
		      || (!tty_stuffed && ap->tpkt != 0)))
			break;
		/* more work to do, see if we can do it now */
		if (test_and_set_bit(XMIT_BUSY, &ap->xmit_flags))
			break;
	}
	spin_unlock_bh(&ap->xmit_lock);
	return done;

flush:
	clear_bit(XMIT_BUSY, &ap->xmit_flags);
	if (ap->tpkt != 0) {
		kfree_skb(ap->tpkt);
		ap->tpkt = NULL;
		clear_bit(XMIT_FULL, &ap->xmit_flags);
		done = 1;
	}
	ap->optr = ap->olim;
	spin_unlock_bh(&ap->xmit_lock);
	return done;
}

/*
 * Flush output from our internal buffers.
 * Called for the TCFLSH ioctl. Can be entered in parallel
 * but this is covered by the xmit_lock.
 */
static void
ppp_async_flush_output(struct asyncppp *ap)
{
	int done = 0;

	spin_lock_bh(&ap->xmit_lock);
	ap->optr = ap->olim;
	if (ap->tpkt != NULL) {
		kfree_skb(ap->tpkt);
		ap->tpkt = NULL;
		clear_bit(XMIT_FULL, &ap->xmit_flags);
		done = 1;
	}
	spin_unlock_bh(&ap->xmit_lock);
#if 0
	if (done)
		ppp_output_wakeup(&ap->chan);
#endif
}

/*
 * Receive-side routines.
 */

/* see how many ordinary chars there are at the start of buf */
static inline int
scan_ordinary(struct asyncppp *ap, const unsigned char *buf, int count)
{
	int i, c;

	for (i = 0; i < count; ++i) {
		c = buf[i];
		if (c == PPP_ESCAPE || c == PPP_FLAG
		    || (c < 0x20 && (ap->raccm & (1 << c)) != 0))
			break;
	}
	return i;
}

/* called when a flag is seen - do end-of-packet processing */
static void
process_input_packet(struct asyncppp *ap)
{
	struct sk_buff *skb;
	unsigned char *p;
	unsigned int len, fcs, proto;

	skb = ap->rpkt;
	if (ap->state & (SC_TOSS | SC_ESCAPE))
		goto err;

	if (skb == NULL)
		return;		/* 0-length packet */

	/* check the FCS */
	p = skb->data;
	len = skb->len;
	if (len < 3)
		goto err;	/* too short */
	fcs = PPP_INITFCS;
	for (; len > 0; --len)
		fcs = PPP_FCS(fcs, *p++);
	if (fcs != PPP_GOODFCS)
		goto err;	/* bad FCS */
	skb_trim(skb, skb->len - 2);

	/* check for address/control and protocol compression */
	p = skb->data;
	if (p[0] == PPP_ALLSTATIONS) {
		/* chop off address/control */
		if (p[1] != PPP_UI || skb->len < 3)
			goto err;
		p = skb_pull(skb, 2);
	}
	proto = p[0];
	if (proto & 1) {
		/* protocol is compressed */
		skb_push(skb, 1)[0] = 0;
	} else {
		if (skb->len < 2)
			goto err;
		proto = (proto << 8) + p[1];
		if (proto == PPP_LCP)
			async_lcp_peek(ap, p, skb->len, 1);
	}

	/* queue the frame to be processed */
	skb->cb[0] = ap->state;
	skb_queue_tail(&ap->rqueue, skb);
	ap->rpkt = NULL;
	ap->state = 0;
	return;

 err:
	/* frame had an error, remember that, reset SC_TOSS & SC_ESCAPE */
	ap->state = SC_PREV_ERROR;
	if (skb) {
		/* make skb appear as freshly allocated */
		skb_trim(skb, 0);
		skb_reserve(skb, - skb_headroom(skb));
	}
}

/* Called when the tty driver has data for us. Runs parallel with the
   other ldisc functions but will not be re-entered */

static void
ppp_async_input(struct asyncppp *ap, const unsigned char *buf,
		char *flags, int count)
{
	struct sk_buff *skb;
	int c, i, j, n, s, f;
	unsigned char *sp;

	/* update bits used for 8-bit cleanness detection */
	if (~ap->rbits & SC_RCV_BITS) {
		s = 0;
		for (i = 0; i < count; ++i) {
			c = buf[i];
			if (flags != 0 && flags[i] != 0)
				continue;
			s |= (c & 0x80)? SC_RCV_B7_1: SC_RCV_B7_0;
			c = ((c >> 4) ^ c) & 0xf;
			s |= (0x6996 & (1 << c))? SC_RCV_ODDP: SC_RCV_EVNP;
		}
		ap->rbits |= s;
	}

	while (count > 0) {
		/* scan through and see how many chars we can do in bulk */
		if ((ap->state & SC_ESCAPE) && buf[0] == PPP_ESCAPE)
			n = 1;
		else
			n = scan_ordinary(ap, buf, count);

		f = 0;
		if (flags != 0 && (ap->state & SC_TOSS) == 0) {
			/* check the flags to see if any char had an error */
			for (j = 0; j < n; ++j)
				if ((f = flags[j]) != 0)
					break;
		}
		if (f != 0) {
			/* start tossing */
			ap->state |= SC_TOSS;

		} else if (n > 0 && (ap->state & SC_TOSS) == 0) {
			/* stuff the chars in the skb */
			skb = ap->rpkt;
			if (skb == 0) {
				skb = dev_alloc_skb(ap->mru + PPP_HDRLEN + 2);
				if (skb == 0)
					goto nomem;
 				ap->rpkt = skb;
 			}
 			if (skb->len == 0) {
 				/* Try to get the payload 4-byte aligned.
 				 * This should match the
 				 * PPP_ALLSTATIONS/PPP_UI/compressed tests in
 				 * process_input_packet, but we do not have
 				 * enough chars here to test buf[1] and buf[2].
 				 */
				if (buf[0] != PPP_ALLSTATIONS)
					skb_reserve(skb, 2 + (buf[0] & 1));
			}
			if (n > skb_tailroom(skb)) {
				/* packet overflowed MRU */
				ap->state |= SC_TOSS;
			} else {
				sp = skb_put(skb, n);
				memcpy(sp, buf, n);
				if (ap->state & SC_ESCAPE) {
					sp[0] ^= 0x20;
					ap->state &= ~SC_ESCAPE;
				}
			}
		}

		if (n >= count)
			break;

		c = buf[n];
		if (flags != NULL && flags[n] != 0) {
			ap->state |= SC_TOSS;
		} else if (c == PPP_FLAG) {
			process_input_packet(ap);
		} else if (c == PPP_ESCAPE) {
			ap->state |= SC_ESCAPE;
		} else if (I_IXON(ap->tty)) {
			if (c == START_CHAR(ap->tty))
				start_tty(ap->tty);
			else if (c == STOP_CHAR(ap->tty))
				stop_tty(ap->tty);
		}
		/* otherwise it's a char in the recv ACCM */
		++n;

		buf += n;
		if (flags != 0)
			flags += n;
		count -= n;
	}
	return;

 nomem:
	printk(KERN_ERR "PPPasync: no memory (input pkt)\n");
	ap->state |= SC_TOSS;
}

/*
 * We look at LCP frames going past so that we can notice
 * and react to the LCP configure-ack from the peer.
 * In the situation where the peer has been sent a configure-ack
 * already, LCP is up once it has sent its configure-ack
 * so the immediately following packet can be sent with the
 * configured LCP options.  This allows us to process the following
 * packet correctly without pppd needing to respond quickly.
 *
 * We only respond to the received configure-ack if we have just
 * sent a configure-request, and the configure-ack contains the
 * same data (this is checked using a 16-bit crc of the data).
 */
#define CONFREQ		1	/* LCP code field values */
#define CONFACK		2
#define LCP_MRU		1	/* LCP option numbers */
#define LCP_ASYNCMAP	2

static void async_lcp_peek(struct asyncppp *ap, unsigned char *data,
			   int len, int inbound)
{
	int dlen, fcs, i, code;
	u32 val;

	data += 2;		/* skip protocol bytes */
	len -= 2;
	if (len < 4)		/* 4 = code, ID, length */
		return;
	code = data[0];
	if (code != CONFACK && code != CONFREQ)
		return;
	dlen = (data[2] << 8) + data[3];
	if (len < dlen)
		return;		/* packet got truncated or length is bogus */

	if (code == (inbound? CONFACK: CONFREQ)) {
		/*
		 * sent confreq or received confack:
		 * calculate the crc of the data from the ID field on.
		 */
		fcs = PPP_INITFCS;
		for (i = 1; i < dlen; ++i)
			fcs = PPP_FCS(fcs, data[i]);

		if (!inbound) {
			/* outbound confreq - remember the crc for later */
			ap->lcp_fcs = fcs;
			return;
		}

		/* received confack, check the crc */
		fcs ^= ap->lcp_fcs;
		ap->lcp_fcs = -1;
		if (fcs != 0)
			return;
	} else if (inbound)
		return;	/* not interested in received confreq */

	/* process the options in the confack */
	data += 4;
	dlen -= 4;
	/* data[0] is code, data[1] is length */
	while (dlen >= 2 && dlen >= data[1] && data[1] >= 2) {
		switch (data[0]) {
		case LCP_MRU:
			val = (data[2] << 8) + data[3];
			if (inbound)
				ap->mru = val;
#if 0
			else
				ap->chan.mtu = val;
#endif
			break;
		case LCP_ASYNCMAP:
			val = (data[2] << 24) + (data[3] << 16)
				+ (data[4] << 8) + data[5];
			if (inbound)
				ap->raccm = val;
			else
				ap->xaccm[0] = val;
			break;
		}
		dlen -= data[1];
		data += data[1];
	}
}

struct tty_struct *get_filp_tty(struct file *f);

void *ppposerial_attach(pppos_binding_t *binding)
{
    struct file *f = filp_open(binding->tty_name, O_RDWR, 0);
    struct tty_struct *tty = NULL;
    struct asyncppp *ap;
    int ret = -EFAULT;

    if (!f || !(tty = get_filp_tty(f)))
        goto Exit;

    read_lock(&disc_data_lock);
    ap = tty->disc_data;
    read_unlock(&disc_data_lock);

    if (!ap)
        goto Exit;

    write_lock_bh(&ap->sc_lock);
    if (!(ap->ppp_sc = ppposerial_ops.be_attach(binding->if_name, 0, 0,
        ppp_async_send, ap)))
    {
	write_unlock_bh(&ap->sc_lock);
        goto Exit;
    }
    write_unlock_bh(&ap->sc_lock);

    ret = 0;

Exit:
    if (ret && f)
    {
        filp_close(f, 0);
        f = NULL;
    }

    return f;
}
EXPORT_SYMBOL(ppposerial_attach);

int ppposerial_detach(void *ctx)
{
    struct file *f = ctx;
    struct tty_struct *tty = NULL;
    struct asyncppp *ap = NULL;
    int ret = -EFAULT;

    if (!f || !(tty = get_filp_tty(f)))
        goto Exit;

    if (!(ap = ap_get(tty)))
        goto Exit;
   
    /* Do not return error in case of missing sc, it might have been detached
     * already following a usb disconnect. */
    __ppposerial_detach(ap);

    ret = 0;

Exit:
    if (ap)
        ap_put(ap);
    if (f)
        filp_close(f, 0);

    return ret;
}
EXPORT_SYMBOL(ppposerial_detach);

int ppposerial_start(ppp_ops_t *ops)
{
    int ret;

    ppposerial_ops = *ops;

    if ((ret = tty_register_ldisc(N_PPP, &ppp_ldisc)))
        return -EFAULT;

    return 0;
}
EXPORT_SYMBOL(ppposerial_start);

void ppposerial_stop(void)
{
    tty_unregister_ldisc(N_PPP);

    memset(&ppposerial_ops, 0, sizeof(ppposerial_ops));
}
EXPORT_SYMBOL(ppposerial_stop);

static int __init ppposerial_init(void)
{
    return 0;
}

static void __exit ppposerial_exit(void)
{
}

module_init(ppposerial_init);
module_exit(ppposerial_exit);
