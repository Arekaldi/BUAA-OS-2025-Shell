#include <lib.h>
#include <env_var.h>
#include <cd.h>

int num_env_vars = 0;
struct EnvVar environ[ENV_VAR_MAX];

int getEnvVar(int argc, char **argv) {
    int flag = 0;
    int num = 0;
    for(int i = 0; i < argc; ++i) {
        if(strcmp(argv[i], "areka") == 0) {
            flag = 1;
            continue;
        }
        if(flag) {
            if(num < ENV_VAR_MAX) {
                strncpy(environ[num].name, argv[i], 16);
                strncpy(environ[num].value, argv[i + 1], 16);
                environ[num].type = argv[i + 2][0] == '0' ? ENV_VAR_TYPE_PART : ENV_VAR_TYPE_ENV;
                environ[num].readOnly = (argv[i + 3][0] - '0');
                environ[num].valid = 1;
                num++;
                i += 3;
            } else {
                debugf("too many environment variables\n");
            }
        }
    }
    return num;
}

char zero[] = "0", one[] = "1";
char areka[] = "areka";
char name[128][17];
char value[128][17];

void passEnvVarToChild(int *argc, char **argv) {
    argv[*argc] = areka;
    (*argc)++;
    memset(name, 0, sizeof(name));
    memset(value, 0, sizeof(value));
    for(int i = 0; i < num_env_vars; ++i) {
        if(environ[i].type == ENV_VAR_TYPE_PART || environ[i].valid == 0)
            continue;
        strncpy(name[i], environ[i].name, 16);
        strncpy(value[i], environ[i].value, 16);
        argv[*argc] = name[i];
        argv[*argc + 1] = value[i];
        argv[*argc + 2] = (environ[i].type == ENV_VAR_TYPE_PART) ? zero : one;
        argv[*argc + 3] = environ[i].readOnly ? one : zero;
        *argc += 4;
    }
    return;
}

int findIdByName(char *name) {
    for(int i = 0; i < num_env_vars; ++i) {
        if(strcmp(environ[i].name, name) == 0 && environ[i].valid == 1) {
            return i;
        }
    }
    return -1;
}

void printEnvVar(void) {
    for(int i = 0; i < num_env_vars; ++i) {
        if(environ[i].valid == 0)
            continue;
        printf("%s=%s\n", environ[i].name, environ[i].value);
    }
}

// 判断字符串是否为环境变量形式 ($name)
int isEnvVar(char *str) {
    if (str == NULL || str[0] != '$') {
        return 0;
    }
    return 1;
}

// 从环境变量字符串中提取变量名 ($name -> name)
char* extractVarName(char *envVar) {
    if (envVar == NULL || envVar[0] != '$') {
        return NULL;
    }
    return envVar + 1; // 跳过 '$' 符号
}

int main(int argc, char **argv) {
	char workPath[MAX_PATH];
    syscall_env_getpwd(syscall_getenvid(), workPath);

    u_int f_envid = get_f_envid(argc, argv);

    if(f_envid == -1) {
        user_panic("touch: no envid found in arguments");
        exit();
    }

	num_env_vars = getEnvVar(argc, argv);


    for(int i = 0; i < argc; ++i) {
		if(strcmp(argv[i], "areka") == 0) {
			argc = i - 1;
			break;
		}
	}

	int i, nflag;

	nflag = 0;
	if (argc > 1 && strcmp(argv[1], "-n") == 0) {
		nflag = 1;
		argc--;
		argv++;
	}
	for (i = 1; i < argc; i++) {
		if (i > 1) {
			printf(" ");
		}
		if(isEnvVar(argv[i])) {
			// 如果是环境变量，替换为实际值
			char *varName = extractVarName(argv[i]);
			int id = findIdByName(varName);
			if(id != -1) {
				printf("%s", environ[id].value);
			} else {
				;
			}
		} else
			printf("%s", argv[i]);
	}
	if (!nflag) {
		printf("\n");
	}
	return 0;
}
