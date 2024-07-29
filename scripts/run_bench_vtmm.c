#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <sys/wait.h>
#include <sys/time.h>

#define tv_to_double(t) (t.tv_sec + (t.tv_usec / 1000000.0))

void timeDiff(struct timeval *d, struct timeval *a, struct timeval *b)
{
  d->tv_sec = a->tv_sec - b->tv_sec;
  d->tv_usec = a->tv_usec - b->tv_usec;
  if (d->tv_usec < 0) {
    d->tv_sec -= 1;
    d->tv_usec += 1000000;
  }
}

double elapsed(struct timeval *starttime, struct timeval *endtime)
{
  struct timeval diff;

  timeDiff(&diff, endtime, starttime);
  return tv_to_double(diff);
}

long vtmm_register_pid(pid_t pid, const char *name)
{
	return syscall(451, pid, name);
}

long vtmm_unregister_pid(pid_t pid)
{
	return syscall(452, pid);
}

int main(int argc, char** argv)
{
	pid_t pid;
	int state, i;
	struct timeval start, end;
	char clean_name[1024];

	if (argc < 2) {
		printf("Usage: ./run_bench [BENCHMARK]\n");
		return 0;
	}

	for(i = strlen(argv[1]); i >= 0; i--) {
		if(argv[1][i] == '/') {
			strncpy(clean_name, &argv[1][i+1], strlen(argv[1]) - i + 1);
			break;
		}
		if(i == 0)
			strncpy(clean_name, argv[1], strlen(argv[1]) + 1);
	}

	gettimeofday(&start, NULL);
	vtmm_register_pid(getpid(), clean_name);
	printf("pid : %d registered, name : %s\n",getpid(), clean_name);

	pid = fork();
	if (pid == 0) {
		execvp(argv[1], &argv[1]);
		printf("Fail to run bench\n");
		exit(-1);
	}

	waitpid(pid, &state, 0);
	vtmm_unregister_pid(getpid());
	gettimeofday(&end, NULL);
	printf("pid : %d unregistered, name : %s, total time : %f s\n",getpid(), clean_name, elapsed(&start, &end));

	return 0;
}
