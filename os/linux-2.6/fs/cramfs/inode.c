/*
 * inode.c
 *
 * Copyright (C) 1999 Linus Torvalds
 * Copyright (C) 2000-2002 Transmeta Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (Version 2) as
 * published by the Free Software Foundation.
 *
 * Compressed ROM filesystem for Linux.
 */

/*
 * These are the VFS interfaces to the compressed ROM filesystem.
 * The actual compression is based on zlib, see the other files.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/blkdev.h>
#include <linux/cramfs_fs.h>
#include <linux/slab.h>
#include <linux/cramfs_fs_sb.h>
#include <linux/buffer_head.h>
#include <linux/vfs.h>
#include <linux/mutex.h>
#include <asm/semaphore.h>

#include <asm/uaccess.h>
#include <linux/proc_fs.h>	/* Necessary because we use the proc fs */

static const struct super_operations cramfs_ops;
static const struct inode_operations cramfs_dir_inode_operations;

#define SB_COMP_METHOD(sb) ((CRAMFS_SB(sb)->flags & \
	CRAMFS_FLAG_COMP_METHOD_MASK) >> CRAMFS_FLAG_COMP_METHOD_SHIFT)

static const struct file_operations cramfs_directory_operations;
static const struct address_space_operations cramfs_aops;

static DEFINE_MUTEX(read_mutex);

/* These two macros may change in future, to provide better st_ino
   semantics. */
#define CRAMINO(x)	((CRAMFS_GET_OFFSET(x) && CRAMFS_GET_SIZE(x))? CRAMFS_GET_OFFSET(x)<<2 : 1)
#define OFFSET(x)	((x)->i_ino)
extern unsigned int cramfs_uncompress_counter;


static int cramfs_iget5_test(struct inode *inode, void *opaque)
{
	struct cramfs_inode *cramfs_inode = opaque;

	if (inode->i_ino != CRAMINO(cramfs_inode))
		return 0; /* does not match */

	if (inode->i_ino != 1)
		return 1;

	/* all empty directories, char, block, pipe, and sock, share inode #1 */

	if ((inode->i_mode != CRAMFS_16(cramfs_inode->mode)) ||
	    (inode->i_gid != cramfs_inode->gid) ||
	    (inode->i_uid != CRAMFS_16(cramfs_inode->uid)))
		return 0; /* does not match */

	if ((S_ISCHR(inode->i_mode) || S_ISBLK(inode->i_mode)) &&
	    (inode->i_rdev != old_decode_dev(CRAMFS_24(cramfs_inode->size))))
		return 0; /* does not match */

	return 1; /* matches */
}

static int cramfs_iget5_set(struct inode *inode, void *opaque)
{
	static struct timespec zerotime = {CRAMFS_TIMESTAMP, 0}; 
	struct cramfs_inode *cramfs_inode = opaque;
	inode->i_mode = CRAMFS_16(cramfs_inode->mode);
	inode->i_uid = CRAMFS_16(cramfs_inode->uid);
	inode->i_size = CRAMFS_24(cramfs_inode->size);
	inode->i_blocks = (CRAMFS_24(cramfs_inode->size) - 1) / 512 + 1;
	inode->i_gid = cramfs_inode->gid;
	/* Struct copy intentional */
	inode->i_mtime = inode->i_atime = inode->i_ctime = zerotime;
	inode->i_ino = CRAMINO(cramfs_inode);
	/* inode->i_nlink is left 1 - arguably wrong for directories,
	   but it's the best we can do without reading the directory
           contents.  1 yields the right result in GNU find, even
	   without -noleaf option. */
	if (S_ISREG(inode->i_mode)) {
		inode->i_fop = &generic_ro_fops;
		inode->i_data.a_ops = &cramfs_aops;
	} else if (S_ISDIR(inode->i_mode)) {
		inode->i_op = &cramfs_dir_inode_operations;
		inode->i_fop = &cramfs_directory_operations;
	} else if (S_ISLNK(inode->i_mode)) {
		inode->i_op = &page_symlink_inode_operations;
		inode->i_data.a_ops = &cramfs_aops;
	} else {
		inode->i_size = 0;
		inode->i_blocks = 0;
		init_special_inode(inode, inode->i_mode,
			old_decode_dev(CRAMFS_24(cramfs_inode->size)));
	}
	return 0;
}

