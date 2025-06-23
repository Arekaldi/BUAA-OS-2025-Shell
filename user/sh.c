#include <args.h>
#include <lib.h>
#include <cd.h>

#define WHITESPACE " \t\r\n"
#define SYMBOLS "<|>&;()"

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
                for (int i = 0; i < i; i++) {
                    printf("%c", buf[i]);
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
