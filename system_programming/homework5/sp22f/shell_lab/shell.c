#include<stdio.h>
#include<sys/types.h>
#include<unistd.h>
#include<errno.h>
#include<sys/wait.h>
#include<string.h>
#include<stdlib.h>

int main(int argc, char* argv[])
{
	pid_t pid;
	int child_status;
	errno = 0;
	printf("processing ...\n");
	pid = fork();
	if (pid == 0){
		if (execvp(argv[1], &argv[1]) == -1){
			fprintf(stderr, "%s\n", strerror(errno));
			exit(0);
		}
	
		else{
			execvp(argv[1], &argv[1]);
			exit(0);
		}
	}

	else{
		wait(&child_status);
		printf("complete.\n");
	}
	return 0;
}