static struct inode *get_cramfs_inode(struct super_block *sb,
				struct cramfs_inode * cramfs_inode)
{
	struct inode *inode = iget5_locked(sb, CRAMINO(cramfs_inode),
					    cramfs_iget5_test, cramfs_iget5_set,
					    cramfs_inode);
	if (inode && (inode->i_state & I_NEW)) {
		unlock_new_inode(inode);
	}
	return inode;
}

/*
 * We have our own block cache: don't fill up the buffer cache
 * with the ROM image, because the way the filesystem is set
 * up the accesses should be fairly regular and cached in the
 * page cache and dentry tree anyway..
 *
 * This also acts as a way to guarantee contiguous areas of up to
 * BLKS_PER_BUF*PAGE_CACHE_SIZE, so that the caller doesn't need to
 * worry about end-of-buffer issues even when decompressing a full
 * page cache.
 */
#define READ_BUFFERS (2)
/* NEXT_BUFFER(): Loop over [0..(READ_BUFFERS-1)]. */
#define NEXT_BUFFER(_ix) ((_ix) ^ 1)

/*
 * BLKS_PER_BUF_SHIFT should be at least 2 to allow for "compressed"
 * data that takes up more space than the original and with unlucky
 * alignment.
 */
#define BLKS_PER_BUF_SHIFT	(2)
#define BLKS_PER_BUF		((1 << BLKS_PER_BUF_SHIFT) * \
    CONFIG_CRAMFS_BLKSZ / PAGE_CACHE_SIZE)
#define BUFFER_SIZE		(BLKS_PER_BUF*PAGE_CACHE_SIZE)

static unsigned char read_buffers[READ_BUFFERS][BUFFER_SIZE];
static unsigned buffer_blocknr[READ_BUFFERS];
static struct super_block * buffer_dev[READ_BUFFERS];
static int next_buffer;

static u32 cramfs_get_blksz(struct super_block *sb)
{
#ifdef CONFIG_CRAMFS_DYN_BLOCKSIZE
    	u32 blksz_shift, blksz;

	blksz_shift = (CRAMFS_SB(sb)->flags & CRAMFS_FLAG_BLKSZ_MASK) >>
	    CRAMFS_FLAG_BLKSZ_SHIFT;
    	if ((blksz = PAGE_CACHE_SIZE << blksz_shift) != CONFIG_CRAMFS_BLKSZ)
	{
	    panic("%s: Incorrect block size %u, should be %d",
		__FUNCTION__, blksz, CONFIG_CRAMFS_BLKSZ);
	}
	return blksz;
#endif
	return PAGE_CACHE_SIZE;
}

static u32 cramfs_get_blk_page_ratio(struct super_block *sb)
{
	return cramfs_get_blksz(sb) >> PAGE_CACHE_SHIFT;
}

/*
 * Returns a pointer to a buffer containing at least LEN bytes of
 * filesystem starting at byte offset OFFSET into the filesystem.
 */
static void *cramfs_read(struct super_block *sb, unsigned int offset, unsigned int len)
{
	struct address_space *mapping = sb->s_bdev->bd_inode->i_mapping;
	struct page *pages[BLKS_PER_BUF];
	unsigned i, blocknr, buffer, unread;
	unsigned long devsize;
	char *data;

	if (!len)
		return NULL;
	blocknr = offset >> PAGE_CACHE_SHIFT;
	offset &= PAGE_CACHE_SIZE - 1;

	/* Check if an existing buffer already has the data.. */
	for (i = 0; i < READ_BUFFERS; i++) {
		unsigned int blk_offset;

		if (buffer_dev[i] != sb)
			continue;
		if (blocknr < buffer_blocknr[i])
			continue;
		blk_offset = (blocknr - buffer_blocknr[i]) << PAGE_CACHE_SHIFT;
		blk_offset += offset;
		if (blk_offset + len > BUFFER_SIZE)
			continue;
		return read_buffers[i] + blk_offset;
	}

	devsize = mapping->host->i_size >> PAGE_CACHE_SHIFT;

	/* Ok, read in BLKS_PER_BUF pages completely first. */
	unread = 0;
	for (i = 0; i < BLKS_PER_BUF; i++) {
		struct page *page = NULL;

		if (blocknr + i < devsize) {
			page = read_mapping_page(mapping, blocknr + i, NULL);
			/* synchronous error? */
			if (IS_ERR(page))
				page = NULL;
		}
		pages[i] = page;
	}

	for (i = 0; i < BLKS_PER_BUF; i++) {
		struct page *page = pages[i];
		if (page) {
			wait_on_page_locked(page);
			if (!PageUptodate(page)) {
				/* asynchronous error */
				page_cache_release(page);
				pages[i] = NULL;
			}
		}
	}

	buffer = next_buffer;
	next_buffer = NEXT_BUFFER(buffer);
	buffer_blocknr[buffer] = blocknr;
	buffer_dev[buffer] = sb;

	data = read_buffers[buffer];
	for (i = 0; i < BLKS_PER_BUF; i++) {
		struct page *page = pages[i];
		if (page) {
			memcpy(data, kmap(page), PAGE_CACHE_SIZE);
			kunmap(page);
			page_cache_release(page);
		} else
			memset(data, 0, PAGE_CACHE_SIZE);
		data += PAGE_CACHE_SIZE;
	}
	return read_buffers[buffer] + offset;
}

