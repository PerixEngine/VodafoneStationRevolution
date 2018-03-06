/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 1999 - 2005, Digium, Inc.
 *
 * Mark Spencer <markster@digium.com>
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
 * \brief Trivial application to read a variable
 * 
 * \ingroup applications
 */
 
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "asterisk.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision: 1.5.4.3 $")

#include "asterisk/lock.h"
#include "asterisk/file.h"
#include "asterisk/logger.h"
#include "asterisk/channel.h"
#include "asterisk/pbx.h"
#include "asterisk/app.h"
#include "asterisk/module.h"
#include "asterisk/translate.h"
#include "asterisk/options.h"
#include "asterisk/utils.h"

#define READ_USAGE "Read(variable[|filename][|maxdigits][|option][|attempts]" \
    "[|timeout][|esc_codes][|esc_timeout][|break_keys])"

#define READSTATUS_VAR "READSTATUS"
#define READSTATUS_ESCAPE "ESCAPE_CODE"
#define READSTATUS_ENDER "ENDER_CODE"

static char *tdesc = "Read Variable Application";

static char *app = "Read";

static char *synopsis = "Read a variable";

static char *descrip = 
"  " READ_USAGE "\n\n"
"Reads a #-terminated string of digits a certain number of times from the\n"
"user in to the given variable.\n"
"  filename   -- file to play before reading digits.\n"
"  maxdigits  -- maximum acceptable number of digits. Stops reading after\n"
"                maxdigits have been entered (without requiring the user to\n"
"                press the '#' key).\n"
"                Defaults to 0 - no limit - wait for the user press the '#' key.\n"
"                Any value below 0 means the same. Max accepted value is 255.\n"
"  option     -- may be 'skip' to return immediately if the line is not up,\n"
"                or 'noanswer' to read digits even if the line is not up.\n"
"  attempts   -- if greater than 1, that many attempts will be made in the \n"
"                event no data is entered.\n"
"  timeout    -- if greater than 0, that value will override the default timeout.\n"
"  esc_codes  -- if one of codes is typed, timeout will be changed to the esc_timeout or 0 by default. Example: 112&911.\n"
"                This argument allows changing of the timeout if specific sequence of keys was entered.\n"
"  esc_timeout-- timeout for escape codes. Default is 0, immediate escape.\n"
"  break_keys -- if the user presses one of this keys (for example: *), the function returns this key immediately.\n"
"                The difference from esc_codes:\n"
"                \t- When pressing one of a break key is detected the function exits immediately.\n"
"                \t- Break key is a single key but not a sequence like esc_codes.\n\n"
"This application sets the following variable upon its successful completion:\n"
"  " READSTATUS_VAR " -- The reason for completion:\n"
"      " READSTATUS_ESCAPE "	-- escape code was dialed\n"
"      " READSTATUS_ENDER "\t -- ender code was dialed"
"Read should disconnect if the function fails or errors out.\n";

STANDARD_LOCAL_USER;

LOCAL_USER_DECL;

#define ast_next_data(instr,ptr,delim) if((ptr=strchr(instr,delim))) { *(ptr) = '\0' ; ptr++;}

