/****************************************************************************
 *
 * rg/pkg/mtd-utils/nandoob.c
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

#include <rg_config.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mtd/mtd-user.h>

#include "common.h"

/* Most of the code extracted from os/linux-2.6/drivers/mtd/nand/nand_base.c
 */

#define NAND_MAX_OOBSIZE	64

#define PROGRAM_VERSION "1.0"
#define PROGRAM_NAME    "nandoob"
static const char *doc = PROGRAM_NAME " version " PROGRAM_VERSION
    " - a tool to calculate ECC with OOB start value (0xFFFFFFFF)";

/* Currently we only support 64 bytes long OOB. */    
#define OOB_SIZE (64)
static uint32_t eccpos_64[OOB_SIZE] = {
    40, 41, 42, 43, 44, 45, 46, 47,
    48, 49, 50, 51, 52, 53, 54, 55,
    56, 57, 58, 59, 60, 61, 62, 63};


static int parse_opt(int argc, char *const argv[]);
static int nand_write_page_swecc(int fd, const uint8_t *buf);

struct args {
	char *f_in;
	char *f_out;
	int out_fd;
	int in_fd;
	int verbose;
	int page_size;
};

static struct args args;

int main(int argc, char *const argv[])
{
    int err, rd = -1;
    unsigned char *buf;

    err = parse_opt(argc, argv);
    if (err)
	return -1;

    if (!(buf = malloc(args.page_size)))
    {
	errmsg("failed memory allocation");
	goto Error_fd;
    }

    printf("Calculating ECC for \'%s\', output to \'%s\'... ", args.f_in,
	args.f_out);
    while ((rd = read(args.in_fd, buf, args.page_size)))
    {
	if (nand_write_page_swecc(args.out_fd, buf))
	{
	    errmsg("failed writing page to %s", args.f_out);
	    goto Error_buf;
	}
    }
    printf("Done\n");

    if (rd)
	errmsg("error in input file - not full page was read");
    else
	verbose(args.verbose, "created %s", args.f_out);

Error_buf:
    free(buf);
Error_fd:
    close(args.out_fd);
    close(args.in_fd);

    return err;
}

static const char *optionsstr =
"-o, --output=<file name>     output file name\n"
"-p, --page-size=<bytes>      size of the physical eraseblock of the flash\n"
"                             this image is created for in bytes\n"
"-m, --mode=soft              this is the default, ECC size=256, ECC bytes=3"
"-v, --verbose                be verbose\n"
"-h, --help                   print help message\n"
"-V, --version                print program version";

static const char *usage =
"Usage: " PROGRAM_NAME " [-o filename] [--page-size=<bytes>] [-h] [--help]\n"
"\t\t[-v] [--version] [--mode=soft] <input NAND file to create OOB headers>\n\n"
"Example " PROGRAM_NAME "-o ubi_oob.img -p 2048 ubi.img" ;

static const struct option long_options[] = {
	{ .name = "output",    .has_arg = 1, .flag = NULL, .val = 'o' },
	{ .name = "verbose",   .has_arg = 0, .flag = NULL, .val = 'v' },
	{ .name = "help",      .has_arg = 0, .flag = NULL, .val = 'h' },
	{ .name = "version",   .has_arg = 0, .flag = NULL, .val = 'V' },
	{ .name = "page-size", .has_arg = 1, .flag = NULL, .val = 'p' },
	{ .name = "ecc-mode",  .has_arg = 1, .flag = NULL, .val = 'm' },
	{ NULL, 0, NULL, 0},
};

/**
 * get_multiplier - convert size specifier to an integer multiplier.
 * @str: the size specifier string
 *
 * This function parses the @str size specifier, which may be one of
 * 'KiB', 'MiB', or 'GiB' into an integer multiplier. Returns positive
 * size multiplier in case of success and %-1 in case of failure.
 */
static int get_multiplier(const char *str)
{
	if (!str)
		return 1;

	/* Remove spaces before the specifier */
	while (*str == ' ' || *str == '\t')
		str += 1;

	if (!strcmp(str, "KiB"))
		return 1024;
	if (!strcmp(str, "MiB"))
		return 1024 * 1024;
	if (!strcmp(str, "GiB"))
		return 1024 * 1024 * 1024;

	return -1;
}

/**
 * get_bytes - convert a string containing amount of bytes into an
 *             integer.
 * @str: string to convert
 *
 * This function parses @str which may have one of 'KiB', 'MiB', or 'GiB' size
 * specifiers. Returns positive amount of bytes in case of success and %-1 in
 * case of failure.
 */
static long long get_bytes(const char *str)
{
	char *endp;
	long long bytes = strtoull(str, &endp, 0);

	if (endp == str || bytes < 0)
		return errmsg("incorrect amount of bytes: \"%s\"", str);

	if (*endp != '\0') {
		int mult = get_multiplier(endp);

		if (mult == -1)
			return errmsg("bad size specifier: \"%s\" - "
				       "should be 'KiB', 'MiB' or 'GiB'", endp);
		bytes *= mult;
	}

	return bytes;
}

