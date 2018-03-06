/****************************************************************************
 * Copyright  (C) 2000 - 2008 Jungo Ltd. All Rights Reserved.
 * 
 *  rg/pkg/dslhome/ipc/client.c * 
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
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifdef CONFIG_RG_CWMP_IPC_LAN_HOST_DEBUG
#define CONN_ADDR "192.168.1.1"
#else
#define CONN_ADDR "127.0.0.1"
#endif

#define BUFFER_SIZE 256

#define ERROR(msg) \
    do \
    { \
	fprintf(stderr, msg "\n");\
	goto Error; \
    } while(0)

static int init_socket(int port)
{
    int fd = -1;
    struct sockaddr_in server_addr;

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	ERROR("Failed opening socket.");

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_aton(CONN_ADDR, &server_addr.sin_addr) <= 0)
       ERROR("Failed generating address.");

    if (connect(fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
       ERROR("Failed to connect to server.");

    return fd;

Error:
    if (fd > 0)
	close(fd);

    return -1;
}

char *cwmp_ipc_run_request(const char *request, int *response_len)
{
    char buf[BUFFER_SIZE] = { 0 };
    char *str = NULL, *tmp;
    int pos = 0, alloc_size = BUFFER_SIZE, len, data_fd = -1;

    if ((data_fd = init_socket(CWMP_IPC_DATA_PORT)) < 0)
	return NULL;

    /* Write request WITH null termination */
    if (write(data_fd, request, strlen(request) + 1) < 0)
	goto Exit;

    if (!response_len)
	goto Exit;

    str = malloc(alloc_size);
    while ((len = read(data_fd, buf, BUFFER_SIZE - 1)) > 0)
    {
	if (pos + len > alloc_size)
	{
	    alloc_size = (alloc_size + len) * 2;
	    tmp = str;
	    if (!(str = realloc(str, alloc_size)))
	    {
		free(tmp);
		goto Exit;
	    }
	}
	memcpy(str + pos, buf, len);
	pos += len;
    }

    if (len < 0)
	fprintf(stderr, "%s: failed reading data from socket\n", __func__);

    str[pos] = '\0';
    *response_len = pos;

Exit:
    close(data_fd);
    return str;
}
