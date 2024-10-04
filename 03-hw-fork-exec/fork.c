#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/wait.h>

int main(int argc, char *argv[]) {
	int pid;
	FILE *fptr;

	fptr = fopen("fork-output.txt", "w");

	fprintf(fptr, "BEFORE FORK (%d)\n", fileno(fptr));
	fflush(fptr);

	printf("Starting program; process has pid %d\n", getpid());

	if ((pid = fork()) < 0) {
		fprintf(stderr, "Could not fork()");
		exit(1);
	}

	/* BEGIN SECTION A */

	printf("Section A;  pid %d\n", getpid());
	// sleep(5);

	/* END SECTION A */
	if (pid == 0) {
		/* BEGIN SECTION B */
		sleep(5);
		fprintf(fptr, "SECTION B (%d)\n", fileno(fptr));

		// printf("Section B\n");
		// sleep(30);
		// sleep(30);
		// printf("Section B done sleeping\n");

		exit(0);

		/* END SECTION B */
	} else {
		/* BEGIN SECTION C */

		fprintf(fptr, "SECTION C (%d)\n", fileno(fptr));
		fclose(fptr);
		sleep(5);

		// printf("Section C\n");
		// sleep(30);
		// printf("Section C done sleeping\n");
		
		// wait(NULL);

		exit(0);

		/* END SECTION C */
	}
	/* BEGIN SECTION D */

	printf("Section D\n");
	// sleep(30);

	/* END SECTION D */
}