static int parse_opt(int argc, char *const argv[])
{
	while (1) {
		int key;

		key = getopt_long(argc, argv, "o:p:m:vhV", long_options, NULL);
		if (key == -1)
			break;

		switch (key) {
		case 'h':
			fprintf(stderr, "%s\n\n", doc);
			fprintf(stderr, "%s\n\n", usage);
			fprintf(stderr, "%s\n", optionsstr);
			exit(EXIT_SUCCESS);

		case 'V':
			fprintf(stderr, "%s\n", PROGRAM_VERSION);
			exit(EXIT_SUCCESS);

		case 'm':
			if (strcmp(optarg, "soft"))
			    return errmsg("only software mode is supported");
			break;

		case 'o':
			args.out_fd = open(optarg, O_CREAT | O_TRUNC | O_WRONLY,
					   S_IWUSR | S_IRUSR | S_IRGRP | S_IWGRP | S_IROTH);
			if (args.out_fd == -1)
				return sys_errmsg("cannot open file \"%s\"", optarg);
			args.f_out = optarg;
			break;
		case 'p':
			args.page_size = get_bytes(optarg);
			if (args.page_size <= 0  || args.page_size > 10000)
			    return errmsg("bad page size: \"%s\"", optarg);
			break;
			
		case 'v':
			args.verbose = 1;
			break;

		case ':':
			return errmsg("parameter is missing");

		default:
			fprintf(stderr, "Use -h for help\n");
			return -1;
		}
	}
	
	if (optind == argc)
		return errmsg("input file was not specified (use -h for help)");

	if (optind != argc - 1)
		return errmsg("more then one input file was specified (use -h for help)");

	args.in_fd = open(argv[optind], O_RDONLY);
	args.f_in = argv[optind];
	if (args.in_fd == -1)
	    return sys_errmsg("cannot open file \"%s\"", argv[optind]);

	if (!args.f_out)
	    return errmsg("output file was not specified (use -h for help)");

	if (!args.page_size)
	    return errmsg("page size was not specified (use -h for help)");

	return 0;
}

int nand_calculate_ecc(const u_char *dat, u_char *ecc_code);
static int nand_write_page_raw(int fb, const uint8_t *buf, uint8_t *oob_poi);
static int nand_write_page_swecc(int fd, const uint8_t *buf)
{
	int i, eccsize = 256;
	int eccbytes = 3;
	int eccsteps = 8;
	int ecctotal = eccsteps * eccbytes;
	uint8_t ecc_calc[NAND_MAX_OOBSIZE];
	const uint8_t *p = buf;
	static uint8_t oob_poi[OOB_SIZE] = {
	    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	};

	/* Software ecc calculation */
	for (i = 0; eccsteps; eccsteps--, i += eccbytes, p += eccsize)
		nand_calculate_ecc(p, &ecc_calc[i]);

	for (i = 0; i < ecctotal; i++)
		oob_poi[eccpos_64[i]] = ecc_calc[i];

	return nand_write_page_raw(fd, buf, oob_poi);
}

/*
 * Pre-calculated 256-way 1 byte column parity
 */
