#include <args.h>
#include <lib.h>
#include <cd.h>
#include <env_var.h>

#define WHITESPACE " \t\r\n"
#define SYMBOLS "<|>&;()#"

int num_env_vars = 0;
struct EnvVar environ[ENV_VAR_MAX];

int getEnvVar(int argc, char **argv);
void passEnvVarToChild(int *argc, char **argv);
int findIdByName(char *name);
void printEnvVar(void);

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
#define MAXCMD 128

int parsecmd(char **argv, int *rightpipe, char *workPath) {
    int argc = 0;
    char *path;
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
            path = resolvePath(t, workPath);
			fd = open(path, O_RDONLY);
			if (fd < 0) {
				debugf("failed to open '%s'\n", path);
				exit();
			}
			dup(fd, 0);
			close(fd);
			// user_panic("< redirection not implemented");

			break;
		case '>':
            int c1 = gettoken(0, &t);
            //a1 >> a2
            if(c1 == '>') {
				if (gettoken(0, &t) != 'w') {
					debugf("syntax error: >> not followed by word\n");
					exit();
				}
				char *path = resolvePath(t, workPath);
				fd = open(path, O_WRONLY | O_CREAT);
				if(fd < 0) {
					debugf("failed to open '%s'\n", path);
					exit();
				}
				if (write_extend(fd, 0, 0) != 0) {
                    debugf("write error copying\n");
                    exit();
                }

				dup(fd, 1);
				close(fd);
				
				break;
			} else if (c1 != 'w') {
				debugf("syntax error: > not followed by word\n");
				exit();
			}
			// Open 't' for writing, create it if not exist and trunc it if exist, dup
			// it onto fd 1, and then close the original fd.
			// If the 'open' function encounters an error,
			// utilize 'debugf' to print relevant messages,
			// and subsequently terminate the process using 'exit'.
			/* Exercise 6.5: Your code here. (2/3) */
            path = resolvePath(t, workPath);
			fd = open(path, O_WRONLY | O_CREAT | O_TRUNC);
			if (fd < 0) {
				debugf("failed to open '%s'\n", path);
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
				return parsecmd(argv, rightpipe, workPath);
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

static char symbols[][2] = {"<", "|", ">", "&", ";", "(", ")", "#"};
int parseAndOr(char **argv, char *andP, char *orP, char *endP, char *editP) {
    int argc = 0;
	while (1) {
		char *t;
		int r;
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
		case '|':
            r = gettoken(0, &t);
            if(r == '|') {
                argv[argc++] = (char *)orP;
                return argc;
            }
            if(argc == 0) {
                debugf("invalid arguments\n");
                exit();
            }
            strcat(argv[argc - 1], " |");
            argv[argc++] =  t;
            break;
        
        case '&':
            r = gettoken(0, &t);
            if(r != '&') {
                debugf("syntax error: & not followed by another &\n");
                exit();
            }
            argv[argc++] = (char *)andP;
            return argc;

        case ';':
            argv[argc++] = (char *)endP;
            return argc;
        case '#':
            argv[argc++] = (char *)editP;
            return argc;

        default:
            for(int i = 0; i < 8; ++i) {
                if(c == symbols[i][0]) {
                    argv[argc++] = symbols[i];
                    break;
                }
            }
            break;
        }
    }
	return argc;
}

int is_builtin_command(char *cmd) {
    gettoken(cmd, 0);
    char *t;
    int r = gettoken(0, &t);
    if(r != 'w')
        return -1;
    for (int i = 0; builtin_commands[i] != NULL; i++) {
        if (strcmp(t, builtin_commands[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

int run_builtin_command(char *cmd, int argc, char **argv) {

    gettoken(cmd, 0);
    char *t;
    int r = gettoken(0, &t);
    if(r != 'w')
        return -1;
    if(strcmp(t, "cd") == 0) {
        return cd_shell(argc, argv);
    } else if(strcmp(t, "pwd") == 0) {
        return pwd_shell(argc, argv);
    } else if(strcmp(t, "exit") == 0) {
        exit();
    } else if(strcmp(t, "declare") == 0) {
        return declare_shell(argc, argv);
    } else if(strcmp(t, "unset") == 0) {
        return unset_shell(argc, argv);
    }
    return -2;
}
    
static char newcmd[MAXARGS];

void runcmd(char *s, u_int f_envid, char *workPath) {

	gettoken(s, 0);

	char *argv[MAXARGS];
	int rightpipe = 0;
	int argc = parsecmd(argv, &rightpipe, workPath);
	if (argc == 0) {
		return;
	}

    argv[argc] = 0;

    memset(newcmd, 0, sizeof(newcmd));
    if(strcmp(argv[0], "echo") == 0 || strcmp(argv[0], "/echo") == 0) {
		strcpy(newcmd, "/echo.b");
		argv[0] = newcmd;
	} else if(strcmp(argv[0], "history") == 0 && argc == 1) {
		strcpy(newcmd, "/cat.b");
		argv[0] = newcmd;
		argv[argc++] = (char *)&"/.mos_history";
	} else if(strcmp(argv[0], "ls") == 0 || strcmp(argv[0], "/ls") == 0) {
		strcpy(newcmd, "/ls.b");
		argv[0] = newcmd;
	} else if(strcmp(argv[0], "cat") == 0 || strcmp(argv[0], "/cat") == 0) {
		strcpy(newcmd, "/cat.b");
		argv[0] = newcmd;
	} else if(strcmp(argv[0], "halt") == 0 || strcmp(argv[0], "/halt") == 0) {
		strcpy(newcmd, "/halt.b");
		argv[0] = newcmd;
	} else if(strcmp(argv[0], "rm") == 0 || strcmp(argv[0], "/rm") == 0) {
		strcpy(newcmd, "/rm.b");
		argv[0] = newcmd;
	} else if(strcmp(argv[0], "sh") == 0 || strcmp(argv[0], "/sh") == 0) {
		strcpy(newcmd, "/sh.b");
		argv[0] = newcmd;
	} else if(strcmp(argv[0], "mkdir") == 0 || strcmp(argv[0], "/mkdir") == 0) {
		strcpy(newcmd, "/mkdir.b");
		argv[0] = newcmd;
	} else if(strcmp(argv[0], "touch") == 0 || strcmp(argv[0], "/touch") == 0) {
		strcpy(newcmd, "/touch.b");
		argv[0] = newcmd;
	}

    int now_envid = syscall_getenvid();

    char env_id_str[12];
    num2str(env_id_str, now_envid);
    argv[argc++] = env_id_str; // Add the environment ID as the last argument

    passEnvVarToChild(&argc, argv);

    u_int r = 0;
	int child = spawn(argv[0], argv);
	close_all();
	if (child >= 0) {
        // wait(child);
        wait_my(&r, now_envid, child);
	} else {
		debugf("spawn %s: %d\n", argv[0], child);
        exit_my(-1, f_envid);
        // exit();
	}
	if (rightpipe) {
        // if(f_envid != -1)
        wait(rightpipe);
        // wait_my(&rr, now_envid, rightpipe);
        // else
            // wait(rightpipe);
	}

    exit_my(r == 0 ? 0 : -1, f_envid);
}

int runbuf(char *buf) {
    char workPath[MAX_PATH];
    syscall_env_getpwd(syscall_getenvid(), workPath);
    char andP[3], orP[3], endP[2], editP[2];
    strcpy(andP, "&&");
    strcpy(orP, "||");
    strcpy(endP, ";");
    strcpy(editP, "#");
    int argc[MAXARGS];
    int num = 0;
    gettoken(buf, 0);
    char *argv[MAXARGS][MAXARGS];
    while(1) {
        int c = parseAndOr(argv[num], andP, orP, endP, editP);
        if(c == 0) break;
        if(argv[num][c - 1] == editP) {
            argv[num][c - 1] = NULL;
            c--;
            argc[num] = c;
            num++;
            break;
        }
        argc[num] = c;
        num++;
    }

    // for(int i = 0; i < num; ++i) {
    //     debugf("argv[%d]: ", i);
    //     for(int j = 0; j < argc[i]; ++j) {
    //         debugf("%s ", argv[i][j]);
    //     }
    //     debugf("\n");
    // }

    char cmd[MAXFILESIZE], temp_cmd[MAXFILESIZE];
    int res[MAXARGS];
    memset(res, 0, sizeof(res));
    for(int i = 0; i < num; ++i) {
        memset(cmd, 0, sizeof(cmd));
        if(i != 0) {
            if(strcmp(argv[i - 1][argc[i - 1] - 1], "||") == 0) {
                if(res[i - 1] == 0)
                    continue;
            }
            if(strcmp(argv[i - 1][argc[i - 1] - 1], "&&") == 0) {
                if(res[i - 1] != 0)
                    continue;
            }
        }
        for(int j = 0; j < argc[i] - (i != num - 1); ++j) {
            strcat(cmd, " ");
            int flag = 0;
            memset(temp_cmd, 0, sizeof(temp_cmd));
            strcpy(temp_cmd, argv[i][j]);
            // debugf("argv: %s, temp_cmd: %s\n", argv[i][j], temp_cmd);
            while(strchr(temp_cmd, '$') != 0) {
                flag = 1;
                int pos = strchr_Pos(temp_cmd, '$', 0);
                char temp[MAXARGS];
                memset(temp, 0, sizeof(temp));
                strncpy(temp, temp_cmd, pos);
                // debugf("now here before $: %s\n", temp);
                int p = pos;
                while (temp_cmd[p] && !strchr(WHITESPACE SYMBOLS, temp_cmd[p]) && temp_cmd[p] != '/')
                    p++;
                char name[MAXARGS];
                strncpy(name, temp_cmd + pos + 1, p - pos - 1);
                name[p - pos - 1] = '\0'; // Null-terminate the name
                int id = findIdByName(name);
                if(id == -1) {
                    // debugf("env var %s not found\n", name);
                    flag = 0;
                    break;
                }
                strcat(temp, environ[id].value);
                // debugf("now here add value: %s\n", temp);
                strcat(temp, temp_cmd + p);
                // debugf("now here after env_var: %s\n", temp);
                strcpy(temp_cmd, temp);
                // debugf("%s\n", temp_cmd);
            }
            if(flag) {
                strcat(cmd, temp_cmd);
                strcpy(argv[i][j], temp_cmd);
            }
            else
                strcat(cmd, argv[i][j]);
        }

        strcpy(temp_cmd, cmd);

        char temp[MAXFILESIZE];
        if(strchr(temp_cmd, '`')) {
            int p = strchr_Pos(temp_cmd, '`', 0);
            memset(temp, 0, sizeof(temp));
            int pos = p;
            while(temp_cmd[++pos] != '`');
            strncpy(temp, temp_cmd + p + 1, pos - p - 1);

            //TODO declare
            if(strcmp(temp, "pwd") == 0) {
                char pwd[MAXFILESIZE];
                syscall_env_getpwd(syscall_getenvid(), pwd);
                // debugf("pwd: %s\n", pwd);
                strcpy(temp, pwd);
                goto catching;
            }

            strcat(temp, " > /.mos_catch");
            int child = fork();
            if (child < 0) {
                debugf("fork failed for command %s\n", cmd);
                exit();
            }
            if(child == 0) {
                runcmd(temp, syscall_getenvid());
                // runbuf(temp, workPath);
            }
            else {
                wait(child);
                // debugf("parent: %d, child: %d, run_res[%d]: %d\n", f_envid, child, i, rr);
            }
            int fd = open("/.mos_catch", O_RDONLY);
            if(fd < 0) {
                debugf("failed to open /.mos_catch\n");
                exit();
            }
            u_int rr;
			if ((rr = read(fd, temp, sizeof(temp))) < 0) return 0;
			close(fd);

            temp[rr] = '\0';
            
            // debugf("catching command content: %s\n", temp);

        catching:

            memset(temp_cmd, 0, sizeof(temp_cmd));

            strncpy(temp_cmd, cmd, p);
            // debugf("now here before `: %s\n", temp_cmd);
            strcat(temp_cmd, temp);
            // debugf("now here add run value: %s, value: %s\n", temp_cmd, temp);
            if(pos + 1 < strlen(cmd))
                strcat(temp_cmd, cmd + pos + 1);
            // debugf("now here after `: %s\n", temp_cmd);
            strcpy(cmd, temp_cmd);
            // strncpy
        }

        int r = is_builtin_command(temp_cmd);
        u_int rr;
        if(r == -1) {
            debugf("invaild command\n");
            // exit();
        }
        if(r == 1)
            rr = run_builtin_command(cmd, argc[i] - (i != num - 1), argv[i]);
        else {
            // debugf("running command: %s\n", cmd);
            int f_envid = syscall_getenvid();
            int child = fork();
            if (child < 0) {
                debugf("fork failed for command %s\n", cmd);
                // exit();
            }
            
            if(child == 0) {
                runcmd(cmd, f_envid);
            }
            else {
                // wait(child);
                wait_my(&rr, f_envid, child);
                // debugf("parent: %d, child: %d, run_res[%d]: %d\n", f_envid, child, i, rr);
            }
        }
        res[i] = rr;
    }

    return 0;
}


char history[20][1024];
int cur_cmdnum = -1;

void savecmd(char *s) {
	if(cur_cmdnum >= 20) return;

	int fd = open(".mos_history", O_WRONLY | O_CREAT );
        
	if(fd < 0) {
        	debugf("failed to open .mos_history");
                exit();
        }
        
	int n = strlen(s);
	if (write_extend(fd, s, n) != n) {
		debugf("write error copying");
		exit();
        }
	if (write(fd, &"\n", 1) != 1) {
		debugf("write error copying");
		exit();
	}

       	close(fd);

	strcpy(history[cur_cmdnum], s);
}

void readline(char *buf, u_int n) {
	cur_cmdnum++;
    int r;
    char c;
    int cursor = 0;  // 光标位置
    int cmd_num = cur_cmdnum;
    
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

            if(c1 == 91 && c2 == 65) {
				//上
				printf("\033[B");
				if(cmd_num > 0 && cmd_num < 20) {
					if(cmd_num == cur_cmdnum) {
						buf[i] = '\0';
						strcpy(history[cur_cmdnum], buf);
					}

					while(cursor--) printf("\033[D");
					for(int k = 0; k < i; k++) printf(" ");
					for(int k = 0; k < i; k++) printf("\033[D");

					cmd_num--;

					int buflen = strlen(history[cmd_num]);
					strcpy(buf, history[cmd_num]);
					for(int k = 0; k < buflen; k++) printf("%c", buf[k]);
					i = buflen;
					cursor = buflen;
				}
			} else if(c1 == 91 && c2 == 66) {
				//下
				if(cmd_num < cur_cmdnum && cmd_num < 19) {
					while(cursor--) printf("\033[D");
					for(int k = 0; k < i; k++) printf(" ");
					for(int k = 0; k < i; k++) printf("\033[D");

					cmd_num++;

					int buflen = strlen(history[cmd_num]);
					strcpy(buf, history[cmd_num]);
					for(int k = 0; k < buflen; k++) printf("%c", buf[k]);
					i = buflen;
					cursor = buflen;
				}
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

    char workPath[MAX_PATH];
    syscall_env_getpwd(syscall_getenvid(), workPath);

    for(int i = 0; i < argc; ++i) {
		if(strcmp(argv[i], "areka") == 0) {
			argc = i - 1;
			break;
		}
	}

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

        savecmd(buf);

		if (buf[0] == '#') {
			continue;
		}
		if (echocmds) {
			printf("# %s\n", buf);
		}

        runbuf(buf);
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

char zero[] = "0", one[] = "1";
char areka[] = "areka";
char name[MAXARGS][17];
char value[MAXARGS][17];

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

    if(argc == 0) {
        // debugf("%d\n", num_env_vars);
        for(int i = 0; i < num_env_vars; ++i) {
            if(!environ[i].valid)
                continue;
            if(environ[i].type == ENV_VAR_TYPE_ENV)
                printf("%s=%s\n", environ[i].name, environ[i].value);
        }
        for(int i = 0; i < num_env_vars; ++i) {
            if(!environ[i].valid)
                continue;
             if(environ[i].type == ENV_VAR_TYPE_PART)
                printf("%s=%s\n", environ[i].name, environ[i].value);
        }
        return 0;
	}
	
	else if(argc == 1) {
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
            if(environ[id].readOnly && environ[id].valid) {
                printf("declare: \'%s\': read-only variable\n", name);
                return -1;
            }
            strncpy(value, argv[0] + pos + 1, 16);

            environ[id].type = type;
            environ[id].readOnly = readOnly;
            strncpy(environ[id].value, value, 16);
            environ[id].valid = 1;
            return 0;
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
        environ[num_env_vars].valid = 1;
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