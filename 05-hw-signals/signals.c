#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<signal.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<errno.h>

#define MAXPIDLEN 10

int foo;
int block;

// print 1, wait 4 seconds, print 2
void sig_handler1(int signum) {
	printf("1\n"); fflush(stdout);
	sleep(4);
	printf("2\n"); fflush(stdout);
}

//print 8, call sig_handler1, wait 4 seconds, print 9
void sig_handler2(int signum) {
	printf("8\n"); fflush(stdout);
	kill(getpid(), SIGINT);
	sleep(4);
	printf("9\n"); fflush(stdout);
}

//print foo
void sig_handler3(int signum) {
	printf("%d\n", foo); fflush(stdout);
}

//if foo is bigger than 0, print 6
void sig_handler4(int signum) {
	if (foo > 0) {
		foo = 6;
	}
}

//set foo to fork(), if foo is 0, print 7
void sig_handler5(int signum) {
	foo = fork();
	if (foo == 0) {
		exit(7);
	}
}

//pid will wait and reap the child process, if waitpid fails, print current value of errno
void sig_handler6(int signum) {
	int pid, status;
	pid = waitpid(-1, &status, WNOHANG);
	if (pid < 0) {
		printf("%d\n", errno); fflush(stdout);
	}
}

//toggle block to be 0 if false, else 1
void sig_handler7(int signum) {
	if (block) {
		block = 0;
	} else {
		block = 1;
	}
}

//reset the handler for SIGTERM to default action (SIG_DFL)
void sig_handler8(int signum) {
	struct sigaction sigact;

	sigact.sa_flags = SA_RESTART;
	sigact.sa_handler = SIG_DFL;
	sigaction(SIGTERM, &sigact, NULL);
}

//wait for child process to terminate and prints its exit status
void sig_handler9(int signum) {
	int status;
	waitpid(-1, &status, 0);
	printf("%d\n", WEXITSTATUS(status)); fflush(stdout);
}

void install_sig_handlers() {
	struct sigaction sigact;

	// zero out flags
	sigact.sa_flags = SA_RESTART;

	sigact.sa_handler = sig_handler1;
	sigaction(SIGHUP, &sigact, NULL);

	sigact.sa_handler = sig_handler1;
	sigaction(SIGINT, &sigact, NULL);

	sigact.sa_handler = sig_handler2;
	sigaction(SIGQUIT, &sigact, NULL);

	sigact.sa_handler = sig_handler3;
	sigaction(SIGTERM, &sigact, NULL);

	// SIGUSR1 and SIGUSR2
	sigact.sa_handler = sig_handler4;
	sigaction(30, &sigact, NULL);

	sigact.sa_handler = sig_handler5;
	sigaction(10, &sigact, NULL);

	sigact.sa_handler = sig_handler6;
	sigaction(16, &sigact, NULL);

	sigact.sa_handler = sig_handler7;
	sigaction(31, &sigact, NULL);

	sigact.sa_handler = sig_handler8;
	sigaction(12, &sigact, NULL);

	sigact.sa_handler = sig_handler9;
	sigaction(SIGCHLD, &sigact, NULL);
}

void sleep_block_loop() {
	int i;
	sigset_t mask;

	for (i = 0; i < 20; i++) {
		sigemptyset(&mask);
		if (block) {
			sigaddset(&mask, SIGINT);
			sigaddset(&mask, SIGCHLD);
		}
		sigprocmask(SIG_SETMASK, &mask, NULL);
		sleep(1);
	}
	printf("25\n"); fflush(stdout);
	exit(0);
}

void start_killer(int pid, char *cmd, char *scenario_num) {
	char *args[4];
	char *env[] = { NULL };
	char pidstr[32];

	sprintf(pidstr, "%d", pid);
	args[0] = cmd;
	args[1] = scenario_num;
	args[2] = pidstr;
	args[3] = NULL;
	if (execve(args[0], &args[0], env) < 0) {
		perror("execve");
		exit(1);
	}
}

int main(int argc, char *argv[]) {
	int pid;

	if (argc < 3) {
		fprintf(stderr, "Usage: %s <killer> [0-9]\n", argv[0]);
		exit(1);
	}

	foo = -1;
	block = 0;

	install_sig_handlers();

	pid = fork();
	if (pid < 0) {
		perror("fork");
		exit(1);
	}

	if (pid > 0) {
		start_killer(pid, argv[1], argv[2]);
	} else {
		sleep_block_loop();
	}
}
