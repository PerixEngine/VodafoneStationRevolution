/****************************************************************************
 * Copyright  (C) 2000 - 2008 Jungo Ltd. All Rights Reserved.
 * 
 *  rg/pkg/dslhome/ipc/client.h * 
 * 
 * This file is Jungo's confidential and proprietary property. 
 * This file may not be copied, 
 * distributed or otherwise used in any way without 
 * the express prior approval of Jungo Ltd. 
 * For information contact info@jungo.com
 * 
 * 
 */

#ifndef _IPC_CLIENT_
#define _IPC_CLIENT_

#define CWMP_IPC_DATA_PORT 9009

/** @brief Sends a SOAP CWMP information request to OpenRG
 * @param request The SOAP request.
 * @param response_len If not NULL, will be set to the length of the received
 *  response.
 * @return On success, SOAP response from OpenRG. On error, NULL.
 *   Returned pointer should be freed.
 */
char *cwmp_ipc_run_request(const char *request, int *response_len);

#endif
