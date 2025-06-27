#include <cd.h>
#include <lib.h>
#include <string.h>

char *strncpy(char *dest, const char *src, u_int n) {
    if (n == 0) return dest;
    char *d = dest;
    const char *s = src;
    while (n-- && (*d++ = *s++));
    return dest;
}

char *strcat(char *dest, const char *src) {
    char *d = dest;
    while (*d) d++; // Move to the end of dest
    while ((*d++ = *src++)); // Copy src to dest
    return dest;
}

char** strtok(char *str, const char delim) {
    static char res[MAXPATH][MAXPATH];
    static char* ptrs[MAXPATH + 1]; 
    
    int len = strlen(str);
    int num = 0;
    int start = 0;
    
    for(int i = 0; i <= len; ++i) {
        // 遇到分隔符或字符串结束
        if(i == len || str[i] == delim) {
            if(i > start) {
                int copy_len = i - start;
                strncpy(res[num], str + start, copy_len);
                res[num][copy_len] = '\0';
                ptrs[num] = res[num];
                num++;
            }
            start = i + 1;
        }
    }
    strcpy(res[num], "/");
    ptrs[num] = res[num];
    
    return ptrs;
}

char* resolvePath(char *path, char *workPath) {
    // debugf("resolving path: %s, %s\n", path, workPath);
    // path like '/%'
    if(path[0] == '/')
        return path;

    static char nowWorkPath[MAXPATH];
    strcpy(nowWorkPath, workPath);

    char **r = strtok(path, '/');
    int i = 0;

    // cd ./../areka/.. -> cd ..
    while((r[i][0] != '/' || r[i][1] != '\0') && i < MAXPATH) {
        if(strcmp(r[i], "..") == 0) {
            int len = strlen(nowWorkPath);
            if(len == 1 && nowWorkPath[0] == '/') {
                i++;
                continue;
            }
            if(nowWorkPath[len - 1] == '/') {
                nowWorkPath[len - 1] = '\0';
            }
            for(int i = len - 1; i >= 0; --i) {
                if(nowWorkPath[i] == '/') {
                    nowWorkPath[i] = (i == 0) ? '/' : '\0';
                    nowWorkPath[i + 1] = '\0';
                    break;
                }
            }
        }
        else if(strcmp(r[i], ".") == 0) {
            // cd .
        }
        else {
            int len = strlen(nowWorkPath);
            if(nowWorkPath[len - 1] != '/')
                strcat(nowWorkPath, "/");
            strcat(nowWorkPath, r[i]);
        }
        i++;
    }

    // debugf("workPath now: %s\n", nowWorkPath);

    return nowWorkPath;
}

int chdir_shell(const char *path, char **argv) {
    // absolute path right now
    int r;
    struct Stat st;
 
    if ((r = stat(path, &st)) < 0) {
        printf("cd: The directory '%s' does not exist\n", argv[1]);
        return 1;
    }

	if(st.st_isdir) {
        syscall_env_chdir(syscall_getenvid(), path);
		return 0;
	}
    else {
		printf("cd: '%s' is not a directory\n", argv[1]);
		return 1;
	}
}

int cd_shell(int argc, char **argv) {
    char workPath[MAX_PATH];
    syscall_env_getpwd(syscall_getenvid(), workPath);

    if(argc > 2) {
        printf("Too many args for cd command\n");
        return 1;
    }
    char absolutePath[MAX_PATH];
    strcpy(absolutePath, argc == 1 ? "/" : argv[1]);
    char *path = resolvePath(absolutePath, workPath);
    return chdir_shell(path, argv);
}

int pwd_shell(int argc, char **argv) {
    char workPath[MAX_PATH];
    syscall_env_getpwd(syscall_getenvid(), workPath);

    if(argc > 1) {
        printf("pwd: expected 0 arguments; got %d\n", argc);
        return 2;
    }
    printf("%s\n", workPath);
    return 0;
}