#include <lib.h>
#include <cd.h>

void usage_mkdir(u_int f_envid) {
    printf("usage: mkdir [-p] <dir>\n");
    exit_my(-1, f_envid);
}

void mkdir_shell(const char *path, u_int continuing) {
    if(continuing) {
        if(my_file_create(path, 1) < 0) {
			char f_dir[MAX_PATH];
			if(get_f_dir(path, f_dir) < 0) {
				return;
			}
			mkdir_shell(f_dir, continuing);
			my_file_create(path, 1);
        }
    } else {
		my_file_create(path, 1);
    }
}

int main(int argc, char **argv) {
    char workPath[MAX_PATH];
    syscall_env_getpwd(syscall_getenvid(), workPath);

    u_int f_envid = get_f_envid(argc, argv);

    if(f_envid == -1) {
        user_panic("mkdir: no envid found in arguments");
        exit();
    }

    int ign_fault = 0;

	ARGBEGIN {
        default:
            usage_mkdir(f_envid);
        case 'p':
            ign_fault = 1;
            break;
        }
    ARGEND

    if(argc < 3) {
        usage_mkdir(f_envid);
    }

	int r;
    struct Stat st;
    char *path = resolvePath(argv[0], workPath);

	int err = 0;

    if ((r = stat(path, &st)) < 0) {
        char f_dir[MAX_PATH];
        get_f_dir(path, f_dir);
        if((r = stat(f_dir, &st) < 0)) {
            if(ign_fault) {
                mkdir_shell(path, 1);
            } else {
                printf("mkdir: cannot create directory '%s': No such file or directory\n", argv[0]);
                err = 1;
            }
        } else {
            mkdir_shell(path, 0);
        }
    } else {
        if(st.st_isdir && !ign_fault) {
            printf("mkdir: cannot create directory '%s': File exists\n", argv[0]);
            err = 1;
        } else if(st.st_isdir == 0) {
            mkdir_shell(path, 0);
        }
	}

    exit_my(err, f_envid);

	return 0;
}
