#include<stdio.h>
#include<sys/types.h>
#include<unistd.h>

int main(int argc, char* argv[])
{

	pid_t pid; 
	pid = fork();
	if (pid > 0)
		printf("I am parent : child pid = %d\n", pid);
	else{
		printf("I am child : my pid = %d\n", getpid());
	}
	return 0;
}
