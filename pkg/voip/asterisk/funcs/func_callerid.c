/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 1999 - 2005, Digium, Inc.
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

/*! \file
 *
 * \brief Caller ID related dialplan functions
 * 
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "asterisk.h"

/* ASTERISK_FILE_VERSION(__FILE__, "$Revision: 1.2.14.3 $") */

#ifndef BUILTIN_FUNC
#include "asterisk/module.h"
#endif /* BUILTIN_FUNC */
#include "asterisk/channel.h"
#include "asterisk/pbx.h"
#include "asterisk/logger.h"
#include "asterisk/utils.h"
#include "asterisk/app.h"
#include "asterisk/options.h"
#include "asterisk/callerid.h"

static char *callerid_read(struct ast_channel *chan, char *cmd, char *data, char *buf, size_t len) 
{
	if (!strncasecmp("all", data, 3)) {
		snprintf(buf, len, "\"%s\" <%s>", chan->cid.cid_name ? chan->cid.cid_name : "", chan->cid.cid_num ? chan->cid.cid_num : "");	
	} else if (!strncasecmp("name", data, 4)) {
		if (chan->cid.cid_name) {
			ast_copy_string(buf, chan->cid.cid_name, len);
		}
	} else if (!strncasecmp("num", data, 3) || !strncasecmp("number", data, 6)) {
		if (chan->cid.cid_num) {
			ast_copy_string(buf, chan->cid.cid_num, len);
		}
	} else if (!strncasecmp("ani", data, 3)) {
		if (chan->cid.cid_ani) {
			ast_copy_string(buf, chan->cid.cid_ani, len);
		}
	} else if (!strncasecmp("dnid", data, 4)) {
		if (chan->cid.cid_dnid) {
			ast_copy_string(buf, chan->cid.cid_dnid, len);
		}
	} else if (!strncasecmp("rdnis", data, 5)) {
		if (chan->cid.cid_rdnis) {
			ast_copy_string(buf, chan->cid.cid_rdnis, len);
		}
	} else {
		ast_log(LOG_ERROR, "Unknown callerid data type.\n");
	}

	return buf;
}

static void callerid_write(struct ast_channel *chan, char *cmd, char *data, const char *value) 
{
	if (!value)
                return;
	
	if (!strncasecmp("all", data, 3)) {
		char name[256];
		char num[256];
		if (!ast_callerid_split(value, name, sizeof(name), num, sizeof(num)))
			ast_set_callerid(chan, num, name, num);	
        } else if (!strncasecmp("name", data, 4)) {
                ast_set_callerid(chan, NULL, value, NULL);
        } else if (!strncasecmp("num", data, 3) || !strncasecmp("number", data, 6)) {
                ast_set_callerid(chan, value, NULL, NULL);
        } else if (!strncasecmp("ani", data, 3)) {
                ast_set_callerid(chan, NULL, NULL, value);
        } else if (!strncasecmp("dnid", data, 4)) {
                /* do we need to lock chan here? */
                if (chan->cid.cid_dnid)
                        free(chan->cid.cid_dnid);
                chan->cid.cid_dnid = ast_strlen_zero(value) ? NULL : strdup(value);
        } else if (!strncasecmp("rdnis", data, 5)) {
                /* do we need to lock chan here? */
                if (chan->cid.cid_rdnis)
                        free(chan->cid.cid_rdnis);
                chan->cid.cid_rdnis = ast_strlen_zero(value) ? NULL : strdup(value);
        } else {
                ast_log(LOG_ERROR, "Unknown callerid data type.\n");
        }
}

#ifndef BUILTIN_FUNC
static
#endif /* BUILTIN_FUNC */
struct ast_custom_function callerid_function = {
	.name = "CALLERID",
	.synopsis = "Gets or sets Caller*ID data on the channel.",
	.syntax = "CALLERID(datatype)",
	.desc = "Gets or sets Caller*ID data on the channel.  The allowable datatypes\n"
	"are \"all\", \"name\", \"num\", \"ANI\", \"DNID\", \"RDNIS\".\n",
	.read = callerid_read,
	.write = callerid_write,
};

/* Function taken from main/enum.c from Asterisk 1.8 */
static int cclen(char *number)
{
        int cc;
	char digits[3] = "";

	if (!number || (strlen(number) < 3)) {
	        return 0;
	}

	strncpy(digits, number, 2);

	if (!sscanf(digits, "%30d", &cc)) {
	        return 0;
	}

	if (cc / 10 == 1 || cc / 10 == 7)
	        return 1;

        if (cc == 20 || cc == 27 || (cc >= 30 && cc <= 34) || cc == 36 ||
	    cc == 39 || cc == 40 || cc == 41 || (cc >= 40 && cc <= 41) ||
	    (cc >= 43 && cc <= 49) || (cc >= 51 && cc <= 58) ||
	    (cc >= 60 && cc <= 66) || cc == 81 || cc == 82 || cc == 84 ||
	    cc == 86 || (cc >= 90 && cc <= 95) || cc == 98) {
	        return 2;
	}

        return 3;
}

/* Returns the country code portion of a given international number,
 * otherwise returns 0.
 *
 * Use: ${COUNTRY_CODE(num)}
 * Example: $[${COUNTRY_CODE(+49123456789)} = 49] */
static char *countrycode(struct ast_channel *chan, char *cmd, char *data, char *buf, size_t len)
{
        char *def_ret = "0";
        int start;
        int cc_len;

        /* Precaution. */
        if (strlen(data) < 3)
	        return def_ret;

	if (!strncmp(data, "+00", 3)) /* Vodafone DE specific requirement */
	        start = 3;
	else if (!strncmp(data, "00", 2))
	        start = 2;
	else if (!strncmp(data, "+", 1))
	        start = 1;
	else 
	        return def_ret;
    
	if (!(cc_len = cclen(&data[start])) || len <= cc_len)
	        return def_ret;

	ast_copy_string(buf, &data[start], cc_len + 1);

	return buf;
}

#ifndef BUILTIN_FUNC
static
#endif /* BUILTIN_FUNC */
struct ast_custom_function countrycode_function = {
	.name = "COUNTRY_CODE",
	.synopsis = "Returns the country code portion of an international number.",
	.syntax = "COUNTRY_CODE(num)",
	.read = countrycode,
};

#ifndef BUILTIN_FUNC
static char *tdesc = "Caller ID related dialplan functions";

int unload_module(void)
{
        ast_custom_function_unregister(&countrycode_function);
        return ast_custom_function_unregister(&callerid_function);
}

int load_module(void)
{
        ast_custom_function_register(&countrycode_function);
        return ast_custom_function_register(&callerid_function);
}

char *description(void)
{
	return tdesc;
}

int usecount(void)
{
	return 0;
}

char *key()
{
	return ASTERISK_GPL_KEY;
}
#endif /* BUILTIN_FUNC */

/*
Local Variables:
mode: C
c-file-style: "linux"
indent-tabs-mode: nil
End:
*/
