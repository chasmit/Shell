/*
 Charlie Smith - CS283 - Assignment 5
 Currently supported: running foreground commands, background commands, cd commands
 There's some things that don't do much right now; they are included in anticipation of part two
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define SIZE 128
#define FOREGROUND 1
#define BACKGROUND 2
#define CD 3

void change_dir(char**);

int
main()
{
	char buf[SIZE];
	char *tokens[SIZE];
	char cd[3];
	char *p;
	char *last;
	pid_t pid;
	int status;
	int type;
	int position;
	int i;

	printf("Launching shell...\n");

	while(1) {
		type = 1;
		i = 0;

		printf("# ");	
		if(fgets(buf, SIZE, stdin) == NULL) {
			break;
		}

		if (feof(stdin)) {
			printf("Exiting shell...\n");
			break;
		}

        	type = FOREGROUND;
        	p = strtok(buf, " \n");

		while(p) {
			tokens[i] = p;
			last = tokens[i];
			p = strtok(NULL, " \n");
			i++;
		}
		
		position = i-1;
        	tokens[i] = NULL;
		
		//check for change directory before forking	
		if(strcmp(tokens[0], "cd") == 0) {
			type = CD;
		} else if(strcmp(last, "&") == 0) {
			type = BACKGROUND;
		}

		if (type == CD) {
			change_dir(tokens);
			continue;
		} else {
			pid = fork();
		
			if (pid < 0) {
				printf("Forking failed\n");
				exit(1);
			} else if (pid == 0) {
				if(type == BACKGROUND) {
					tokens[position] = '\0';
				}
				execvp(tokens[0], tokens);
				exit(0);			
			} else if (pid == 0 && type != BACKGROUND) {
				waitpid(pid, &status, 0);
			} else {
				waitpid(pid, &status, 0);
			}
		}
	}
	exit(0);
}

void
change_dir(char** tokens)
{
	char *cwd;
	char *newdir;

	cwd = getcwd(cwd, SIZE);
	newdir = tokens[1];
	printf("%s\n", tokens[1]);
	
	if (chdir(newdir) == -1) {
		printf("Cannot find directory\n");
	} else {
		printf("Directory changed to %s\n", newdir);
	}
}