static const u_char nand_ecc_precalc_table[] = {
	0x00, 0x55, 0x56, 0x03, 0x59, 0x0c, 0x0f, 0x5a, 0x5a, 0x0f, 0x0c, 0x59, 0x03, 0x56, 0x55, 0x00,
	0x65, 0x30, 0x33, 0x66, 0x3c, 0x69, 0x6a, 0x3f, 0x3f, 0x6a, 0x69, 0x3c, 0x66, 0x33, 0x30, 0x65,
	0x66, 0x33, 0x30, 0x65, 0x3f, 0x6a, 0x69, 0x3c, 0x3c, 0x69, 0x6a, 0x3f, 0x65, 0x30, 0x33, 0x66,
	0x03, 0x56, 0x55, 0x00, 0x5a, 0x0f, 0x0c, 0x59, 0x59, 0x0c, 0x0f, 0x5a, 0x00, 0x55, 0x56, 0x03,
	0x69, 0x3c, 0x3f, 0x6a, 0x30, 0x65, 0x66, 0x33, 0x33, 0x66, 0x65, 0x30, 0x6a, 0x3f, 0x3c, 0x69,
	0x0c, 0x59, 0x5a, 0x0f, 0x55, 0x00, 0x03, 0x56, 0x56, 0x03, 0x00, 0x55, 0x0f, 0x5a, 0x59, 0x0c,
	0x0f, 0x5a, 0x59, 0x0c, 0x56, 0x03, 0x00, 0x55, 0x55, 0x00, 0x03, 0x56, 0x0c, 0x59, 0x5a, 0x0f,
	0x6a, 0x3f, 0x3c, 0x69, 0x33, 0x66, 0x65, 0x30, 0x30, 0x65, 0x66, 0x33, 0x69, 0x3c, 0x3f, 0x6a,
	0x6a, 0x3f, 0x3c, 0x69, 0x33, 0x66, 0x65, 0x30, 0x30, 0x65, 0x66, 0x33, 0x69, 0x3c, 0x3f, 0x6a,
	0x0f, 0x5a, 0x59, 0x0c, 0x56, 0x03, 0x00, 0x55, 0x55, 0x00, 0x03, 0x56, 0x0c, 0x59, 0x5a, 0x0f,
	0x0c, 0x59, 0x5a, 0x0f, 0x55, 0x00, 0x03, 0x56, 0x56, 0x03, 0x00, 0x55, 0x0f, 0x5a, 0x59, 0x0c,
	0x69, 0x3c, 0x3f, 0x6a, 0x30, 0x65, 0x66, 0x33, 0x33, 0x66, 0x65, 0x30, 0x6a, 0x3f, 0x3c, 0x69,
	0x03, 0x56, 0x55, 0x00, 0x5a, 0x0f, 0x0c, 0x59, 0x59, 0x0c, 0x0f, 0x5a, 0x00, 0x55, 0x56, 0x03,
	0x66, 0x33, 0x30, 0x65, 0x3f, 0x6a, 0x69, 0x3c, 0x3c, 0x69, 0x6a, 0x3f, 0x65, 0x30, 0x33, 0x66,
	0x65, 0x30, 0x33, 0x66, 0x3c, 0x69, 0x6a, 0x3f, 0x3f, 0x6a, 0x69, 0x3c, 0x66, 0x33, 0x30, 0x65,
	0x00, 0x55, 0x56, 0x03, 0x59, 0x0c, 0x0f, 0x5a, 0x5a, 0x0f, 0x0c, 0x59, 0x03, 0x56, 0x55, 0x00
};

/**
 * nand_calculate_ecc - [NAND Interface] Calculate 3-byte ECC for 256-byte block
 * @dat:	raw data
 * @ecc_code:	buffer for ECC
 */
int nand_calculate_ecc(const u_char *dat, u_char *ecc_code)
{
	uint8_t idx, reg1, reg2, reg3, tmp1, tmp2;
	int i;

	/* Initialize variables */
	reg1 = reg2 = reg3 = 0;

	/* Build up column parity */
	for(i = 0; i < 256; i++) {
		/* Get CP0 - CP5 from table */
		idx = nand_ecc_precalc_table[*dat++];
		reg1 ^= (idx & 0x3f);

		/* All bit XOR = 1 ? */
		if (idx & 0x40) {
			reg3 ^= (uint8_t) i;
			reg2 ^= ~((uint8_t) i);
		}
	}

	/* Create non-inverted ECC code from line parity */
	tmp1  = (reg3 & 0x80) >> 0; /* B7 -> B7 */
	tmp1 |= (reg2 & 0x80) >> 1; /* B7 -> B6 */
	tmp1 |= (reg3 & 0x40) >> 1; /* B6 -> B5 */
	tmp1 |= (reg2 & 0x40) >> 2; /* B6 -> B4 */
	tmp1 |= (reg3 & 0x20) >> 2; /* B5 -> B3 */
	tmp1 |= (reg2 & 0x20) >> 3; /* B5 -> B2 */
	tmp1 |= (reg3 & 0x10) >> 3; /* B4 -> B1 */
	tmp1 |= (reg2 & 0x10) >> 4; /* B4 -> B0 */

	tmp2  = (reg3 & 0x08) << 4; /* B3 -> B7 */
	tmp2 |= (reg2 & 0x08) << 3; /* B3 -> B6 */
	tmp2 |= (reg3 & 0x04) << 3; /* B2 -> B5 */
	tmp2 |= (reg2 & 0x04) << 2; /* B2 -> B4 */
	tmp2 |= (reg3 & 0x02) << 2; /* B1 -> B3 */
	tmp2 |= (reg2 & 0x02) << 1; /* B1 -> B2 */
	tmp2 |= (reg3 & 0x01) << 1; /* B0 -> B1 */
	tmp2 |= (reg2 & 0x01) << 0; /* B7 -> B0 */

	/* Calculate final ECC code */
#ifdef CONFIG_MTD_NAND_ECC_SMC
	ecc_code[0] = ~tmp2;
	ecc_code[1] = ~tmp1;
#else
	ecc_code[0] = ~tmp1;
	ecc_code[1] = ~tmp2;
#endif
	ecc_code[2] = ((~reg1) << 2) | 0x03;

	return 0;
}

static int nand_write_page_raw(int fb, const uint8_t *buf, uint8_t *oob_poi)
{
    if (args.page_size != write(fb, buf, args.page_size))
	return errmsg("failed to write page\n");
    if (OOB_SIZE != write(fb, oob_poi, OOB_SIZE))
	return errmsg("failed to write oob data\n");
    return 0;
}
