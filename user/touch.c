#include<lib.h>
#include <cd.h>

void usage_touch(u_int f_envid) {
    printf("usage: touch [-p] <dir>\n");
    exit_my(-1, f_envid);
}

void touch_shell(const char *path) {
	my_file_create(path, 0);
}

int main(int argc, char **argv) {

    char workPath[MAX_PATH];
    syscall_env_getpwd(syscall_getenvid(), workPath);

    u_int f_envid = get_f_envid(argc, argv);

    if(f_envid == -1) {
        user_panic("touch: no envid found in arguments");
        exit();
    }

	ARGBEGIN {
        default:
            usage_touch(f_envid);
            break;
    }
    ARGEND

	int r;
    struct Stat st;
    
    char *path = resolvePath(argv[0], workPath);

	int err = 0;

    if ((r = stat(path, &st)) < 0) {
		char f_dir[MAX_PATH];
        get_f_dir(path, f_dir);

		if((r = stat(f_dir, &st) < 0)) {
			printf("touch: cannot touch '%s': No such file or directory\n", argv[0]);
			err = 1;
		} else {
			touch_shell(path);
		}
    } else {
		if(st.st_isdir == 0) {
			touch_shell(path);
		}
	}

    exit_my(err, f_envid);

	return 0;
}

