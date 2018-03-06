/****************************************************************************
 *
 * rg/pkg/samba/source/printing/print_rg.c
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

#include "includes.h"
#include "printing.h"

#include <lpd_client.h>

/*
 * printing interface definitions for openrg
 */

static int rg_job_delete(const char *printer_name, const char *lprm_command,
    struct printjob *pjob)
{
    int rc;

    rc = lpd_client_lprm((char *)printer_name, pjob->sysjob);
    
    DEBUG(2, ("lprm job %d returns %d\n", pjob->sysjob, rc));

    return rc;
}

static int rg_job_pause(int snum, struct printjob *pjob)
{
    return -1;
}

static int rg_job_resume(int snum, struct printjob *pjob)
{
    return -1;
}

static int rg_job_submit(int snum, struct printjob *pjob)
{
    /* jobs are submitted as part of the spooling process */
    return 0;
}

static int rg_queue_get(const char *printer_name,
               enum printing_types printing_type, char *lpq_command,
               print_queue_struct **q, print_status_struct *status)
{
    int fd;
    FILE *outp;
    int numlines = 50;
    int qcount = 0;
    print_queue_struct *queue = NULL;

    fd = lpd_client_lpq((char *)printer_name);

    if (fd == -1)
	return 0;
    
    outp = fdopen(fd, "r");

    /* turn the lpq output into a series of job structures */
    
    ZERO_STRUCTP(status);
    queue = calloc(numlines+1, sizeof(print_queue_struct));
    if (!queue)
	goto Done;

    while (1)
    {
	char buf[200];

	if (!fgets(buf, sizeof(buf), outp))
	    break;
		    
	/* parse the line */
	if (parse_lpq_entry(printing_type, buf, &queue[qcount], status,
	    qcount==0))
	{
	    qcount++;
	}

	if (qcount == numlines)
	{
	    numlines *= 2;
	    queue = realloc(queue, numlines);
	    if (!queue)
	    {
		qcount = 0;
		goto Done;
	    }
	}
    }		
Done:
    fclose(outp);
    *q = queue;
    return qcount;
}

static int rg_queue_pause(int snum)
{
    return -1;
}

static int rg_queue_resume(int snum)
{
    return -1;
}

struct printif rg_printif = {
    PRINT_VLP,
    rg_queue_get,
    rg_queue_pause,
    rg_queue_resume,
    rg_job_delete,
    rg_job_pause,
    rg_job_resume,
    rg_job_submit,
};

