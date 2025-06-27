#include <env.h>
#include <lib.h>
void wait(u_int envid) {
	const volatile struct Env *e;

	e = &envs[ENVX(envid)];
	while (e->env_id == envid && e->env_status != ENV_FREE) {
		syscall_yield();
	}
}

void wait_my(u_int *value, u_int f_envid, u_int c_envid) {
	wait(c_envid);
	*value = syscall_get_child_message(f_envid);
}