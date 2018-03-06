/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 1999 - 2005, Digium, Inc.
 *
 * Matt O'Gorman <mogorman@digium.com>
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
 * \brief TR application -- performs SetParams and GetParams to the OpenRG
 *
 * \ingroup applications
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <dslhome/ipc/client.h>

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include "asterisk.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision: 1.1.4.4 $")

#include "asterisk/logger.h"
#include "asterisk/channel.h"
#include "asterisk/pbx.h"
#include "asterisk/app.h"
#include "asterisk/module.h"

static char *tdesc = "executes SetParams and GetParams, stores return value into variable";

static char *app_get = "CwmpGet";
static char *app_set = "CwmpSet";
static char *app_factory_reset = "CwmpFactoryReset";

static char *get_synopsis = "CwmpGet(varname=tr)";
static char *set_synopsis = "CwmpSet(tr=<value>)";
static char *factory_reset_synopsis = "CwmpFactoryReset()";

static char *get_descrip =
"CwmpGet(varname=tr)\n"
"  Varname - Variable name to store result in.\n"
"  TR - The name of the TR to query.\n";

static char *set_descrip =
"CwmpSet(tr=<value>)\n"
"  TR - The name of the TR to query.\n"
"  Value - value to set.\n"
"  This application sets the following channel variables upon completion:\n"
"    CWMPSTATUS   - This is the status of the set action\n"
;

static char *factory_reset_descrip =
"CwmpFactoryReset()\n"
"  This application sets the following channel variables upon completion:\n"
"    CWMPSTATUS   - This is the status of the action\n"
;

AST_MUTEX_DEFINE_STATIC(libxml_lock);

STANDARD_LOCAL_USER;

LOCAL_USER_DECL;

#define MAX_REQUEST_SIZE 512
#define REQUEST_GENERIC_GET "<GetParameterValues xmlns=\"\"><ParameterNames><string>%s</string></ParameterNames></GetParameterValues>"

const char *setStringParameterValues = "<SetParameterValues xmlns=\"\"><ParameterList><ParameterValueStruct SOAP-ENC:arrayType=\"cwmp:ParameterInfoStruct[1]\"><Name>%s</Name><Value xsi:type=\"xsd:string\">%s</Value></ParameterValueStruct></ParameterList></SetParameterValues>";

const char *factoryReset = "<FactoryReset xmlns=\"\">/</FactoryReset>";

char *buildRequest(const char *request, ...)
{
	va_list argptr;
	static char dst[MAX_REQUEST_SIZE];

	va_start(argptr, request);
	vsnprintf(dst, MAX_REQUEST_SIZE, request, argptr);
	va_end(argptr);

	return dst;
}

static int parseValue(const char *result, char *retval, int retvallen) {
	xmlDocPtr doc;
	xmlXPathContextPtr xpathCtx;
	xmlXPathObjectPtr xpathObj;
	int rc = -1;

	ast_mutex_lock(&libxml_lock);
	memset(retval, 0, retvallen);
	ast_log(LOG_DEBUG, "result: %s\n", result);

	doc = xmlReadMemory(result, strlen(result), NULL, NULL, XML_PARSE_NOERROR|XML_PARSE_NOWARNING);

	xpathCtx = xmlXPathNewContext(doc);
	xpathObj = xmlXPathEvalExpression((xmlChar *)"/GetParameterValuesResponse/ParameterList/ParameterValueStruct/Value", xpathCtx);

	if(xpathObj) {
		/* we have a node */
		if( xpathObj->nodesetval && xpathObj->nodesetval->nodeNr>0) {
        		xmlChar *xmlContent =
			    xmlNodeGetContent(xpathObj->nodesetval->nodeTab[0]);

			snprintf(retval, retvallen, "%s", (char *)xmlContent);
			rc = 0;
			xmlFree(xmlContent);
		}
	}

	xmlXPathFreeObject(xpathObj);
	xmlXPathFreeContext(xpathCtx);
	xmlFreeDoc(doc);
	free((void *)result);
	ast_mutex_unlock(&libxml_lock);

	return rc;
}

static int get_exec(struct ast_channel *chan, void *data)
{
	struct localuser *u;
	char *s, *varname=NULL, *request=NULL;
	char *result;
	int response_len;
	char buffer[128] = "";

	if (ast_strlen_zero(data)) {
		ast_log(LOG_WARNING, "get require an argument!\n");
		return -1;
	}

	LOCAL_USER_ADD(u);

	s = ast_strdupa(data);
	if (!s)
	{
		ast_log(LOG_ERROR, "Out of memory\n");
		LOCAL_USER_REMOVE(u);
		return -1;
	}

	varname = strsep(&s, "=");
	request = s;

	result = cwmp_ipc_run_request(buildRequest(REQUEST_GENERIC_GET,
		request), &response_len);
	if (result)
	{
		parseValue(result, buffer, 128);
		pbx_builtin_setvar_helper(chan, varname, buffer);
	}

	LOCAL_USER_REMOVE(u);
	return 0;
}

static int set_exec(struct ast_channel *chan, void *data)
{
	struct localuser *u;
	char *s, *tr_name=NULL, *value=NULL;
	char *result, *status = "FAILURE";
	int response_len;

	if (ast_strlen_zero(data)) {
		ast_log(LOG_WARNING, "set require an argument!\n");
		return -1;
	}

	LOCAL_USER_ADD(u);

	s = ast_strdupa(data);
	if (!s)
	{
		ast_log(LOG_ERROR, "Out of memory\n");
		LOCAL_USER_REMOVE(u);
		return -1;
	}

	tr_name = strsep(&s, "=");
	value = s;

	result = cwmp_ipc_run_request(buildRequest(setStringParameterValues,
	    tr_name, value), &response_len);
	if (result)
	{
		if (!strstr(result, "<cwmp:Fault>"))
		    status = "SUCCESS";
		free(result);
	}
	ast_log(LOG_DEBUG, "Exiting with CWMPSTATUS=%s.\n", status);
	pbx_builtin_setvar_helper(chan, "CWMPSTATUS", status);

	LOCAL_USER_REMOVE(u);
	return 0;
}

static int factory_reset_exec(struct ast_channel *chan, void *data)
{
	struct localuser *u;
	char *result, *status = "FAILURE";
	int response_len;

	LOCAL_USER_ADD(u);

	result = cwmp_ipc_run_request(factoryReset, &response_len);
	if (result)
	{
		if (!strstr(result, "<cwmp:Fault>"))
		    status = "SUCCESS";
		free(result);
	}
	ast_log(LOG_DEBUG, "Exiting with CWMPSTATUS=%s.\n", status);
	pbx_builtin_setvar_helper(chan, "CWMPSTATUS", status);

	LOCAL_USER_REMOVE(u);
	return 0;
}

int unload_module(void)
{
	int res;

	res = ast_unregister_application(app_get);
	res |= ast_unregister_application(app_set);
	res |= ast_unregister_application(app_factory_reset);
	
	STANDARD_HANGUP_LOCALUSERS;

	return res;	
}

int load_module(void)
{
	int res;

	res = ast_register_application(app_get, get_exec,
	    get_synopsis, get_descrip);
	res |= ast_register_application(app_set, set_exec,
	    set_synopsis, set_descrip);
	res |= ast_register_application(app_factory_reset, factory_reset_exec,
	    factory_reset_synopsis, factory_reset_descrip);

	return res;
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
