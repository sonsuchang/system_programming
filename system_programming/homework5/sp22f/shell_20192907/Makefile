

all: fork shell myshell

fork: fork.c
	gcc -o fork fork.c

shell: shell.c
	gcc -o shell shell.c

myshell: myshell.c
	gcc -o myshell myshell.c -lreadline

clean:
	rm fork shell myshell
