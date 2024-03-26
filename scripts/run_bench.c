#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <sys/wait.h>

long mttm_register_pid(pid_t pid)
{
	return syscall(449, pid);
}

long mttm_unregister_pid(pid_t pid)
{
	return syscall(450, pid);
}

int main(int argc, char** argv)
{
	pid_t pid;
	int state;

	if (argc < 2) {
		printf("Usage: ./run_bench [BENCHMARK]\n");
		return 0;
	}

	mttm_register_pid(getpid());
	printf("pid : %d registered, name : %s\n",getpid(), argv[1]);

	pid = fork();
	if (pid == 0) {
		execvp(argv[1], &argv[1]);
		printf("Fail to run bench\n");
		exit(-1);
	}

	waitpid(pid, &state, 0);
	mttm_unregister_pid(getpid());
	printf("pid : %d unregistered, name : %s\n",getpid(), argv[1]);

	return 0;
}
