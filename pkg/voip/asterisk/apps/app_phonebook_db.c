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
 * \brief Phonebook database utility
 * 
 * \par See also
 * \arg \ref Config_vm
 * \ingroup applications
 */

/*
 * 12-16-2004 : Support for Greek added by InAccess Networks (work funded by HOL, www.hol.gr)
 *				 George Konstantoulakis <gkon@inaccessnetworks.com>
 *
 * 05-10-2005 : Support for Swedish and Norwegian added by Daniel Nylander, http://www.danielnylander.se/
 *
 * 05-11-2005 : An option for maximum number of messsages per mailbox added by GDS Partners (www.gdspartners.com)
 * 07-11-2005 : An issue with voicemail synchronization has been fixed by GDS Partners (www.gdspartners.com)
 *				 Stojan Sljivic <stojan.sljivic@gdspartners.com>
 *
 */

#include "asterisk/channel.h"
#include "asterisk/module.h"
#include "asterisk/pbx.h"
#include "asterisk/cli.h"
#include "asterisk.h"
#include "asterisk/app.h"
#include <voip/phonebook/pb_db_utils.h>

ASTERISK_FILE_VERSION(__FILE__, "$Revision: 1.1.4.1 $")
#define OP_NUMBER_IS_CONTACT "NUMBER_IS_CONTACT"
#define PHONEBOOK_DB_APP "PhonebookDBUtil"

static char *tdesc = "Phonebook database utility";
static char *pb_app = PHONEBOOK_DB_APP;
static char *pb_synopsis = "Receive info from phonebook database";
static char *pb_desc = PHONEBOOK_DB_APP "(operation|output_variable_name|[param]):\n"
" Operations:\n"
"	" OP_NUMBER_IS_CONTACT " - check whether a phone number belongs to a "
"					contact\n";

static int phone_is_contact(struct ast_channel *chan, int argc,
    char *argv[])
{
    char *phone, *dest;

    if (argc != 3)
	return -1;

    /* Directory path must be seeded. Perhaps find an elegant solution? */
    pb_get_directory(ast_config_AST_SPOOL_DIR);
    dest = argv[1];
    phone = argv[2];
    if (pb_contact_is_exist_by_ph_number(phone))
	pbx_builtin_setvar_helper(chan, dest, "true");
    else
	pbx_builtin_setvar_helper(chan, dest, "false");

    return 0;
}

static int pb_exec(struct ast_channel *chan, void *data)
{
    char tmp[256], *argv[4], *op_name;
    int argc, rc = 0;

    if (ast_strlen_zero(data))
    {
	ast_log(LOG_WARNING, PHONEBOOK_DB_APP " requires arguments\n");
	return -1;
    }

    ast_copy_string(tmp, data, sizeof(tmp));
    argc = ast_app_separate_args(tmp, '|', argv, sizeof(argv) /
	sizeof(argv[0]));
    op_name = argv[0];

    if (!strncasecmp(op_name, OP_NUMBER_IS_CONTACT, strlen(op_name)))
	rc = phone_is_contact(chan, argc, argv);
    else
    {
	ast_log(LOG_WARNING, "Unsupported operation '%s' for "
	    PHONEBOOK_DB_APP "\n", op_name);
	rc = -2;
    }

    if (rc == -1)
    {
	ast_log(LOG_WARNING, "Wrong number of arguments for operation "
	    "'%s'\n", op_name);
    }

    return rc;
}

int unload_module(void)
{
    int rc;

    rc = ast_unregister_application(pb_app);
    return rc;
}

int load_module(void)
{
    int ret;

    ret = ast_register_application(pb_app, pb_exec, pb_synopsis, pb_desc);

    return 0;
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
