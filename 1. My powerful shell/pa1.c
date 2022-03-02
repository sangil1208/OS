/**********************************************************************
 * Copyright (c) 2021
 *  Sang-Hoon Kim <sanghoonkim@ajou.ac.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTIABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 **********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>

#include <string.h>

#include "types.h"
#include "list_head.h"
#include "parser.h"
#include "sys/wait.h"

# define SIZE_LIMIT 80

struct entry
{
	struct list_head list;
	char *string;
};

/***********************************************************************
 * struct list_head history
 *
 * DESCRIPTION
 *   Use this list_head to store unlimited command history.
 */
LIST_HEAD(history);


/***********************************************************************
 * run_command()
 *
 * DESCRIPTION
 *   Implement the specified shell features here using the parsed
 *   command tokens.
 *
 * RETURN VALUE
 *   Return 1 on successful command execution
 *   Return 0 when user inputs "exit"
 *   Return <0 on error
 */
static int run_command(int nr_tokens, char *tokens[])
{	
	//check if pipe character is included in command
	int pipe_index = 0;
	for (int i=0; i< nr_tokens; i++){
		if (strcmp(tokens[i], "|") == 0){
			pipe_index = i;
			break;
		}
	}

	//implementation of executing pipe included command
	if (pipe_index != 0){
		// Divide the command into front and back of the pipe.
		char *front[MAX_NR_TOKENS] = {NULL};                                  
		char *rear[MAX_NR_TOKENS] = {NULL};

		for (int i=0; i<pipe_index; i++){
			front[i] = tokens[i];
			rear[i] = tokens[pipe_index + i +1];
		}

		//executing pipe included command
		int fd[2];
		pipe(fd);
		pid_t pid = fork();
		int status = 0;

		if (pid > 0){
			pid_t pid2 = fork();
			int status2 = 0;
			if (pid2 > 0){
				close(fd[0]);
				close(fd[1]);
				wait(&status2);
			}
			else if (pid2 < 0){
				fprintf(stderr, "fork error");
				return -1;
			}
			else{
				close(fd[1]);
				dup2(fd[0], STDIN_FILENO);
				close(fd[0]);
				if(execvp(rear[0], rear) < 0){
					fprintf(stderr, "Unable to execute %s\n", rear[0]);
					exit(0);
				}
			}
			wait(&status);
			if (status == -1)
				return -1;

		}

		else if (pid < 0){
			fprintf(stderr, "fork error");
			return -1;
		}
		
		else{
			close(fd[0]);
			dup2(fd[1], STDOUT_FILENO);
			close(fd[1]);
			if(execvp(front[0], front) < 0){
				fprintf(stderr, "Unable to execute %s\n", front[0]);
				exit(0);
			}

		}
		return 1;
	}

	// implementation of built-in command
	if (strcmp(tokens[0], "exit") == 0) return 0;

	if (strcmp(tokens[0], "cd") == 0)
	{
		if (nr_tokens == 1 || strcmp(tokens[1], "~") == 0)
		{
			chdir(getenv("HOME"));
		}
		else
			chdir(tokens[1]);
		return 1;
	}

	if (strcmp(tokens[0], "history") == 0)
	{
		struct entry *cur;
		int i = 0;
		if (!list_empty(&history))
		{
			list_for_each_entry_reverse(cur, &history, list)
				fprintf(stderr, "%2d: %s", i++, cur->string);
		}
		return 1;
	}

	if (strcmp(tokens[0], "!") == 0)
	{
		int index = strtol(tokens[1], NULL, 10);
		struct entry *cur;
		int i = 0;
		if (!list_empty(&history))
		{
			list_for_each_entry_reverse(cur, &history, list)
			{
				if (i == index){
					char *tokens_t[MAX_NR_TOKENS] = { NULL };
					int nr_tokens_t = 0;
					char command[SIZE_LIMIT] = {""};
					strcpy(command, cur->string);
					if (parse_command(command, &nr_tokens_t, tokens_t) == 0) return 1;

					run_command(nr_tokens_t, tokens_t);
				}
				i++;
			}
		}
		return 1;
	}

	// implementation of executing external executables
	int status = 0;
	pid_t pid = fork();

	if (pid > 0)
	{
		wait(&status);
		if (status == -1)
			return -1;
		return 1;
	}

	else if (pid < 0)
	{
		fprintf(stderr, "fork error");
		return -1;
	}

	else
	{	

		if (execvp(tokens[0], tokens) < 0)
		{
			fprintf(stderr, "Unable to execute %s\n", tokens[0]);
			exit(0);
		}

		return 1;
	}
}


