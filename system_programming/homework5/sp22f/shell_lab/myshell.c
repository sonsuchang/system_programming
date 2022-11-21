#include<stdio.h>
#include<sys/types.h>
#include<unistd.h>
#include<errno.h>
#include<string.h>
#include<stdlib.h>
#include<sys/wait.h>
#include<readline/readline.h>
#include<readline/history.h>

int main(int argc, char* argv[])
{
	pid_t pid, child_status;
    	char* cmdline;
    	char* opt;
	while(1){
		char* cmdarr[100];
		int i = 0;
		cmdline = readline("$ ");
		opt = strtok(cmdline, " \n");
		if(!strcmp(opt, "quit")){
			break;
		}
		cmdarr[i] = opt;
		while(opt != NULL){
			i += 1;
			opt = strtok(NULL, " \n");
			cmdarr[i] = opt;
		}
		pid = fork();

		if (pid == 0){	
			
			if (execvp(cmdarr[0], &cmdarr[0]) == -1){
				fprintf(stderr, "%s\n", strerror(errno));
				return -1;
			}

			else{
				execvp(cmdarr[0], &cmdarr[0]);
				exit(0);
			}
		}

		else{
			wait(NULL);
		}
	}
    	return 0;
}

