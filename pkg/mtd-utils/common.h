/****************************************************************************
 *
 * rg/pkg/mtd-utils/common.h
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

#ifndef __MTD_UTILS_COMMON_H__
#define __MTD_UTILS_COMMON_H__

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MIN(a ,b) ((a) < (b) ? (a) : (b))
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

/* Verbose messages */
#define verbose(verbose, fmt, ...) do {                            \
	if (verbose)                                               \
		printf(PROGRAM_NAME ": " fmt "\n", ##__VA_ARGS__); \
} while(0)

/* Normal messages */
#define normsg(fmt, ...) do {                              \
	printf(PROGRAM_NAME ": " fmt "\n", ##__VA_ARGS__); \
} while(0)
#define normsg_cont(fmt, ...) do {                    \
	printf(PROGRAM_NAME ": " fmt, ##__VA_ARGS__); \
} while(0)
#define normsg_cont(fmt, ...) do {                         \
	printf(PROGRAM_NAME ": " fmt, ##__VA_ARGS__);      \
} while(0)

/* Error messages */
#define errmsg(fmt, ...)  ({                                                \
	fprintf(stderr, PROGRAM_NAME ": error!: " fmt "\n", ##__VA_ARGS__); \
	-1;                                                                 \
})

/* System error messages */
#define sys_errmsg(fmt, ...)  ({                                            \
	int _err = errno;                                                   \
	size_t _i;                                                           \
	fprintf(stderr, PROGRAM_NAME ": error!: " fmt "\n", ##__VA_ARGS__); \
	for (_i = 0; _i < sizeof(PROGRAM_NAME) + 1; _i++)                   \
		fprintf(stderr, " ");                                       \
	fprintf(stderr, "error %d (%s)\n", _err, strerror(_err));           \
	-1;                                                                 \
})

/* Warnings */
#define warnmsg(fmt, ...) do {                                                \
	fprintf(stderr, PROGRAM_NAME ": warning!: " fmt "\n", ##__VA_ARGS__); \
} while(0)


#ifdef __cplusplus
}
#endif

#endif