/***********************************************************************
 * append_history()
 *
 * DESCRIPTION
 *   Append @command into the history. The appended command can be later
 *   recalled with "!" built-in command
 */
static void append_history(char *const command)
{
	struct entry *new = (struct entry *)malloc(sizeof(struct entry));
	//set size limit of each command, If the command exceeds the limit, cut it.
	if (strlen(command) > SIZE_LIMIT){
		new->string = malloc(sizeof(char) * (SIZE_LIMIT + 2));
		strncpy(new->string, command, SIZE_LIMIT);
		strcat(new->string, "\n");
		list_add(&new->list, &history);
		return;
	}
	new->string = malloc(sizeof(char) * (strlen(command) + 1));
	strcpy(new->string, command);
	list_add(&new->list, &history);
}


/***********************************************************************
 * initialize()
 *
 * DESCRIPTION
 *   Call-back function for your own initialization code. It is OK to
 *   leave blank if you don't need any initialization.
 *
 * RETURN VALUE
 *   Return 0 on successful initialization.
 *   Return other value on error, which leads the program to exit.
 */
static int initialize(int argc, char * const argv[])
{
	return 0;
}


/***********************************************************************
 * finalize()
 *
 * DESCRIPTION
 *   Callback function for finalizing your code. Like @initialize(),
 *   you may leave this function blank.
 */
static void finalize(int argc, char * const argv[])
{	
	// free memory at the end of main
	if(!list_empty(&history)){
		struct entry* temp = list_first_entry(&history, struct entry, list);
		list_del(&temp->list);
		free(temp->string);
		free(temp);
	}
}


/*====================================================================*/
/*          ****** DO NOT MODIFY ANYTHING BELOW THIS LINE ******      */
/*          ****** BUT YOU MAY CALL SOME IF YOU WANT TO.. ******      */
static int __process_command(char * command)
{
	char *tokens[MAX_NR_TOKENS] = { NULL };
	int nr_tokens = 0;

	if (parse_command(command, &nr_tokens, tokens) == 0)
		return 1;

	return run_command(nr_tokens, tokens);
}

static bool __verbose = true;
static const char *__color_start = "[0;31;40m";
static const char *__color_end = "[0m";

static void __print_prompt(void)
{
	char *prompt = "$";
	if (!__verbose) return;

	fprintf(stderr, "%s%s%s ", __color_start, prompt, __color_end);
}

/***********************************************************************
 * main() of this program.
 */
int main(int argc, char * const argv[])
{
	char command[MAX_COMMAND_LEN] = { '\0' };
	int ret = 0;
	int opt;

	while ((opt = getopt(argc, argv, "qm")) != -1) {
		switch (opt) {
		case 'q':
			__verbose = false;
			break;
		case 'm':
			__color_start = __color_end = "\0";
			break;
		}
	}

	if ((ret = initialize(argc, argv))) return EXIT_FAILURE;

	/**
	 * Make stdin unbuffered to prevent ghost (buffered) inputs during
	 * abnormal exit after fork()
	 */
	setvbuf(stdin, NULL, _IONBF, 0);

	while (true) {
		__print_prompt();

		if (!fgets(command, sizeof(command), stdin)) break;

		append_history(command);
		ret = __process_command(command);

		if (!ret) break;
	}

	finalize(argc, argv);

	return EXIT_SUCCESS;
}

