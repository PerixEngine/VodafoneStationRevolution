/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2005, Andriy Pylypenko
 * Code based on format_wav.c by Mark Spencer
 *
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

/*! 
 * \file
 *
 * \brief Work with Sun Microsystems AU format.
 *
 * signed linear
 * \arg File extension: au
 * \ingroup formats
 */
 
#include <stdlib.h>
#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "asterisk.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision: 1.6.2.4 $")

#include "asterisk/lock.h"
#include "asterisk/channel.h"
#include "asterisk/file.h"
#include "asterisk/logger.h"
#include "asterisk/sched.h"
#include "asterisk/module.h"
#include "asterisk/endian.h"

#define BUF_SIZE		160

#define AU_HEADER_SIZE		24
#define AU_HEADER(var)		u_int32_t var[6]

#define AU_HDR_MAGIC_OFF	0
#define AU_HDR_HDR_SIZE_OFF	1
#define AU_HDR_DATA_SIZE_OFF	2
#define AU_HDR_ENCODING_OFF	3
#define AU_HDR_SAMPLE_RATE_OFF	4
#define AU_HDR_CHANNELS_OFF	5

#define AU_ENC_8BIT_ULAW	1
#define AU_ENC_8BIT_ALAW	27

/* Project specific flag
 *
 * Must be larger then BUF_SIZE and AU_HEADER_SIZE
 * Should be multiple of CONFIG_HW_NAND_MIN_IO_SIZE
 * or -1 to precache whole file
 * or 0 for working in legacy mode */
#define AU_CACHE_SIZE		(2048 * 5)

struct ast_filestream {
	void *reserved[AST_RESERVED_POINTERS];
	/* This is what a filestream means to us */
	FILE *f; 				/* Descriptor */
	struct ast_channel *owner;
	struct ast_frame fr;			/* Frame information */
	char waste[AST_FRIENDLY_OFFSET];	/* Buffer for sending frames, etc */
	int empty;
	short buf[BUF_SIZE];
	char *cache_buf;
	int file_size;
	int cache_data_sz;
	int cache_cur_pnt;
	int reload_flag;
	int reload_offset;
};


AST_MUTEX_DEFINE_STATIC(au_lock);
static int localusecnt = 0;

static char *name_ulaw = "au_ulaw";
static char *name_alaw = "au_alaw";
static char *desc = "Sun Microsystems AU format (signed linear)";

/* This trick (concatinating ulaw/alaw) allows supporting of alaw/ulaw co-existence for au file extension
 * */
static char *exts_ulaw = "ulaw.au";
static char *exts_alaw = "alaw.au";


#define AU_MAGIC 0x2e736e64
#if __BYTE_ORDER == __BIG_ENDIAN
#define htoll(b) (b)
#define htols(b) (b)
#define ltohl(b) (b)
#define ltohs(b) (b)
#else
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define htoll(b)  \
          (((((b)      ) & 0xFF) << 24) | \
	       ((((b) >>  8) & 0xFF) << 16) | \
		   ((((b) >> 16) & 0xFF) <<  8) | \
		   ((((b) >> 24) & 0xFF)      ))
#define htols(b) \
          (((((b)      ) & 0xFF) << 8) | \
		   ((((b) >> 8) & 0xFF)      ))
#define ltohl(b) htoll(b)
#define ltohs(b) htols(b)
#else
#error "Endianess not defined"
#endif
#endif