static void cramfs_put_super(struct super_block *sb)
{
    cramfs_uncompress_exit(SB_COMP_METHOD(sb));
    if (CRAMFS_SB(sb)->uncomp_buffer)
	kfree(CRAMFS_SB(sb)->uncomp_buffer);

    kfree(sb->s_fs_info);
    sb->s_fs_info = NULL;
}

static int cramfs_remount(struct super_block *sb, int *flags, char *data)
{
	*flags |= MS_RDONLY;
	return 0;
}

static int cramfs_fill_super(struct super_block *sb, void *data, int silent)
{
	int i;
	struct cramfs_super super;
	unsigned long root_offset;
	struct cramfs_sb_info *sbi;
	struct inode *root;

	sb->s_flags |= MS_RDONLY;

	sbi = kzalloc(sizeof(struct cramfs_sb_info), GFP_KERNEL);
	if (!sbi)
		return -ENOMEM;
	sb->s_fs_info = sbi;

	/* Invalidate the read buffers on mount: think disk change.. */
	mutex_lock(&read_mutex);
	for (i = 0; i < READ_BUFFERS; i++)
		buffer_blocknr[i] = -1;

	/* Read the first block and get the superblock from it */
	memcpy(&super, cramfs_read(sb, 0, sizeof(super)), sizeof(super));
	mutex_unlock(&read_mutex);

	/* Do sanity checks on the superblock */
	if (super.magic != CRAMFS_32(CRAMFS_MAGIC)) {
		/* check at 512 byte offset */
		mutex_lock(&read_mutex);
		memcpy(&super, cramfs_read(sb, 512, sizeof(super)), sizeof(super));
		mutex_unlock(&read_mutex);
		if (super.magic != CRAMFS_32(CRAMFS_MAGIC)) {
			if (!silent)
				printk(KERN_ERR "cramfs: wrong magic\n");
			goto out;
		}
	}

	/* flags is reused several times, so swab it once */
	super.flags = CRAMFS_32(super.flags);

	/* get feature flags first */
	if (super.flags & ~CRAMFS_SUPPORTED_FLAGS) {
		printk(KERN_ERR "cramfs: unsupported filesystem features\n");
		goto out;
	}

	/* Check that the root inode is in a sane state */
	if (!S_ISDIR(CRAMFS_16(super.root.mode))) {
		printk(KERN_ERR "cramfs: root is not a directory\n");
		goto out;
	}
	root_offset = CRAMFS_GET_OFFSET(&(super.root)) << 2;
	if (super.flags & CRAMFS_FLAG_FSID_VERSION_2) {
		sbi->size = CRAMFS_32(super.size);
		sbi->blocks = CRAMFS_32(super.fsid.blocks);
		sbi->files = CRAMFS_32(super.fsid.files);
	} else {
		sbi->size =  1 << 28;
		sbi->blocks = 0;
		sbi->files = 0;
	}
	sbi->magic = CRAMFS_MAGIC;
	sbi->flags = super.flags;
	if (root_offset == 0)
		printk(KERN_INFO "cramfs: empty filesystem");
	else if (!(super.flags & CRAMFS_FLAG_SHIFTED_ROOT_OFFSET) &&
		 ((root_offset != sizeof(struct cramfs_super)) &&
		  (root_offset != 512 + sizeof(struct cramfs_super))))
	{
		printk(KERN_ERR "cramfs: bad root offset %lu\n", root_offset);
		goto out;
	}

	/* Set it all up.. */
	sbi->uncomp_buffer = NULL;
	sbi->uncomp_blk_offset = -1;
	if (cramfs_uncompress_init(SB_COMP_METHOD(sb)))
	{
	        printk(KERN_ERR "cramfs: error initializing compression\n");
		goto out;
	}	
	sb->s_op = &cramfs_ops;
	root = get_cramfs_inode(sb, &super.root);
	if (!root)
		goto out;
	sb->s_root = d_alloc_root(root);
	if (!sb->s_root) {
		iput(root);
		goto out;
	}
	return 0;
out:
	kfree(sbi);
	sb->s_fs_info = NULL;
	return -EINVAL;
}

