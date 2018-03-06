#include "includes.h"
#define DONT_REDEFINE_WRITE
#include "write_all.h"

int write_all(int f, char *b, int n)
{
    int total_sent = 0;

    while (total_sent < n)
    {
	int sent = write(f, b+total_sent, n-total_sent);

	if (sent < 0)
	{
	    if (errno != EAGAIN)
	    {
		d_printf("Error writing local file errno=%d\n", errno);
		return sent;
	    }
	    else
	    {
		/* sleep for 1 millisecond */
		struct timespec rqtp;

		rqtp.tv_sec = 0;
		rqtp.tv_nsec = 1000;
		nanosleep(&rqtp, (struct timespec *)NULL);
	    }
	}
	else
	    total_sent += sent;
    }
    return total_sent;
}
