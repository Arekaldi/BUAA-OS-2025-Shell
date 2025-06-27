#ifndef _ENV_VAR_H_
#define _ENV_VAR_H_

#define ENV_VAR_MAX 128

#include <types.h>

enum env_var_type {
    ENV_VAR_TYPE_PART,
    ENV_VAR_TYPE_ENV
};

struct EnvVar {
    char name[17];
    char value[17];
    enum env_var_type type; // ENV_VAR_TYPE_PART or ENV_VAR_TYPE_ENV
    u_int readOnly;
    u_int valid;
};

int declare_shell(int argc, char **argv);
int unset_shell(int argc, char **argv);
void usage_declare(void);
void usage_unset(void);
int strchr_Pos(char *str, char c, int start_pos);

#endif