static int cramfs_statfs(struct dentry *dentry, struct kstatfs *buf)
{
	struct super_block *sb = dentry->d_sb;

	buf->f_type = CRAMFS_MAGIC;
	buf->f_bsize = PAGE_CACHE_SIZE;
	buf->f_blocks = CRAMFS_SB(sb)->blocks;
	buf->f_bfree = 0;
	buf->f_bavail = 0;
	buf->f_files = CRAMFS_SB(sb)->files;
	buf->f_ffree = 0;
	buf->f_namelen = CRAMFS_MAXPATHLEN;
	return 0;
}

/*
 * Read a cramfs directory entry.
 */
static int cramfs_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	struct inode *inode = filp->f_path.dentry->d_inode;
	struct super_block *sb = inode->i_sb;
	char *buf;
	unsigned int offset;
	int copied;

	/* Offset within the thing. */
	offset = filp->f_pos;
	if (offset >= inode->i_size)
		return 0;
	/* Directory entries are always 4-byte aligned */
	if (offset & 3)
		return -EINVAL;

	buf = kmalloc(256, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	copied = 0;
	while (offset < inode->i_size) {
		struct cramfs_inode *de;
		unsigned long nextoffset;
		char *name;
		ino_t ino;
		mode_t mode;
		int namelen, error;

		mutex_lock(&read_mutex);
		de = cramfs_read(sb, OFFSET(inode) + offset, sizeof(*de)+256);
		name = (char *)(de+1);

		/*
		 * Namelengths on disk are shifted by two
		 * and the name padded out to 4-byte boundaries
		 * with zeroes.
		 */
		namelen = CRAMFS_GET_NAMELEN(de) << 2;
		memcpy(buf, name, namelen);
		ino = CRAMINO(de);
		mode = CRAMFS_16(de->mode);
		mutex_unlock(&read_mutex);
		nextoffset = offset + sizeof(*de) + namelen;
		for (;;) {
			if (!namelen) {
				kfree(buf);
				return -EIO;
			}
			if (buf[namelen-1])
				break;
			namelen--;
		}
		error = filldir(dirent, buf, namelen, offset, ino, mode >> 12);
		if (error)
			break;

		offset = nextoffset;
		filp->f_pos = offset;
		copied++;
	}
	kfree(buf);
	return 0;
}

/*
 * Lookup and fill in the inode data..
 */
static struct dentry * cramfs_lookup(struct inode *dir, struct dentry *dentry, struct nameidata *nd)
{
	unsigned int offset = 0;
	int sorted;

	mutex_lock(&read_mutex);
	sorted = CRAMFS_SB(dir->i_sb)->flags & CRAMFS_FLAG_SORTED_DIRS;
	while (offset < dir->i_size) {
		struct cramfs_inode *de;
		char *name;
		int namelen, retval;

		de = cramfs_read(dir->i_sb, OFFSET(dir) + offset, sizeof(*de)+256);
		name = (char *)(de+1);

		/* Try to take advantage of sorted directories */
		if (sorted && (dentry->d_name.name[0] < name[0]))
			break;

		namelen = CRAMFS_GET_NAMELEN(de) << 2;
		offset += sizeof(*de) + namelen;

		/* Quick check that the name is roughly the right length */
		if (((dentry->d_name.len + 3) & ~3) != namelen)
			continue;

		for (;;) {
			if (!namelen) {
				mutex_unlock(&read_mutex);
				return ERR_PTR(-EIO);
			}
			if (name[namelen-1])
				break;
			namelen--;
		}
		if (namelen != dentry->d_name.len)
			continue;
		retval = memcmp(dentry->d_name.name, name, namelen);
		if (retval > 0)
			continue;
		if (!retval) {
			struct cramfs_inode entry = *de;
			mutex_unlock(&read_mutex);
			d_add(dentry, get_cramfs_inode(dir->i_sb, &entry));
			return NULL;
		}
		/* else (retval < 0) */
		if (sorted)
			break;
	}
	mutex_unlock(&read_mutex);
	d_add(dentry, NULL);
	return NULL;
}

