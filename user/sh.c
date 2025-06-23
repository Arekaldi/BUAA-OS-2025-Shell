#include <args.h>
#include <lib.h>
#include <cd.h>
#include <env_var.h>

#define WHITESPACE " \t\r\n"
#define SYMBOLS "<|>&;()"

int num_env_vars = 0;
struct EnvVar environ[ENV_VAR_MAX];

/* Overview:
 *   Parse the next token from the string at s.
 *
 * Post-Condition:
 *   Set '*p1' to the beginning of the token and '*p2' to just past the token.
 *   Return:
 *     - 0 if the end of string is reached.
 *     - '<' for < (stdin redirection).
 *     - '>' for > (stdout redirection).
 *     - '|' for | (pipe).
 *     - 'w' for a word (command, argument, or file name).
 *
 *   The buffer is modified to turn the spaces after words into zero bytes ('\0'), so that the
 *   returned token is a null-terminated string.
 */

const static char *builtin_commands[] = {
	"cd",
	"pwd",
	"exit",
	"declare",
	"unset",
	NULL
};

int is_builtin_command(char *cmd) {
    for (int i = 0; builtin_commands[i] != NULL; i++) {
        if (strcmp(cmd, builtin_commands[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

int _gettoken(char *s, char **p1, char **p2) {
	*p1 = 0;
	*p2 = 0;
	if (s == 0) {
		return 0;
	}

	while (strchr(WHITESPACE, *s)) {
		*s++ = 0;
	}
	if (*s == 0) {
		return 0;
	}

	if (strchr(SYMBOLS, *s)) {
		int t = *s;
		*p1 = s;
		*s++ = 0;
		*p2 = s;
		return t;
	}

	*p1 = s;
	while (*s && !strchr(WHITESPACE SYMBOLS, *s)) {
		s++;
	}
	*p2 = s;
	return 'w';
}

int gettoken(char *s, char **p1) {
	static int c, nc;
	static char *np1, *np2;

	if (s) {
		nc = _gettoken(s, &np1, &np2);
		return 0;
	}
	c = nc;
	*p1 = np1;
	nc = _gettoken(np2, &np1, &np2);
	return c;
}

#define MAXARGS 128

int parsecmd(char **argv, int *rightpipe) {
	int argc = 0;
	while (1) {
		char *t;
		int fd, r;
		int c = gettoken(0, &t);
		switch (c) {
		case 0:
			return argc;
		case 'w':
			if (argc >= MAXARGS) {
				debugf("too many arguments\n");
				exit();
			}
			argv[argc++] = t;
			break;
		case '<':
			if (gettoken(0, &t) != 'w') {
				debugf("syntax error: < not followed by word\n");
				exit();
			}
			// Open 't' for reading, dup it onto fd 0, and then close the original fd.
			// If the 'open' function encounters an error,
			// utilize 'debugf' to print relevant messages,
			// and subsequently terminate the process using 'exit'.
			/* Exercise 6.5: Your code here. (1/3) */
			fd = open(t, O_RDONLY);
			if (fd < 0) {
				debugf("failed to open '%s'\n", t);
				exit();
			}
			dup(fd, 0);
			close(fd);
			// user_panic("< redirection not implemented");

			break;
		case '>':
			if (gettoken(0, &t) != 'w') {
				debugf("syntax error: > not followed by word\n");
				exit();
			}
			// Open 't' for writing, create it if not exist and trunc it if exist, dup
			// it onto fd 1, and then close the original fd.
			// If the 'open' function encounters an error,
			// utilize 'debugf' to print relevant messages,
			// and subsequently terminate the process using 'exit'.
			/* Exercise 6.5: Your code here. (2/3) */
			fd = open(t, O_WRONLY | O_CREAT | O_TRUNC);
			if (fd < 0) {
				debugf("failed to open '%s'\n", t);
				exit();
			}
			dup(fd, 1);
			close(fd);
			// user_panic("> redirection not implemented");

			break;
		case '|':;
			/*
			 * First, allocate a pipe.
			 * Then fork, set '*rightpipe' to the returned child envid or zero.
			 * The child runs the right side of the pipe:
			 * - dup the read end of the pipe onto 0
			 * - close the read end of the pipe
			 * - close the write end of the pipe
			 * - and 'return parsecmd(argv, rightpipe)' again, to parse the rest of the
			 *   command line.
			 * The parent runs the left side of the pipe:
			 * - dup the write end of the pipe onto 1
			 * - close the write end of the pipe
			 * - close the read end of the pipe
			 * - and 'return argc', to execute the left of the pipeline.
			 */
			int p[2];
			/* Exercise 6.5: Your code here. (3/3) */
			r = pipe(p);
			if (r != 0) {
                debugf("pipe failed\n");
                exit();
            }
            r = fork();
            if (r < 0) {
                debugf("fork failed for pipe\n");
                exit();
            }
			*rightpipe = r;
			if (r == 0) {
				dup(p[0], 0);
				close(p[0]);
				close(p[1]);
				return parsecmd(argv, rightpipe);
			} else {
				dup(p[1], 1);
				close(p[1]);
				close(p[0]);
				return argc;
			}
			// user_panic("| not implemented");

			break;
		}
	}

	return argc;
}

void runcmd(char *s) {
	gettoken(s, 0);

	char *argv[MAXARGS];
	int rightpipe = 0;
	int argc = parsecmd(argv, &rightpipe);
	if (argc == 0) {
		return;
	}

	debugf("runcmd: %s\n", s);
	for(int i = 0; i < argc; i++) {
		debugf("argv[%d] = '%s'\n", i, argv[i]);
	}
	if(strcmp(argv[0], "cd") == 0) {
		if (cd_shell(argc, argv) < 0) {
		}
		return;
	}
	else if(strcmp(argv[0], "pwd") == 0) {
		if (pwd_shell(argc, argv) < 0) {
		}
		return;
	}
	else if(strcmp(argv[0], "exit") == 0) {
		exit();
	}

	argv[argc] = 0;

	int child = spawn(argv[0], argv);
	close_all();
	if (child >= 0) {
		wait(child);
	} else {
		debugf("spawn %s: %d\n", argv[0], child);
	}
	if (rightpipe) {
		wait(rightpipe);
	}
	exit();
}

void readline(char *buf, u_int n) {
    int r;
    char c;
    int cursor = 0;  // 光标位置
    
    for(int i = 0; i < n; ++i) {
        if ((r = read(0, &c, 1)) != 1) {
            if (r < 0) {
                debugf("read error: %d\n", r);
            }
            exit();
        }
        
        if (c == '\b' || c == 0x7f) {
            // Backspace: 删除光标左侧字符
            if (cursor > 0) {
				cursor--;
				i -= 2;
				printf("\b");
				for(int k = cursor; k <= i; k++) {
					buf[k] = buf[k + 1];
					printf("%c", buf[k]);
				}
				printf(" \b");
				for(int k = cursor; k <= i; k++) printf("\b");
			} else {
				i -= 1;
				printf("\b ");
			}
        } else if (c == 27) {
            char c1, c2;
            if ((r = read(0, &c1, 1)) != 1) {
                if (r < 0) debugf("read error: %d\n", r);
                exit();
            }
            if ((r = read(0, &c2, 1)) != 1) {
                if (r < 0) debugf("read error: %d\n", r);
                exit();
            }
            
            if (c1 == 91) {
                if (c2 == 67) {
                    if (cursor < i)
                        cursor++;
                    else
                        printf("\b");
                } else if (c2 == 68) {
                    if (cursor > 0) {
                        cursor--;
                    }
					else
                        printf("\033[C");
                }

				i--;
            }
        } else if (c == 1) {
            // Ctrl-A: 光标跳至最前
            while (cursor > 0) {
                cursor--;
                printf("\b");
            }
			i--;
        } else if (c == 5) {
            // Ctrl-E: 光标跳至最后
            while (cursor < i) {
                cursor++;
                printf("\033[C");
            }
			i--;
        } else if (c == 11) {
            // Ctrl-K: 删除从当前光标处到最后的文本
            if (cursor < i) {
                // 清除从光标到行尾的内容
                for (int j = cursor; j < i; j++) {
                    printf(" ");
                }
                // 将光标移回原位置
                for (int j = cursor; j < i; j++) {
                    printf("\b");
                }
                i = cursor;
            }
        } else if (c == 21) {
            // Ctrl-U: 删除从最开始到光标前的文本
            if (cursor > 0) {
                // 将光标后的内容移到开头
                for (int j = 0; j < i - cursor; j++) {
                    buf[j] = buf[cursor + j];
                }
                
                // 清除整行并重新显示
                for (int i = 0; i < cursor; i++) {
                    printf("\b");
                }
                for(int j = 0; j < i; ++j) {
                    printf(" ");
                }
                for(int j = 0; j < i; ++j) {
                    printf("\b");
                }

                i -= cursor;
                cursor = 0;
                
                // 重新显示内容
                for (int j = 0; j < i; j++) {
                    printf("%c", buf[j]);
                }
            }
        } else if (c == 23) {
            // Ctrl-W: 向左删除最近一个word
            int start = cursor - 1;
            
            // 先跳过空白字符
            while (start >= 0 && buf[start] == ' ') {
                start--;
				printf("\b");
            }
            
            // 再删除非空白字符
            while (start >= 0 && buf[start] != ' ') {
                start--;
				printf("\b");
            }
            
            if (start < cursor) {
                // 将光标后的内容向前移动
                for (int k = start; k < i - (cursor - start); k++) {
                    buf[k + 1] = buf[k + (cursor - start)];
					printf("%c", buf[k + 1]);
                }
                
                // 移动光标到删除开始位置
                for (int k = 0; k < cursor - start - 1; k++) {
                    printf(" ");
                }
                
                // 重新显示从删除位置到行尾的内容
                for (int k = start; k < i - 1; k++) {
					printf("\b");
                }
                
                i -= (cursor - start);
                cursor = start + 1;
            }
        } else if (c == '\r' || c == '\n') {
            // 回车: 结束输入
            buf[i] = 0;
            printf("\n");
            return;
        } else if (c >= 32 && c <= 126) {
            for (int j = i; j > cursor; j--) {
                buf[j] = buf[j - 1];
            }
			
			for(int j = cursor + 1; j <= i; ++j) {
				printf("%c", buf[j]);
			}
            
			for(int j = i; j > cursor; --j)
				printf("\b");

            buf[cursor] = c;
			cursor++;
        }
    }
    
    debugf("line too long\n");
    while ((r = read(0, buf, 1)) == 1 && buf[0] != '\r' && buf[0] != '\n') {
        ;
    }
    buf[0] = 0;
}

char buf[1024];

void usage(void) {
	printf("usage: sh [-ix] [script-file]\n");
	exit();
}

int main(int argc, char **argv) {
	num_env_vars = getEnvVar(argc, argv);
	int r;
	int interactive = iscons(0);
	int echocmds = 0;
	printf("\n:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::\n");
	printf("::                                                         ::\n");
	printf("::                     MOS Shell 2024                      ::\n");
	printf("::                                                         ::\n");
	printf(":::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::\n");
	ARGBEGIN {
	case 'i':
		interactive = 1;
		break;
	case 'x':
		echocmds = 1;
		break;
	default:
		usage();
	}
	ARGEND

	if (argc > 1) {
		usage();
	}
	if (argc == 1) {
		close(0);
		if ((r = open(argv[0], O_RDONLY)) < 0) {
			user_panic("open %s: %d", argv[0], r);
		}
		user_assert(r == 0);
	}
	for (;;) {
		if (interactive) {
			printf("\n$ ");
		}
		readline(buf, sizeof buf);

		if (buf[0] == '#') {
			continue;
		}
		if (echocmds) {
			printf("# %s\n", buf);
		}

		debugf("running shell with cmd %s\n", buf);

		if ((r = fork()) < 0) {
			user_panic("fork: %d", r);
		}
		if (r == 0) {
			runcmd(buf);
			exit();
		} else {
			wait(r);
		}
	}
	return 0;
}
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

void passEnvVarToChild(int *argc, char **argv) {
    strcpy(argv[*argc], "areka");
    (*argc)++;
    for(int i = 0; i < num_env_vars; ++i) {
        if(environ[i].type == ENV_VAR_TYPE_PART || environ[i].valid == 0)
            continue;
        strncpy(argv[*argc], environ[i].name, 16);
        strncpy(argv[*argc + 1], environ[i].value, 16);
        argv[*argc + 2][0] = (environ[i].type == ENV_VAR_TYPE_PART) ? '0' : '1';
        argv[*argc + 3][0] = environ[i].readOnly + '0';
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

int strchr_Pos(char *str, char c, int start_pos) {
    for(int i = start_pos; str[i] != '\0'; i++) {
        if(str[i] == c) {
            return i;
        }
    }
    return -1;
}

int isValidVarName(char *name) {
    if(name == NULL || name[0] == '\0') {
        return 0;
    }
    
    // First character must be letter or underscore
    if(!((name[0] >= 'a' && name[0] <= 'z') || 
         (name[0] >= 'A' && name[0] <= 'Z') || 
         name[0] == '_')) {
        return 0;
    }
    
    // Remaining characters must be letters, digits, or underscores
    for(int i = 1; name[i] != '\0'; i++) {
        if(!((name[i] >= 'a' && name[i] <= 'z') || 
             (name[i] >= 'A' && name[i] <= 'Z') || 
             (name[i] >= '0' && name[i] <= '9') || 
             name[i] == '_')) {
            return 0;
        }
    }
    
    return 1;
}

int declare_shell(int argc, char **argv) {
    enum env_var_type type = ENV_VAR_TYPE_PART;
    int readOnly = 0;
    ARGBEGIN {
        case 'x':
            type = ENV_VAR_TYPE_ENV;
            break;
        case 'r':
            readOnly = 1;
            break;
        default:
            usage_declare();
            return -1;
    }
    ARGEND

    if(argc == 0 && type == ENV_VAR_TYPE_PART && readOnly == 0) {
		return 0;
	}
	
	if(argc == 1) {
		int pos = strchr_Pos(argv[0], '=', 0);
        if(pos == -1) {
            environ[num_env_vars].type = type;
            environ[num_env_vars].readOnly = readOnly;
            strncpy(environ[num_env_vars].name, argv[0], 16);
            environ[num_env_vars].value[0] = '\0';
            num_env_vars++;
            return 0;
        }
        int pos1 = strchr_Pos(argv[0], ' ', pos + 1);
        if(pos1 != -1) {
            // 含有多个 '='
            printf("declare: \'%s\': not a valid identifier", argv[0]);
            return -1;
        }
        char name[17];
        char value[17];
        strncpy(name, argv[0], pos);
        name[pos] = '\0';

        int id = findIdByName(name);
        if(id != -1) {
            if(environ[id].readOnly) {
                printf("declare: \'%s\': read-only variable\n", name);
                return -1;
            }
        }

        strncpy(value, argv[0] + pos + 1, 16);
        if(!isValidVarName(name)) {
            printf("declare: \'%s\': not a valid identifier\n", name);
            return -1;
        }
        if(strlen(value) > 16) {
            printf("declare: \'%s\': value too long\n", value);
            return -1;
        }
        if(strlen(name) > 16) {
            printf("declare: \'%s\': name too long\n", name);
            return -1;
        }

        environ[num_env_vars].type = type;
        environ[num_env_vars].readOnly = readOnly;
        strncpy(environ[num_env_vars].name, name, 16);
        strncpy(environ[num_env_vars].value, value, 16);
        num_env_vars++;
        return 0;
    }

	usage_declare();
	return 1;
}

int unset_shell(int argc, char **argv) {
    if(argc == 0) {
        usage_unset();
        return -1;
    }
    
    for(int i = 0; i < argc; ++i) {
        int id = findIdByName(argv[i]);
        if(id == -1) {
            printf("unset: \'%s\': not found\n", argv[i]);
            continue;
        }
        if(environ[id].readOnly) {
            printf("unset: \'%s\': read-only variable\n", argv[i]);
            continue;
        }
        environ[id].valid = 0; // Mark as invalid
    }
    return 0;
}

void usage_declare(void) {
    printf("usage: declare [-xr] [NAME [=VALUE]]\n");
}

void usage_unset(void) {
    printf("usage: unset NAME\n");
}