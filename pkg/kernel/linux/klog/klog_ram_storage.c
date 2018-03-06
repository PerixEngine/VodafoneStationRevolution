/****************************************************************************
 *
 * rg/pkg/kernel/linux/klog/klog_ram_storage.c * 
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

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <asm/io.h>
#include "klog.h"

#define KLOG_MAGIC 0x6B6C6F67 /* The magic is the hex representation of klog */

/* Uncomment the following macro in order to print klog_ram header fields upon
 * invalid checksum discovery */
/* #define DEBUG_KLOG */

typedef struct klog_ram_info_t {
    /* magic and *_checksum are used to validate the information
     * in the buffer.
     * The following 3 fields below, must be placed in the beginning
     * of structure and their order must not be changed
     */
    unsigned int magic;
    unsigned int header_checksum;
    unsigned int data_checksum;

    /* Kernel or Usermode crash indicator. */
    unsigned int has_crashed;

    /* Number of reboots stored in RAM buffer. */
    unsigned int num_reboots;

    /* Crash timestamp */
    struct timeval timestamp;

    /* Cyclic buffer management offsets start and end. */
    unsigned int start;
    unsigned int end;
    unsigned int buf_size;
    char data[];
} klog_ram_info_t;

extern unsigned long klog_ram_paddr;

static klog_ram_info_t *klog_ram;
static unsigned int klog_ram_size = CONFIG_RG_KLOG_RAMSIZE;
static klog_storage_data_t klog_crash = {};
static klog_storage_data_t klog_buf = {};
static spinlock_t klog_ram_spinlock;
static inline void csum_update_field(unsigned int *field, unsigned int value);
static inline void csum_update_field_custom_size(u8 *field,
    u8 *value, unsigned int sizeof_field);

static inline unsigned int csum_partial(u8 *address, unsigned int len,
    unsigned int sum)
{
    register u32 chksum = sum;
    register u32 tmp;

    /* Process unaligned part */
    while (((u32)address & 3) && len)
    {
	chksum += *(address++);
	len--;
    }

    /* Process aligned part */
    while (len >= 4)
    {
	tmp = *((u32 *)address);
	chksum += tmp & 0xff;
	tmp >>= 8;
	chksum += tmp & 0xff;
	tmp >>= 8;
	chksum += tmp & 0xff;
	tmp >>= 8;
	chksum += tmp & 0xff;
	
	len -= 4;
	address += 4;
    }

    /* Process unaligned leftovers */
    while (len--)
	chksum += *(address++);

    return chksum;
}

static unsigned int klog_ram_header_csum_compute(void)
{
    unsigned int csum, header_size;

    header_size = (u32)&klog_ram->data - (u32)&klog_ram->data_checksum;
    csum = csum_partial((u8*)&klog_ram->data_checksum, header_size, 0);
    return csum;
}

static unsigned int klog_ram_data_csum_compute(void)
{
    unsigned int csum;

    csum = csum_partial(klog_ram->data, klog_ram->buf_size, 0);
    return csum;
}

static int klog_ram_valid(void)
{
    int rc = 0;

    spin_lock(&klog_ram_spinlock);
    if (klog_ram->magic != KLOG_MAGIC)
	goto Exit;

    if (klog_ram->header_checksum != klog_ram_header_csum_compute() ||
	klog_ram->data_checksum != klog_ram_data_csum_compute())
    {
#ifdef DEBUG_KLOG
	printk(KERN_ALERT "DEBUG_KLOG %s klog_ram->magic 0x%x, " \
	    "klog_ram->header_checksum 0x%x(expected to be 0x%x), " \
	    "klog_ram->data_checksum 0x%x klog_ram->has_crashed 0x%x, " \
	    "klog_ram->num_reboots 0x%x, klog_ram->timestamp.tv_sec %ld, "
	    "klog_ram->timestamp.tv_usec %ld klog_ram->start 0x%x, " \
	    "klog_ram->end 0x%x, klog_ram->buf_size 0x%x  \n", __FUNCTION__,
	    klog_ram->magic, klog_ram->header_checksum,
	    klog_ram_header_csum_compute(), klog_ram->data_checksum,
	    klog_ram->has_crashed, klog_ram->num_reboots,
	    klog_ram->timestamp.tv_sec, klog_ram->timestamp.tv_usec,
	    klog_ram->start, klog_ram->end, klog_ram->buf_size);

	if (klog_ram->header_checksum == klog_ram_header_csum_compute())
	    printk(KERN_ALERT "DEBUG_KLOG %s invalid klog_ram->data_checksum " \
		"0x%x while should be 0x%x\n", __FUNCTION__,
		klog_ram->data_checksum, klog_ram_data_csum_compute());
#endif
	goto Exit;
    }

    rc = 1;

Exit:
    spin_unlock(&klog_ram_spinlock);
    return rc;
}

