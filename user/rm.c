#include<lib.h>
#include <cd.h>

void usage_rm(u_int f_envid) {
    printf("usage: rm [-rf] <dir>\n");
    exit_my(-1, f_envid);
}


int main(int argc, char **argv) {

    char workPath[MAX_PATH];
    syscall_env_getpwd(syscall_getenvid(), workPath);

    u_int f_envid = get_f_envid(argc, argv);

    if(f_envid == -1) {
        user_panic("touch: no envid found in arguments");
        exit();
    }

    u_int continuing = 0, force = 0;
	ARGBEGIN {
        case 'r':
            continuing = 1;
            break;
	    case 'f':
            force = 1;
            break;
        default:
            usage_rm(f_envid);
            break;
    }
    ARGEND

	
	int r;
    struct Stat st;
    
    char *path = resolvePath(argv[0], workPath);

	int err = 0;

    if ((r = stat(path, &st)) < 0) {
        if(!force) {
            printf("rm: cannot remove '%s': No such file or directory\n", argv[0]);
            err = 1;
        }
    } else {
        if(st.st_isdir && !continuing) {
            printf("rm: cannot remove '%s': Is a directory\n", argv[0]);
            err = 1;
        } else {
            remove(path);
        }
	}

	return 0;
}