static int read_exec(struct ast_channel *chan, void *data)
{
	int res = 0;
	struct localuser *u;
	char tmp[256];
	char *timeout = NULL;
	char *varname = NULL;
	char *filename = NULL;
	char *loops;
	char *maxdigitstr=NULL;
	char *options=NULL;
	char *escape_code = NULL;
	char *escape_timeout_str = NULL;
	char *break_keys = NULL;
	int option_skip = 0;
	int option_noanswer = 0;
	int maxdigits=255;
	int tries = 1;
	int to = 0;
	int x = 0;
	int escape_timeout = 0;
	char *argcopy = NULL;
	char *args[11];

	if (ast_strlen_zero(data)) {
		ast_log(LOG_WARNING, "Read requires an argument (variable)\n");
		return -1;
	}

	LOCAL_USER_ADD(u);
	
	argcopy = ast_strdupa(data);
	if (!argcopy) {
		ast_log(LOG_ERROR, "Out of memory\n");
		LOCAL_USER_REMOVE(u);
		return -1;
	}

	if (ast_app_separate_args(argcopy, '|', args, sizeof(args) / sizeof(args[0])) < 1) {
		ast_log(LOG_WARNING, "Cannot Parse Arguments.\n");
		LOCAL_USER_REMOVE(u);
		return -1;
	}

	varname = args[x++];
	filename = args[x++];
	maxdigitstr = args[x++];
	options = args[x++];
	loops = args[x++];
	timeout = args[x++];
	escape_code = args[x++];
	escape_timeout_str = args[x++];
	break_keys = args[x++];
	
	if (options) { 
		if (!strcasecmp(options, "skip"))
			option_skip = 1;
		else if (!strcasecmp(options, "noanswer"))
			option_noanswer = 1;
		else {
			if (strchr(options, 's'))
				option_skip = 1;
			if (strchr(options, 'n'))
				option_noanswer = 1;
		}
	}

	if(loops) {
		tries = atoi(loops);
		if(tries <= 0)
			tries = 1;
	}

	if(timeout) {
		to = atoi(timeout);
		if (to <= 0)
			to = 0;
		else
			to *= 1000;
	}

	if (ast_strlen_zero(filename)) 
		filename = NULL;
	if (maxdigitstr) {
		maxdigits = atoi(maxdigitstr);
		if ((maxdigits<1) || (maxdigits>255)) {
    			maxdigits = 255;
		} else if (option_verbose > 2)
			ast_verbose(VERBOSE_PREFIX_3 "Accepting a maximum of %d digits.\n", maxdigits);
	}
	if (ast_strlen_zero(varname)) {
		ast_log(LOG_WARNING, "Invalid! Usage: " READ_USAGE "\n\n");
		LOCAL_USER_REMOVE(u);
		return -1;
	}

	if (ast_strlen_zero(escape_code)) 
		escape_code = NULL;

	if (escape_code && escape_timeout_str)
	{
	        escape_timeout = atoi(escape_timeout_str);
		escape_timeout = escape_timeout > 0 ? escape_timeout * 1000 : 0; 
	}
	
	if (ast_strlen_zero(break_keys)) 
		break_keys = NULL;

	if (chan->_state != AST_STATE_UP) {
		if (option_skip) {
			/* At the user's option, skip if the line is not up */
			pbx_builtin_setvar_helper(chan, varname, "\0");
			LOCAL_USER_REMOVE(u);
			return 0;
		} else if (!option_noanswer) {
			/* Otherwise answer unless we're supposed to read while on-hook */
			res = ast_answer(chan);
		}
	}
	if (!res) {
		while(tries && !res) {
			ast_stopstream(chan);
			res = ast_app_getdata_allow_escape(chan, filename, tmp, maxdigits, to, escape_code, escape_timeout, break_keys);
			if (res > -1) {
				pbx_builtin_setvar_helper(chan, varname, tmp);
				if (res == 3)
				    pbx_builtin_setvar_helper(chan, READSTATUS_VAR, READSTATUS_ESCAPE);
				else if (res == 0)
				    pbx_builtin_setvar_helper(chan, READSTATUS_VAR, READSTATUS_ENDER);
				if (!ast_strlen_zero(tmp)) {
					if (option_verbose > 2)
						ast_verbose(VERBOSE_PREFIX_3 "User entered '%s'\n", tmp);
					tries = 0;
				} else {
					tries--;
					if (option_verbose > 2) {
						if (tries)
							ast_verbose(VERBOSE_PREFIX_3 "User entered nothing, %d chance%s left\n", tries, (tries != 1) ? "s" : "");
						else
							ast_verbose(VERBOSE_PREFIX_3 "User entered nothing.\n");
					}
				}
				res = 0;
			} else {
				if (option_verbose > 2)
					ast_verbose(VERBOSE_PREFIX_3 "User disconnected\n");
			}
		}
	}
	LOCAL_USER_REMOVE(u);
	return res;
}

int unload_module(void)
{
	int res;

	res = ast_unregister_application(app);
	
	STANDARD_HANGUP_LOCALUSERS;

	return res;	
}

int load_module(void)
{
	return ast_register_application(app, read_exec, synopsis, descrip);
}

char *description(void)
{
	return tdesc;
}

int usecount(void)
{
	int res;
	STANDARD_USECOUNT(res);
	return res;
}

char *key()
{
	return ASTERISK_GPL_KEY;
}
