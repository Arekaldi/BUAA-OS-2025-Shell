#include <lib.h>
#include <cd.h>

char buf[8192];

void cat(int f, char *s) {
	long n;
	int r;

	while ((n = read(f, buf, (long)sizeof buf)) > 0) {
		if ((r = write(1, buf, n)) != n) {
			user_panic("write error copying %s: %d", s, r);
		}
	}
	if (n < 0) {
		user_panic("error reading %s: %d", s, n);
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

	int f, i;

	if (argc == 3) {
		cat(0, "<stdin>");
	} else {
		for (i = 1; i < argc - 2; i++) {
			char *path = resolvePath(argv[i], workPath);
			f = open(path, O_RDONLY);
			if (f < 0) {
				user_panic("can't open %s: %d", argv[i], f);
			} else {
				cat(f, path);
				close(f);
			}
		}
	}
	return 0;
}
