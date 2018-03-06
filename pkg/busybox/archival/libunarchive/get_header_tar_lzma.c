/* vi: set sw=4 ts=4: */
/*
 * Small lzma deflate implementation.
 * Copyright (C) 2006  Aurelien Jacobs <aurel@gnuage.org>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

#include "libbb.h"
#include "unarchive.h"

extern int unpack_lzma_stream(int src_fd, int dst_fd);

char get_header_tar_lzma(archive_handle_t *archive_handle)
{
	/* Can't lseek over pipes */
	archive_handle->seek = seek_by_char;

	archive_handle->src_fd = open_transformer(archive_handle->src_fd, unpack_lzma_stream);
	archive_handle->offset = 0;
	while (get_header_tar(archive_handle) == EXIT_SUCCESS)
		continue;

	/* Can only do one file at a time */
	return EXIT_FAILURE;
}
