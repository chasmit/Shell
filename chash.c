/*
 Charlie Smith - CS283 - Assignment 6
 A little clunky. Support stdin/stdout redir, command sequences, pipe commands
 Does not support multiple pipe commands :(
 */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define SIZE 128
#define FOREGROUND 1
#define BACKGROUND 2
#define CD 3
#define REDIR_IN 4
#define REDIR_OUT 5
#define REDIR_BOTH 6
#define REDIR_BOTHAPP 7
#define APPEND 8
#define SEQUENTIAL 9 
#define CONCURRENT 10
#define PIPED 11

void tokprint(char**);
void change_dir(char**);
char* set_delimiter(int);
int set_type(char*, int);
void exec_cmd(char**, int);
int exec_multiple(char**, int, int); 
int exec_redir(char**, int);
int exec_pipe(char**, int);

//main, also does initial tokenizing based on command type
int
main(int argc, char *argv[])
{
	char buf[SIZE];
	char *tokens[SIZE];
	char *p, *delimiter, *last;
	int i, type, position, cmdcount, pid, status;

	printf("Launching shell...\n");

	while (1) {
		printf("# ");
		i = 0;
		
		if (fgets(buf, SIZE, stdin) == NULL) {
			break;
		}

		if (feof(stdin)) {
			printf("Exiting shell...\n");
			break;
		}

		type = set_type(buf, FOREGROUND);
		
		//if condition for & ; or | commands(shorthand)
		if (type >= 9) {
			delimiter = set_delimiter(type);

			p = strtok(buf, delimiter);
		      	while (p) {
				tokens[i] = p;
				i++;
				p = strtok(NULL, delimiter);
			}
			cmdcount = i;
		//tokenize single commands
		} else {
			p = strtok(buf, " \n");	
			while(p) {
				tokens[i] = p;
				last = tokens[i];
				p = strtok(NULL, " \n");
				i++;
			}
		
			position = i-1;	
			tokens[i] = NULL;
		}
		
		//check for change directory before forking
		//else, determine from tokens how the command should be treated	
		if (strcmp(tokens[0], "cd") == 0) {
			type = CD;
			change_dir(tokens);
		}  else if (type <= 8) 
				exec_cmd(tokens, type);
		else if (type <= 10) 
				exec_multiple(tokens, type, cmdcount);
		else
				exec_pipe(tokens, cmdcount);
	}
	exit(0);
}

//used for bugfixing
void
tokprint(char** tokens)
{
	int i;
	i = 0;
	while (tokens[i]) {
		printf("Token: %s\n", tokens[i]);
        	i++;
	}

}

//built-in directory change within the shell process
void
change_dir(char** tokens) 
{
	char *newdir;
	newdir = tokens[1];
	
	if (chdir(newdir) == -1) 
		printf("Cannot find directory\n");
	else 
		printf("Working directory changed to %s\n", newdir);
	
}

//tokenizing helper function
char*
set_delimiter(int type) {
	if (type == SEQUENTIAL)
		return ";";
	else if (type == CONCURRENT)
		return "&";
	else if (type == PIPED)
		return "|";
}

/* goes through tokens and determines what type of command it is
   assumes single foreground upon call
   APPEND is identified after tokenizing because its easier
*/ 
int
set_type(char* buf, int type)
{
	int i;
	for (i = 0; i < strlen(buf); i++) {
		switch (buf[i]) {
			case '<':
				if (type >= 9)
					break;
				else if (type == REDIR_OUT) 
					type = REDIR_BOTH;
				else 
					type = REDIR_IN;
				break;
			case '>':
				if (type >= 9)
					break;
				else if (type == REDIR_IN) 
					type = REDIR_BOTH;
				else 
					type = REDIR_OUT;
				break;
			case ';':
				type = SEQUENTIAL;
				break;
			case '&':
				type = CONCURRENT;
				break;
			case '|':
				type = PIPED;
				break;
		}
		if (type == REDIR_BOTH)
			break;
	}
	return type;
}

//execute single command
void
exec_cmd(char** tokens, int type)
{
	int pid, status;

	pid = fork();
	if (pid < 0) {
		perror("Child forking failed");
		exit(1);
	} else if (pid == 0) {
		switch (type) {
			case FOREGROUND:
				if ((execvp(*tokens, tokens)) < 0)
					perror("Command not recognized");
				break;
			case REDIR_IN: case REDIR_OUT: case REDIR_BOTH: case APPEND:
				exec_redir(tokens, type);
				break;
			default:
				break;
		}
	} else 
		wait(&status);
}

