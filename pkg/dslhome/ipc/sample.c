/****************************************************************************
 * Copyright  (C) 2000 - 2008 Jungo Ltd. All Rights Reserved.
 * 
 *  rg/pkg/dslhome/ipc/sample.c * 
 * 
 * This file is Jungo's confidential and proprietary property. 
 * This file may not be copied, 
 * distributed or otherwise used in any way without 
 * the express prior approval of Jungo Ltd. 
 * For information contact info@jungo.com
 * 
 * 
 */

#include "client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void send_cwmp(void)
{
    int req_len;
    char buffer[1024], *res = NULL;

    printf("Enter request: ");
    if (!fgets(buffer, sizeof(buffer), stdin))
    {
	printf("Error reading request\n");
	return;
    }

    req_len = strlen(buffer);
    if (req_len > 0 && buffer[req_len - 1] == '\n')
	buffer[req_len - 1] = '\0';

    res = cwmp_ipc_run_request(buffer, NULL);

    if (res)
    {
	printf("Got response:\n%s\n", res);
	free(res);
    }
    else
	printf("Error sending request\n");
}

int main(int argc, char *argv[])
{
    send_cwmp();

    return 0;
}