static void inline klog_ram_reset(void)
{
    spin_lock(&klog_ram_spinlock);
    memset(klog_ram, 0, klog_ram_size);
    klog_ram->magic = KLOG_MAGIC;
    klog_ram->num_reboots = 0;
    klog_ram->buf_size = klog_ram_size - sizeof(klog_ram_info_t);
    klog_ram->header_checksum = klog_ram_header_csum_compute();
    spin_unlock(&klog_ram_spinlock);
}

static int klog_ram_save_cur_buf(klog_storage_data_t *data)
{
    spin_lock(&klog_ram_spinlock);
    if (klog_ram->start > klog_ram->end)
        data->size = klog_ram->end + klog_ram->buf_size - klog_ram->start;
    else
	data->size = klog_ram->end - klog_ram->start;

    data->num_reboots = klog_ram->num_reboots;

    if (!(data->buffer = (char *)__get_free_pages(GFP_KERNEL,
	get_order(data->size))))
    {
	printk(KERN_ERR "klog: Allocation failure. Cannot copy log "
	    "information\n");
	data->status = KLOG_ERROR;
	spin_unlock(&klog_ram_spinlock);
	return -1;
    }

    if (klog_ram->start > klog_ram->end)
    {
	memcpy(data->buffer, klog_ram->data + klog_ram->start,
	    klog_ram->buf_size - klog_ram->start);
	memcpy(data->buffer + klog_ram->buf_size - klog_ram->start,
	    klog_ram->data, klog_ram->end);
    }
    else
	memcpy(data->buffer, klog_ram->data + klog_ram->start, data->size);
    spin_unlock(&klog_ram_spinlock);

    return 0;
}

static void *klog_ram_vmap(unsigned long paddr, size_t size)
{
    unsigned long pfn_start;
    struct page **pages;
    int i, page_count;
    void *vaddr;

    printk("klog: buffer in system RAM, vmapping\n");

    if (offset_in_page(paddr) || size % PAGE_SIZE)
    {
	printk("klog: physical address and size must be page-aligned\n");
	return NULL;
    }

    pfn_start = paddr >> PAGE_SHIFT;
    page_count = size / PAGE_SIZE;

    if (!(pages = kmalloc(sizeof(struct page *) * page_count, GFP_KERNEL)))
    {
	printk("klog: Failed to allocate array for %d pages\n", page_count);
	return NULL;
    }

    for (i = 0; i < page_count; i++)
	pages[i] = pfn_to_page(pfn_start + i);

    vaddr = vmap(pages, page_count, VM_MAP, pgprot_noncached(PAGE_KERNEL));

    kfree(pages);
    return vaddr;
}

static int __init klog_ram_init(void)
{
    spin_lock_init(&klog_ram_spinlock);

    if (!klog_ram_paddr)
    {
	printk(KERN_ERR "klog: RAM buffer undefined!\n");
	/* Ideally, we would just exit and not allow usage of the module. */
	BUG();
    }

    if (!(klog_ram = klog_ram_vmap(klog_ram_paddr, klog_ram_size)))
    {
	printk(KERN_ERR "klog: cannot allocate RAM buffered!\n");
	/* Ideally, we would just exit and not allow usage of the module. */
	BUG();
    }

    printk(KERN_NOTICE "klog: Using RAM buffer at address 0x%p size 0x%x\n",
	klog_ram, klog_ram_size);

    if (!klog_ram_valid())
    {
        printk(KERN_NOTICE "klog: Invalid magic or checksum, reseting all "
	    "information.\n");
	goto Exit;
    }

    if (!klog_ram->has_crashed)
    {
        printk(KERN_NOTICE "klog: Data is valid, no crash information "
	    "available.\n");
	csum_update_field(&klog_ram->num_reboots, klog_ram->num_reboots + 1);
	return 0;
    }

    printk(KERN_NOTICE "klog: Data is valid, found new crash information.\n");

    /* Save last crash information and clear buffer for new data. */
    if (klog_ram_save_cur_buf(&klog_crash))
	goto Exit;
    klog_crash.status = KLOG_CRASH;
    klog_crash.timestamp = klog_ram->timestamp;

Exit:
    klog_ram_reset();
    return 0;
}

static void klog_ram_mark_crash(void)
{
    struct timeval timestamp;

    csum_update_field(&klog_ram->has_crashed, 1);
    do_gettimeofday(&timestamp);
    csum_update_field_custom_size((u8*)&klog_ram->timestamp,
	(u8*)&timestamp, sizeof(klog_ram->timestamp));
    printk(KERN_NOTICE "klog: Marked crash in ram buffer!!!\n");
}

