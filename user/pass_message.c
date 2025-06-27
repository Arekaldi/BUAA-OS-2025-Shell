#include <lib.h>
#include <cd.h>

u_int num2str(char *buf, u_int num) {
    char *p = buf;
    if (num == 0) {
        *p++ = '0';
    } else {
        u_int n = num;
        while (n > 0) {
            n /= 10;
            p++;
        }
        *p = '\0';
        while (num > 0) {
            *(--p) = '0' + (num % 10);
            num /= 10;
        }
    }
    return 0;
}

u_int str2num(char *buf, u_int *num) {
    u_int n = 0;
    while (*buf) {
        if (*buf < '0' || *buf > '9') {
            return -1; // Invalid character
        }
        n = n * 10 + (*buf - '0');
        buf++;
    }
    *num = n;
    return 0; // Success
}

u_int get_f_envid(int argc, char **argv) {
    for(int i = 0; i < argc; ++i) {
        if(i + 1 < argc && strcmp(argv[i + 1], "areka") == 0) {
            u_int envid;
            if(str2num(argv[i], &envid) == 0) {
                return envid; // Return the found envid
            } else {
                user_panic("Invalid envid format");
            }
        }
    }
    user_panic("No envid found in arguments");
    return -1;
}

u_int get_f_dir(const char *nowPath, char *f_dir) {
    char tempPath[MAX_PATH];
    strncpy(tempPath, nowPath, MAX_PATH);
    int len = strlen(tempPath);
    if(len == 1 && tempPath[0] == '/') {
        strcpy(f_dir, "/");
        return 0;
    }
    if(tempPath[len - 1] == '/') {
        tempPath[len - 1] = '\0';
    }
    for(int i = len - 1; i >= 0; --i) {
        if(tempPath[i] == '/') {
            strncpy(f_dir, tempPath, i + 1);
            f_dir[i + 1] = '\0';
            return 0;
        }
    }
    return -1;
}