static int check_header(struct ast_filestream *fs, int fmt)
{
	AU_HEADER(header);
	u_int32_t magic;
	u_int32_t hdr_size;
	u_int32_t data_size;
	u_int32_t encoding;
	u_int32_t sample_rate;
	u_int32_t channels;

	if (!AU_CACHE_SIZE) {
		if (fread(header, 1, AU_HEADER_SIZE, fs->f) != AU_HEADER_SIZE) {
			ast_log(LOG_WARNING, "Read failed (header)\n");
			return -1;
		}
	}
	else
		memcpy(header, fs->cache_buf, AU_HEADER_SIZE);

	magic = ltohl(header[AU_HDR_MAGIC_OFF]);
	if (magic != (u_int32_t) AU_MAGIC) {
		ast_log(LOG_WARNING, "Bad magic: 0x%x\n", magic);
	}
	hdr_size = ltohl(header[AU_HDR_HDR_SIZE_OFF]);
	if (hdr_size < AU_HEADER_SIZE)
		hdr_size = AU_HEADER_SIZE;
/*	data_size = ltohl(header[AU_HDR_DATA_SIZE_OFF]); */
	encoding = ltohl(header[AU_HDR_ENCODING_OFF]);
	if (fmt == AST_FORMAT_ULAW && encoding != AU_ENC_8BIT_ULAW) {
		ast_log(LOG_WARNING, "Unexpected format: %d. Expected 8bit ULAW (%d)\n", encoding, AU_ENC_8BIT_ULAW);
		return -1;
	}
	if (fmt == AST_FORMAT_ALAW && encoding != AU_ENC_8BIT_ALAW) {
		ast_log(LOG_WARNING, "Unexpected format: %d. Expected 8bit ALAW (%d)\n", encoding, AU_ENC_8BIT_ALAW);
		return -1;
	}
	sample_rate = ltohl(header[AU_HDR_SAMPLE_RATE_OFF]);
	if (sample_rate != 8000) {
		ast_log(LOG_WARNING, "Sample rate can only be 8000 not %d\n", sample_rate);
		return -1;
	}
	channels = ltohl(header[AU_HDR_CHANNELS_OFF]);
	if (channels != 1) {
		ast_log(LOG_WARNING, "Not in mono: channels=%d\n", channels);
		return -1;
	}
	/* Skip to data */
	data_size = fs->file_size - hdr_size;
	fs->cache_cur_pnt = hdr_size;

	if (fseek(fs->f, hdr_size, SEEK_SET) == -1 ) {
		ast_log(LOG_WARNING, "Failed to skip to data: %d\n", hdr_size);
		return -1;
	}
	return data_size;
}

static int update_header(FILE *f)
{
	off_t cur, end;
	u_int32_t datalen;
	int bytes;

	cur = ftell(f);
	fseek(f, 0, SEEK_END);
	end = ftell(f);
	/* data starts 24 bytes in */
	bytes = end - AU_HEADER_SIZE;
	datalen = htoll(bytes);

	if (cur < 0) {
		ast_log(LOG_WARNING, "Unable to find our position\n");
		return -1;
	}
	if (fseek(f, AU_HDR_DATA_SIZE_OFF * sizeof(u_int32_t), SEEK_SET)) {
		ast_log(LOG_WARNING, "Unable to set our position\n");
		return -1;
	}
	if (fwrite(&datalen, 1, sizeof(datalen), f) != sizeof(datalen)) {
		ast_log(LOG_WARNING, "Unable to set write file size\n");
		return -1;
	}
	if (fseek(f, cur, SEEK_SET)) {
		ast_log(LOG_WARNING, "Unable to return to position\n");
		return -1;
	}
	return 0;
}

static int write_header(FILE *f, int fmt)
{
	AU_HEADER(header);

	header[AU_HDR_MAGIC_OFF] = htoll((u_int32_t) AU_MAGIC);
	header[AU_HDR_HDR_SIZE_OFF] = htoll(AU_HEADER_SIZE);
	header[AU_HDR_DATA_SIZE_OFF] = 0;
	header[AU_HDR_ENCODING_OFF] = htoll(fmt == AST_FORMAT_ULAW ? AU_ENC_8BIT_ULAW : AU_ENC_8BIT_ALAW);
	header[AU_HDR_SAMPLE_RATE_OFF] = htoll(8000);
	header[AU_HDR_CHANNELS_OFF] = htoll(1);

	/* Write an au header, ignoring sizes which will be filled in later */
	fseek(f, 0, SEEK_SET);
	if (fwrite(header, 1, AU_HEADER_SIZE, f) != AU_HEADER_SIZE) {
		ast_log(LOG_WARNING, "Unable to write header\n");
		return -1;
	}
	return 0;
}

static struct ast_filestream *au_open(FILE *f, int fmt)
{
	struct ast_filestream *tmp;
	char *name = NULL;

	if (!(tmp = malloc(sizeof(struct ast_filestream)))) {
		ast_log(LOG_ERROR, "Out of memory\n");
		return NULL;
	}

	memset(tmp, 0, sizeof(struct ast_filestream));

	tmp->f = f;
	fseek(f, 0, SEEK_END);
	tmp->file_size = ftell(f);
	fseek(f, 0, SEEK_SET);

	if (AU_CACHE_SIZE) {
		tmp->cache_data_sz = AU_CACHE_SIZE > 0 ? AU_CACHE_SIZE : tmp->file_size ;

		if (!(tmp->cache_buf = malloc(tmp->cache_data_sz))) {
			free(tmp);
			return NULL;
		}

		tmp->cache_data_sz = fread(tmp->cache_buf, 1, tmp->cache_data_sz, f);

		if ((AU_CACHE_SIZE < 0 && tmp->cache_data_sz != tmp->file_size) ||
		    (tmp->cache_data_sz != AU_CACHE_SIZE && tmp->cache_data_sz != tmp->file_size)) {
			free(tmp->cache_buf);
			free(tmp);
			return NULL;
		}
	}