#ifdef CONFIG_CRAMFS_DYN_BLOCKSIZE
/* first_page_in_block is the index of data's first page in the uncompressed
 * block */
static int cramfs_get_uncomp_data(struct super_block *sb, void *dst, int dstlen,
    int first_page_in_block, int blk_start_offset, int blk_comp_len, void *ra)
{
	int copy_bytes;
	int start = first_page_in_block << PAGE_CACHE_SHIFT;
	u32 blksz = cramfs_get_blksz(sb);
	
	if (!CRAMFS_SB(sb)->uncomp_buffer &&
	        !(CRAMFS_SB(sb)->uncomp_buffer = kmalloc(blksz, GFP_KERNEL)))
	{
		printk(KERN_ERR "Error allocating CRAMFS block cache buffer\n");
		return 0;
	}

	if (blk_start_offset != CRAMFS_SB(sb)->uncomp_blk_offset)
	{
		void *src;

		CRAMFS_SB(sb)->uncomp_blk_offset = blk_start_offset;

		src = cramfs_read(sb, blk_start_offset, blk_comp_len);
		if (SB_COMP_METHOD(sb) == CRAMFS_FLAG_COMP_METHOD_LZMA)
		{
		    /* Some validation scope B75619 debugging */
		    u32 out_len = 0;
		    int i;

		    /* Read length */
		    for (i = 0; i < 4; i++)
			out_len += (u32)( ((u8 *)src)[i+5] ) << (i * 8);

		    if (out_len > blksz)
		    {
			printk("B75619: I catch you: call from %p, "
			    "reading \"%s\".\n", ra, sb->s_id);
		    }
		}

		CRAMFS_SB(sb)->uncomp_blk_data_size = cramfs_uncompress_block(
			CRAMFS_SB(sb)->uncomp_buffer, blksz,
			src,
			blk_comp_len, SB_COMP_METHOD(sb));
	}
	
	copy_bytes = min(CRAMFS_SB(sb)->uncomp_blk_data_size - start, dstlen);
	
	memcpy(dst, CRAMFS_SB(sb)->uncomp_buffer + start, copy_bytes);
	return copy_bytes;
}
#endif

static int cramfs_readpage(struct file *file, struct page * page)
{
	struct inode *inode = page->mapping->host;
	u32 maxpage, bytes_filled;
	void *pgdata;
#ifdef CONFIG_CRAMFS_DYN_BLOCKSIZE
	void *ra = __builtin_return_address(0);
#endif

	/* maxpage is the number of 4K pages in the file */
	maxpage = (inode->i_size + PAGE_CACHE_SIZE - 1) >> PAGE_CACHE_SHIFT;
	bytes_filled = 0;
	if (page->index < maxpage) {
		struct super_block *sb = inode->i_sb;
		u32 ratio = cramfs_get_blk_page_ratio(sb);
		u32 page_block = page->index / ratio;
		u32 blkptr_offset = OFFSET(inode) + page_block * 4;
		u32 start_offset, compr_len, maxblock;

		/* maxblock is the number of blocks in the file */
		maxblock = (maxpage + ratio - 1) / ratio;

		/* start_offset is the offset of the requested page's
		 * compressed block from the beginning of the cramfs image.
		 * At the beginning of the file there is an array of "block
		 * pointers" and then the compressed blocks themselves.
		 * A "block pointer" means the offset of the compressed block's
		 * end from the beginning of cramfs image.
		 * So, start_offset of block n is calculated in the following
		 * way:
		 * - If n == 0 then immediatly after the array of block
		 *   pointers.
		 * - Else, the end of block n-1 taken from the array of block
		 *   pointers. */
		
		start_offset = OFFSET(inode) + maxblock*4;
		mutex_lock(&read_mutex);
		if (page_block)
			start_offset = CRAMFS_32(*(u32 *) cramfs_read(sb, blkptr_offset-4, 4));

		/* Compressed length is block n's end minus block n-1's end */
		compr_len = CRAMFS_32(*(u32 *) cramfs_read(sb, blkptr_offset, 4)) - start_offset;
		mutex_unlock(&read_mutex);
		pgdata = kmap(page);
		if (compr_len == 0)
			; /* hole */
#ifndef CONFIG_CRAMFS_DYN_BLOCKSIZE
		else if (compr_len > (PAGE_CACHE_SIZE << 1))
			printk(KERN_ERR "cramfs: bad compressed blocksize %u\n", compr_len);
#endif
		else {
			mutex_lock(&read_mutex);
#ifdef CONFIG_CRAMFS_DYN_BLOCKSIZE
				bytes_filled = cramfs_get_uncomp_data(sb,
				        pgdata,
					PAGE_CACHE_SIZE,
					page->index - page_block * ratio,
					start_offset, compr_len, ra);
#else
				bytes_filled = cramfs_uncompress_block(pgdata,
					 PAGE_CACHE_SIZE,
					 cramfs_read(sb, start_offset,
					 compr_len),
					 compr_len, SB_COMP_METHOD(sb));
#endif
			mutex_unlock(&read_mutex);
		}
	} else
		pgdata = kmap(page);
	memset(pgdata + bytes_filled, 0, PAGE_CACHE_SIZE - bytes_filled);
	kunmap(page);
	flush_dcache_page(page);
	SetPageUptodate(page);
	unlock_page(page);
	return 0;
}

