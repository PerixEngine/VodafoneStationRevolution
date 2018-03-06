/*
 * This file is part of UBIFS.
 *
 * Copyright (C) 2006-2008 Nokia Corporation
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Authors: Artem Bityutskiy
 *          Adrian Hunter
 */

#include "ubifs.h"
#include <linux/version.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23))
#define BYTES_PER_LINE 32

/**
 * ubifs_hexdump - dump a buffer.
 * @ptr: the buffer to dump
 * @size: buffer size which must be multiple of 4 bytes
 */
void ubifs_hexdump(const void *ptr, int size)
{
	int i, k = 0, rows, columns;
	const uint8_t *p = ptr;

	rows = size / BYTES_PER_LINE + size % BYTES_PER_LINE;
	for (i = 0; i < rows; i++) {
		int j;

		cond_resched();
		columns = min(size - k, BYTES_PER_LINE) / 4;
		if (columns == 0)
			break;
		printk(KERN_DEBUG "%5d:  ", i * BYTES_PER_LINE);
		for (j = 0; j < columns; j++) {
			int n, N;

			N = size - k > 4 ? 4 : size - k;
			for (n = 0; n < N; n++)
				printk("%02x", p[k++]);
			printk(" ");
		}
		printk("\n");
	}
}
#endif /* LINUX_VERSION_CODE < 2.6.23 */

#ifdef UBIFS_COMPAT_USE_OLD_PREPARE_WRITE
int ubifs_prepare_write(struct file *file, struct page *page, unsigned from,
			unsigned to)
{
	struct inode *inode = page->mapping->host;
	struct ubifs_info *c = inode->i_sb->s_fs_info;
	loff_t pos = ((loff_t)page->index << PAGE_CACHE_SHIFT) + to;
	struct ubifs_budget_req req;
	int err;

	ubifs_assert(!(inode->i_sb->s_flags & MS_RDONLY));

	if (unlikely(c->ro_media))
		return -EROFS;

	if (!PageUptodate(page)) {
		/*
		 * The page is not loaded from the flash and has to be loaded
		 * unless we are writing all of it.
		 */
		if (from == 0 && to == PAGE_CACHE_SIZE)
			/*
			 * Set the PG_checked flag to make the further code
			 * allocate full budget, because we do not know whether
			 * the page exists on the flash media or not.
			 */
			SetPageChecked(page);
		else {
			err = do_readpage(page);
			if (err)
				return err;
		}

		SetPageUptodate(page);
		ClearPageError(page);
	}

	memset(&req, 0, sizeof(struct ubifs_budget_req));
	if (!PagePrivate(page)) {
		/*
		 * If the PG_Checked flag is set, the page corresponds to a
		 * hole or to a place beyond the inode. In this case we have to
		 * budget for a new page, otherwise for a dirtied page.
		 */
		if (PageChecked(page))
			req.new_page = 1;
		else
			req.dirtied_page = 1;
	} else
		req.locked_pg = 1;

	if (pos > inode->i_size)
		/*
		 * We are writing beyond the file which means we are going to
		 * change inode size and make the inode dirty. And in turn,
		 * this means we have to budget for making the inode dirty.
		 */
		req.dirtied_ino = 1;

	err = ubifs_budget_space(c, &req);
	return err;
}

int ubifs_commit_write(struct file *file, struct page *page, unsigned from,
		       unsigned to)
{
	struct inode *inode = page->mapping->host;
	struct ubifs_inode *ui = ubifs_inode(inode);
	struct ubifs_info *c = inode->i_sb->s_fs_info;
	loff_t pos = ((loff_t)page->index << PAGE_CACHE_SHIFT) + to;

	dbg_gen("ino %lu, pg %lu, offs %lld-%lld (in pg: %u-%u, %u bytes) "
		"flags %#lx", inode->i_ino, page->index, pos - to + from,
		pos, from, to, to - from, page->flags);
	ubifs_assert(PageUptodate(page));
	ubifs_assert(mutex_is_locked(&inode->i_mutex));

	if (!PagePrivate(page)) {
		SetPagePrivate(page);
		atomic_long_inc(&c->dirty_pg_cnt);
		__set_page_dirty_nobuffers(page);
	}

	if (pos > inode->i_size) {
		int release;

		mutex_lock(&ui->ui_mutex);
		i_size_write(inode, pos);
		ui->ui_size = pos;
		release = ui->dirty;
		/*
		 * Note, we do not set @I_DIRTY_PAGES (which means that the
		 * inode has dirty pages), this has been done in
		 * '__set_page_dirty_nobuffers()'.
		 */
		__mark_inode_dirty(inode, I_DIRTY_DATASYNC);
		mutex_unlock(&ui->ui_mutex);

		/*
		 * We've marked the inode as dirty and we have allocated budget
		 * for this. However, the inode may had already be be dirty
		 * before, in which case we have to free the budget.
		 */
		if (release)
			ubifs_release_dirty_inode_budget(c, ui);
	}

	return 0;
}

#include <linux/fs.h>
#include <linux/writeback.h>

#define MAX_MKSPC_RETRIES 3

#define NR_TO_WRITE 16

static int shrink_liability(struct ubifs_info *c, int nr_to_write,
			    int locked_pg)
{
	struct writeback_control wbc = {
		.sync_mode   = WB_SYNC_NONE,
		.range_end   = LLONG_MAX,
		.nr_to_write = nr_to_write,
		.skip_locked_pages = locked_pg,
	};

	generic_sync_sb_inodes(c->vfs_sb, &wbc);
	dbg_budg("%ld pages were written back", nr_to_write - wbc.nr_to_write);
	return nr_to_write - wbc.nr_to_write;
}

static int run_gc(struct ubifs_info *c)
{
	int err, lnum;

	/* Make some free space by garbage-collecting dirty space */
	down_read(&c->commit_sem);
	lnum = ubifs_garbage_collect(c, 1);
	up_read(&c->commit_sem);
	if (lnum < 0)
		return lnum;

	/* GC freed one LEB, return it to lprops */
	dbg_budg("GC freed LEB %d", lnum);
	err = ubifs_return_leb(c, lnum);
	if (err)
		return err;

	return 0;
}

extern long long get_liability(struct ubifs_info *c);

int ubifs_make_free_space(struct ubifs_info *c, int locked_pg)
{
	int err, retries = 0;
	long long liab1, liab2;

	do {
		liab1 = get_liability(c);
		/*
		 * We probably have some dirty pages or inodes (liability), try
		 * to write them back.
		 */
		dbg_budg("liability %lld, run write-back", liab1);
		shrink_liability(c, NR_TO_WRITE, locked_pg);

		liab2 = get_liability(c);
		if (liab2 < liab1)
			return -EAGAIN;

		dbg_budg("new liability %lld (not shrinked)", liab2);

		/* Liability did not shrink again, try GC */
		dbg_budg("Run GC");
		err = run_gc(c);
		if (!err)
			return -EAGAIN;

		if (err != -EAGAIN && err != -ENOSPC)
			/* Some real error happened */
			return err;

		dbg_budg("Run commit (retries %d)", retries);
		err = ubifs_run_commit(c);
		if (err)
			return err;
	} while (retries++ < MAX_MKSPC_RETRIES);

	return -ENOSPC;
}

#endif /* UBIFS_COMPAT_USE_OLD_PREPARE_WRITE */