	if (check_header(tmp, fmt) < 0) {
		free(tmp->cache_buf);
		free(tmp);
		return NULL;
	}
	if (ast_mutex_lock(&au_lock)) {
		ast_log(LOG_WARNING, "Unable to lock au count\n");
		free(tmp->cache_buf);
		free(tmp);
		return NULL;
	}

	if (fmt == AST_FORMAT_ULAW)
		name = name_ulaw;
	else if (fmt == AST_FORMAT_ALAW)
		name = name_alaw;
	tmp->fr.data = tmp->buf;
	tmp->fr.frametype = AST_FRAME_VOICE;
	tmp->fr.subclass = fmt;
	/* datalen will vary for each frame */
	tmp->fr.src = name;
	tmp->fr.mallocd = 0;
	localusecnt++;
	ast_mutex_unlock(&au_lock);
	ast_update_use_count();
	return tmp;
}

static struct ast_filestream *au_alaw_open(FILE *f)
{
	return au_open(f, AST_FORMAT_ALAW);
}

static struct ast_filestream *au_ulaw_open(FILE *f)
{
	return au_open(f, AST_FORMAT_ULAW);
}

static struct ast_filestream *au_rewrite(FILE *f, const char *comment, int fmt)
{
	struct ast_filestream *tmp;

	if ((tmp = malloc(sizeof(struct ast_filestream))) == NULL) {
		ast_log(LOG_ERROR, "Out of memory\n");
		return NULL;
	}

	memset(tmp, 0, sizeof(struct ast_filestream));
	if (write_header(f, fmt)) {
		free(tmp);
		return NULL;
	}
	if (ast_mutex_lock(&au_lock)) {
		ast_log(LOG_WARNING, "Unable to lock au count\n");
		free(tmp);
		return NULL;
	}
	tmp->f = f;
	localusecnt++;
	ast_mutex_unlock(&au_lock);
	ast_update_use_count();
	return tmp;
}

static struct ast_filestream *au_alaw_rewrite(FILE *f, const char *comment)
{
	return au_rewrite(f, comment, AST_FORMAT_ALAW);
}

static struct ast_filestream *au_ulaw_rewrite(FILE *f, const char *comment)
{
	return au_rewrite(f, comment, AST_FORMAT_ULAW);
}

static void au_close(struct ast_filestream *s)
{
	if (ast_mutex_lock(&au_lock)) {
		ast_log(LOG_WARNING, "Unable to lock au count\n");
		return;
	}
	localusecnt--;
	ast_mutex_unlock(&au_lock);
	ast_update_use_count();
	fclose(s->f);
	free(s->cache_buf);
	free(s);
}

static struct ast_frame *au_read(struct ast_filestream *s, int *whennext, 
	int fmt)
{
	int res;
	int delay;
	/* Send a frame from the file to the appropriate channel */

	s->fr.frametype = AST_FRAME_VOICE;
	s->fr.subclass = fmt;
	s->fr.offset = AST_FRIENDLY_OFFSET;
	s->fr.mallocd = 0;
	s->fr.data = s->buf;


	if (!AU_CACHE_SIZE) {
		if ((res = fread(s->buf, 1, BUF_SIZE, s->f)) < 1) {
			if (res)
				ast_log(LOG_WARNING, "Short read (%d) (%s)!\n", res, strerror(errno));
			return NULL;
		}
	}
	else if (AU_CACHE_SIZE > 0) { 
		if (s->reload_flag) {
#if AU_CACHE_SIZE != 0
			fseek(s->f, (s->reload_offset / AU_CACHE_SIZE) * AU_CACHE_SIZE, SEEK_SET);
#endif
			if ((res = fread(s->cache_buf, 1, AU_CACHE_SIZE, s->f)) < 1) {
				if (res)
					ast_log(LOG_WARNING, "Short read (%d) (%s)!\n", res, strerror(errno));
				return NULL;
			}
			fseek(s->f, s->reload_offset, SEEK_SET);
			s->reload_flag = 0;
		}

		if (s->cache_cur_pnt + BUF_SIZE > AU_CACHE_SIZE) {
			if ((res = fread(s->cache_buf, 1, AU_CACHE_SIZE, s->f)) < 1) {
				if (res)
					ast_log(LOG_WARNING, "Short read (%d) (%s)!\n", res, strerror(errno));
				return NULL;
			}

			s->cache_data_sz = res;
			s->cache_cur_pnt = 0;
			fseek(s->f, -res, SEEK_CUR);
		}
	}

	if (AU_CACHE_SIZE) {
		/* Reached end of file */
		if ((res = s->cache_data_sz - s->cache_cur_pnt) <= 0)
			return NULL;

		if (res > BUF_SIZE)
			res = BUF_SIZE;

		memcpy(s->buf, &s->cache_buf[s->cache_cur_pnt], res);
		s->cache_cur_pnt += res;
		fseek(s->f, res, SEEK_CUR);
	}
	s->fr.samples = res;
	s->fr.datalen = res;
	delay = s->fr.samples;
	*whennext = delay;
	return &s->fr;
}