//execute multiple commands
//note that the tokens variable from main is renamed cmd here. easier for me to understand
int
exec_multiple(char** cmd, int type, int cmdcount)
{
	int i, arrcount;
       	int pid, chpid;
	int status;
	char* tokens[SIZE];

	i = 0;
	arrcount = 0;

	pid = fork();
	if (pid < 0) {
		printf("Child fork failed\n");
		exit(1);
	//child
	} else if (pid == 0) {
		while (i < cmdcount) {
			type = FOREGROUND;
			arrcount = 0;
			tokens[0] = strtok(cmd[i], " \n");
			while (tokens[arrcount]) {
				arrcount++;
				tokens[arrcount] = strtok(NULL, " \n");
			}
			
			chpid = fork();
			if (chpid < 0) {
				printf("Grandchild forking failed\n");
				exit(2);
			} else if (chpid == 0) {
				if ((execvp(*tokens, tokens)) < 0) { 
					perror("Command not recognized");
				}
				exit(0);
			} else {
				//waits for return for sequential, not for concurrent
				if (type == SEQUENTIAL) 
					wait(&status);
			}
			i++;
		}
	//parent
	} else {
		wait(&status);
	}
	return(0);
}

int
exec_redir(char** tokens, int type)
{
	int i;
	int pid, status; 
	int fdread, fdwrite;
	int inpos, outpos;
	char *cmd[SIZE];
	char *in, *out;

	i = 0;
	//get redirected input/output files
	while (tokens[i]) {
		if (strcmp(tokens[i], ">") == 0 || (strcmp(tokens[i], ">>") == 0)) {
			if (strcmp(tokens[i], ">>") == 0) {
				if (type == REDIR_OUT) {
					type = APPEND;
				} else
					type = REDIR_BOTHAPP;
			}
			out = tokens[i+1];
			outpos = i;
		} else if (strcmp(tokens[i], "<") == 0) {
			in = tokens[i+1];
			inpos = i;
		} else {
			cmd[i] = tokens[i];
		}
		i++;
	}
	cmd[i+1] = '\0';
	
	pid = fork();
	if (pid < 0) {
		printf("Child fork failed\n");
                exit(1);
        //child
        } else if (pid == 0) {
		switch (type) {
			case REDIR_IN:
				fdread = open(in, O_RDONLY);
				if (fdread < 0) {
					perror("error opening file\n");
					exit(3);
				}
				dup2(fdread, 0);
				close(fdread);				
				if ((execvp(*cmd, cmd)) < 0) 
					perror("Command not recognized\n");
				break;
			case REDIR_OUT: case APPEND:	
				if (type == REDIR_OUT)
					fdwrite = open(out, O_CREAT|O_WRONLY|O_TRUNC, 0755);
				else
					fdwrite = open(out, O_CREAT|O_WRONLY|O_APPEND, 0755);
				dup2(fdwrite, 1);
                                close(fdwrite);
                                if ((execvp(*cmd, cmd)) < 0)
                                        perror("Command not recognized");
                                break;
			case REDIR_BOTH: case REDIR_BOTHAPP:
				fdread = open(in, O_RDONLY);
				if (type == REDIR_BOTH)
					fdwrite = open(out, O_CREAT|O_WRONLY|O_TRUNC, 0755);
				else
					fdwrite = open(out, O_CREAT|O_WRONLY|O_APPEND, 0755);
				dup2(fdread, 0);
				close(fdread);
				dup2(fdwrite, 1);
				close(fdwrite);
				if ((execvp(*cmd, cmd) < 0))
					perror("Command not recognized");
				break;
		}	
	} else {
                wait(&status);
        }

	return(0);
}

int
exec_pipe(char** cmd, int cmdcount)
{
	int i, arrcount, status;
	int pid, chpid;
	int fd[2];
	char *proc1[SIZE], *proc2[SIZE];

	i = 0;
	
	pid = fork();	
	if (pid < 0) {
                printf("Child fork failed\n");
                exit(1);
	} else if (pid == 0) {
		arrcount = 0;
		proc1[0] = strtok(cmd[i], " \n");
		while (proc1[arrcount]) {
			arrcount++;
			proc1[arrcount] = strtok(NULL, " \n");
		}
		arrcount = 0;
		i++;
		
		proc2[0] = strtok(cmd[i], " \n");
		while (proc2[arrcount]) {
			arrcount++;
			proc2[arrcount] = strtok(NULL, " \n");
		}
		
		pipe(fd);
		chpid = fork();
		if (chpid < 0) {
			perror("Grandchild forking failed");
			exit(2);	
		//grandchild writes to child
		} else if (chpid == 0) {
			dup2(fd[1], 1);
			close(fd[0]);
			close(fd[1]);
			if ((execvp(*proc1, proc1) < 0))
				perror("Command not recognized");
		//child reads from grandchild
		} else {
			dup2(fd[0], 0);
			close(fd[0]);
			close(fd[1]);
			if ((execvp(*proc2, proc2) < 0))
				perror("Command not recognized");
		}	
	} else {
		wait(&status);
	}
	return(0);
}
