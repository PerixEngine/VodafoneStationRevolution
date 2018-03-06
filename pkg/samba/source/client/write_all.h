#ifndef _SMB_CLIENT_WRITE_ALL_H_
#define _SMB_CLIENT_WRITE_ALL_H_

int write_all(int f, char *b, int n);

#ifndef DONT_REDEFINE_WRITE
#define write(f,b,n) write_all(f,b,n)
#endif

#endif