static void klog_ram_clean(void)
{
    klog_ram_reset();
    printk(KERN_NOTICE "klog: Cleaned ram buffer!!!\n");
}

static inline void csum_update_field(unsigned int *field, unsigned int value)
{
    klog_ram->header_checksum -= csum_partial((u8*)field, sizeof(unsigned int),
	0);
    *field = value;
    klog_ram->header_checksum += csum_partial((u8*)field, sizeof(unsigned int),
	0);
}
static inline void csum_update_field_custom_size(u8 *field,
    u8 *value, unsigned int sizeof_field)
{
    klog_ram->header_checksum -= csum_partial(field, sizeof_field, 0);
    memcpy(field, value, sizeof_field);
    klog_ram->header_checksum += csum_partial(field, sizeof_field, 0);
}

static inline void csum_update_buffer(char *dst, const char *src,
    unsigned int length)
{
    klog_ram->header_checksum -= csum_partial((u8*)&klog_ram->data_checksum,
	sizeof(unsigned int), 0);
    klog_ram->data_checksum -= csum_partial(dst, length, 0);
    memcpy(dst, src, length);
    klog_ram->data_checksum += csum_partial(dst, length, 0);
    klog_ram->header_checksum += csum_partial((u8*)&klog_ram->data_checksum,
	sizeof(unsigned int), 0);
}

static void klog_ram_write(const char *buffer, unsigned int length)
{
    static int in_klog_ram_write = 0;
    
    if (in_klog_ram_write)
	return;

    spin_lock(&klog_ram_spinlock);
    in_klog_ram_write = 1;

    if (length > klog_ram->buf_size)
    {
	buffer += length - klog_ram->buf_size;
	length = klog_ram->buf_size;
    }

    if ((klog_ram->end + length) > klog_ram->buf_size)
    {
	unsigned int len1 = klog_ram->buf_size - klog_ram->end;

        csum_update_buffer(klog_ram->data + klog_ram->end, buffer, len1);
        csum_update_buffer(klog_ram->data, buffer + len1, length - len1);
	
	/* Update start/end */
	csum_update_field(&klog_ram->end,
	    klog_ram->end + length - klog_ram->buf_size);
	csum_update_field(&klog_ram->start, klog_ram->end + 1);
    }
    else
    {
        csum_update_buffer(klog_ram->data + klog_ram->end, buffer, length);
	/* Update start/end */
	if (klog_ram->start > klog_ram->end)
	{
	    csum_update_field(&klog_ram->start,
		(klog_ram->end + length + 1)%klog_ram->buf_size);
	}
	csum_update_field(&klog_ram->end,
	    (klog_ram->end + length)%klog_ram->buf_size);
    }
    in_klog_ram_write = 0;
    spin_unlock(&klog_ram_spinlock);
}

static klog_storage_data_t *klog_ram_last_crash_data_get(void)
{
    return &klog_crash;
}

static void klog_ram_buffer_free(int is_crash)
{
    klog_storage_data_t *data;

    data = is_crash ? &klog_crash : &klog_buf;

    free_pages((unsigned long)data->buffer, get_order(data->size));
    data->buffer = NULL;
    data->size = 0;
}

static int klog_ram_save_cur(void)
{
    if (klog_ram_save_cur_buf(&klog_buf))
	return -1;
    do_gettimeofday(&klog_buf.timestamp);
    return 0;
}

static klog_storage_data_t *klog_ram_cur_buf_data_get(void)
{
    return &klog_buf;
}

static klog_storage_ops_t klog_ram_storage_ops = {
    .klog_storage_write = klog_ram_write,
    .klog_storage_mark_crash = klog_ram_mark_crash,
    .klog_storage_save_cur = klog_ram_save_cur,
    .klog_storage_last_crash_data_get = klog_ram_last_crash_data_get,
    .klog_storage_cur_buf_data_get = klog_ram_cur_buf_data_get,
    .klog_storage_clean = klog_ram_clean,
    .klog_storage_buffer_free = klog_ram_buffer_free,
};

int __init klog_ram_storage_init(void)
{
    int rc;

    if (!(rc = klog_ram_init()))
	rc = klog_storage_ops_register(&klog_ram_storage_ops);

    printk(KERN_ALERT "klog RAM store initialization %s.\n", rc ? "failed" :
	"succeeded");

    return rc;
}

static void __exit klog_ram_storage_exit(void)
{
    /* TODO klog_ram_uninit() - free data->buffer */
    if (klog_storage_ops_unregister())
	printk(KERN_ALERT "klog RAM storage uninitialization failed.\n");
}

#ifdef MODULE
module_init(klog_ram_storage_init);
module_exit(klog_ram_storage_exit);
#endif