static const struct address_space_operations cramfs_aops = {
	.readpage = cramfs_readpage
};

/*
 * Our operations:
 */

/*
 * A directory can only readdir
 */
static const struct file_operations cramfs_directory_operations = {
	.llseek		= generic_file_llseek,
	.read		= generic_read_dir,
	.readdir	= cramfs_readdir,
};

static const struct inode_operations cramfs_dir_inode_operations = {
	.lookup		= cramfs_lookup,
};

static const struct super_operations cramfs_ops = {
	.put_super	= cramfs_put_super,
	.remount_fs	= cramfs_remount,
	.statfs		= cramfs_statfs,
};

static int cramfs_get_sb(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *data, struct vfsmount *mnt)
{
	return get_sb_bdev(fs_type, flags, dev_name, data, cramfs_fill_super,
			   mnt);
}

static struct file_system_type cramfs_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "cramfs",
	.get_sb		= cramfs_get_sb,
	.kill_sb	= kill_block_super,
	.fs_flags	= FS_REQUIRES_DEV,
};

#ifdef CONFIG_RG_CRAMFS_MONITOR
static int procfile_read(char *buffer, char **buffer_location, off_t offset,
    int buffer_length, int *eof, void *data)
{
    int ret;

    if (offset > 0) 
    {
	/* we have finished to read, return 0 */
	ret = 0;
    } 
    else 
    {
	/* fill the buffer, return the buffer size */
	ret = sprintf(buffer, "%d\n", cramfs_uncompress_counter);
    }

    return ret;
}

static void init_cramfs_pageins_monitoring(void)
{
    cramfs_uncompress_counter = 0;

    if (create_proc_read_entry(CONFIG_RG_CRAMFS_MONITOR_FILE, S_IFREG|S_IRUGO,
	NULL, procfile_read, NULL))
    {
	printk(KERN_INFO "/proc/%s created\n", CONFIG_RG_CRAMFS_MONITOR_FILE);
    }
    else
    {
	printk(KERN_ALERT "Error: Could not initialize /proc/%s\n",
	    CONFIG_RG_CRAMFS_MONITOR_FILE);
    }
}

static void exit_cramfs_pageins_monitoring(void)
{
    remove_proc_entry(CONFIG_RG_CRAMFS_MONITOR_FILE, &proc_root);
    printk(KERN_INFO "/proc/%s removed\n", CONFIG_RG_CRAMFS_MONITOR_FILE);
}
#else
 #define init_cramfs_pageins_monitoring()
 #define exit_cramfs_pageins_monitoring()
#endif 

static int __init init_cramfs_fs(void)
{
    init_cramfs_pageins_monitoring();

    return register_filesystem(&cramfs_fs_type);
}

static void __exit exit_cramfs_fs(void)
{
    exit_cramfs_pageins_monitoring();
    unregister_filesystem(&cramfs_fs_type);
}

module_init(init_cramfs_fs)
module_exit(exit_cramfs_fs)
MODULE_LICENSE("GPL");
