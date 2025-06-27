#include <cd.h>
#include <lib.h>

int flag[256];

void lsdir(char *, char *);
void ls1(char *, u_int, u_int, char *);

void ls(char *path, char *prefix) {
	int r;
	struct Stat st;

	if ((r = stat(path, &st)) < 0) {
		user_panic("stat %s: %d", path, r);
	}
	if (st.st_isdir && !flag['d']) {
		lsdir(path, prefix);
	} else {
		ls1(0, st.st_isdir, st.st_size, path);
	}
}

void lsdir(char *path, char *prefix) {
	int fd, n;
	struct File f;

	if ((fd = open(path, O_RDONLY)) < 0) {
		user_panic("open %s: %d", path, fd);
	}
	while ((n = readn(fd, &f, sizeof f)) == sizeof f) {
		if (f.f_name[0]) {
			ls1(prefix, f.f_type == FTYPE_DIR, f.f_size, f.f_name);
		}
	}
	if (n > 0) {
		user_panic("short read in directory %s", path);
	}
	if (n < 0) {
		user_panic("error reading directory %s: %d", path, n);
	}
}

void ls1(char *prefix, u_int isdir, u_int size, char *name) {
	char *sep;

	if (flag['l']) {
		printf("%11d %c ", size, isdir ? 'd' : '-');
	}
	if (prefix) {
		if (prefix[0] && prefix[strlen(prefix) - 1] != '/') {
			sep = "/";
		} else {
			sep = "";
		}
		printf("%s%s", prefix, sep);
	}
	printf("%s", name);
	if (flag['F'] && isdir) {
		printf("/");
	}
	printf(" ");
}

void usage(u_int f_envid) {
	printf("usage: ls [-dFl] [file...]\n");
	exit_my(-1, f_envid);
}

int main(int argc, char **argv) {
	int i;

	int f_envid = get_f_envid(argc, argv);

	ARGBEGIN {
		default:
			usage(f_envid);
		case 'd':
		case 'F':
		case 'l':
			flag[(u_char)ARGC()]++;
			break;
	}
	ARGEND

	char workPath[MAX_PATH];
	syscall_env_getpwd(syscall_getenvid(), workPath);

	for(int i = 0; i < argc; ++i) {
		if(strcmp(argv[i], "areka") == 0) {
			argc = i - 1;
			break;
		}
	}

	if (argc == 0) {
		ls(workPath, "");
	} else {
		for (i = 0; i < argc; i++) {
			//TODO
			char *path = resolvePath(argv[i], workPath);
			ls(path, path);
		}
	}
	printf("\n");

	exit_my(0, f_envid);

	return 0;
}
