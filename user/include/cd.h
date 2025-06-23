#ifndef _USER_CD_H_
#define _USER_CD_H_

#include <types.h>

#define MAXPATH 1024

int cd_shell(int argc, char **argv);
int pwd_shell(int argc, char **argv);
char *strncpy(char *dest, const char *src, u_int n);

#endif
