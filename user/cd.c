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
    
    // 分割字符串
    int len = strlen(str);
    int num = 0, lst = 0;
    for(int i = 0; i < len; ++i) {
        if((i + 1 == len || str[i] == delim) && i != lst) {
            int copy_len = i - lst + (i + 1 == len);
            
            strncpy(res[num], str + lst + !(i != 0 && lst == 0), copy_len);
            res[num][copy_len] = '\0';
            ptrs[num] = res[num];
            num++;
            lst = i;
        }
    }
    
    // 结束标记
    strcpy(res[num], "/");
    ptrs[num] = res[num];
    
    return ptrs;
}

char* resolvePath(char *path, char *workPath) {
    // path like '/%'
    if(path[0] == '/')
        return path;

    static char nowWorkPath[MAXPATH];
    strcpy(nowWorkPath, workPath);

    char **r = strtok(path, '/');
    int i = 0;

    // cd ./../areka/.. -> cd ..
    while((r[i][0] != '/' || r[i][1] != '\0') && i < MAXPATH) {
        // 父目录
        if(strcmp(r[i], "..") == 0) {
            if(strcmp(nowWorkPath, "/") != 0) {
                // 去掉最后一个 '/'
                int len = strlen(nowWorkPath);
                len--;
                for(; nowWorkPath[len] != '/'; --len);
                nowWorkPath[len + 1] = '\0';
            }
        }
        else if(strcmp(r[i], ".") != 0) {
            int len = strlen(nowWorkPath);
            if(nowWorkPath[len - 1] != '/')
                strcat(nowWorkPath, "/");
            strcat(nowWorkPath, r[i]);
        }
        else {
            // cd .
        }
        i++;
    }

    strcat(nowWorkPath, "/");

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

    debugf("cd_shell: current work path is '%s'\n", workPath);

    if(argc > 2) {
        printf("Too many args for cd command\n");
        return 1;
    }
    else if(argc == 1) {
        argc = 2;
        argv[1] = "/";
    }
    char *path = resolvePath(argv[1], workPath);
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