static struct ast_frame *au_alaw_read(struct ast_filestream *s, int *whennext)
{
	return au_read(s, whennext, AST_FORMAT_ALAW);
}

static struct ast_frame *au_ulaw_read(struct ast_filestream *s, int *whennext)
{
	return au_read(s, whennext, AST_FORMAT_ULAW);
}

static int au_write(struct ast_filestream *fs, struct ast_frame *f, int fmt)
{
	int res;

	if (f->frametype != AST_FRAME_VOICE) {
		ast_log(LOG_WARNING, "Asked to write non-voice frame!\n");
		return -1;
	}
	if (f->subclass != fmt) {
		ast_log(LOG_WARNING, "Asked to write non-%s frame (%d)!\n",
			fmt == AST_FORMAT_ULAW ? "ulaw" : "alaw", f->subclass);
		return -1;
	}
	if ((res = fwrite(f->data, 1, f->datalen, fs->f)) != f->datalen) {
			ast_log(LOG_WARNING, "Bad write (%d/%d): %s\n", res, f->datalen, strerror(errno));
			return -1;
	}
	update_header(fs->f);
	return 0;
}

static int au_alaw_write(struct ast_filestream *fs, struct ast_frame *f)
{
	return au_write(fs, f, AST_FORMAT_ALAW);
}

static int au_ulaw_write(struct ast_filestream *fs, struct ast_frame *f)
{
	return au_write(fs, f, AST_FORMAT_ULAW);
}

static int au_seek(struct ast_filestream *fs, long sample_offset, int whence)
{
	off_t min, max, cur;
	long offset = 0, samples;
	
	samples = sample_offset;
	min = AU_HEADER_SIZE;
	cur = ftell(fs->f);
	fseek(fs->f, 0, SEEK_END);
	max = ftell(fs->f);
	if (whence == SEEK_SET)
		offset = samples + min;
	else if (whence == SEEK_CUR || whence == SEEK_FORCECUR)
		offset = samples + cur;
	else if (whence == SEEK_END)
		offset = max - samples;
        if (whence != SEEK_FORCECUR) {
		offset = (offset > max) ? max : offset;
	}
	/* always protect the header space. */
	offset = (offset < min) ? min : offset;

	if (AU_CACHE_SIZE < 0)
		fs->cache_cur_pnt = offset;
	else if (AU_CACHE_SIZE > 0) {
#if AU_CACHE_SIZE != 0
		fs->cache_cur_pnt = offset % AU_CACHE_SIZE;
#endif
		fs->reload_offset = offset;
		fs->reload_flag = 1;
	}

	return fseek(fs->f, offset, SEEK_SET);
}

static int au_trunc(struct ast_filestream *fs)
{
	if (ftruncate(fileno(fs->f), ftell(fs->f)))
		return -1;
	return update_header(fs->f);
}

static long au_tell(struct ast_filestream *fs)
{
	off_t offset;

	offset = ftell(fs->f);
	return offset - AU_HEADER_SIZE;
}

static char *au_getcomment(struct ast_filestream *s)
{
	return NULL;
}

int load_module()
{
	if (ast_format_register(name_ulaw, exts_ulaw, AST_FORMAT_ULAW,
				au_ulaw_open,
				au_ulaw_rewrite,
				au_ulaw_write,
				au_seek,
				au_trunc,
				au_tell,
				au_ulaw_read,
				au_close,
				au_getcomment))
	{
		return -1;
	}

	if (ast_format_register(name_alaw, exts_alaw, AST_FORMAT_ALAW,
				au_alaw_open,
				au_alaw_rewrite,
				au_alaw_write,
				au_seek,
				au_trunc,
				au_tell,
				au_alaw_read,
				au_close,
				au_getcomment))
	{
		unload_module();
		return -1;
	}

	return 0;
}

int unload_module()
{
	return ast_format_unregister(name_ulaw) |
		ast_format_unregister(name_alaw);
}

int usecount()
{
	return localusecnt;
}

char *description()
{
	return desc;
}

char *key()
{
	return ASTERISK_GPL_KEY;
}
