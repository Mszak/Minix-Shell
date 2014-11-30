#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/param.h>
#include <signal.h>

#include "builtins.h"
#include "config.h"

int echo(char*[]);
int lexit(char*[]);
int cd(char*[]);
int ls(char*[]);
int lkill(char*[]);
int undefined(char*[]);

builtin_pair builtins_table[]={
	{"exit",	&lexit},
	{"lecho",	&echo},
	{"lcd",		&cd},
	{"lkill",	&lkill},
	{"lls",		&ls},
	{NULL,NULL}
};

int
is_shell_command(char const* program) {
	for (int index = 0; builtins_table[index].name != NULL; ++index) {
		if (strcmp(program, builtins_table[index].name) == 0) {
			return index;
		}
	}
	return -1;
}

int
execute_shell_command(int command_index, char** argv) {
	return builtins_table[command_index].fun(argv);
}

int
get_signal(char const* str) {
	int length = strlen(str);
	char num[length];
	if (str[0] != '-') {
		return -1;
	}
	int sig = atoi(str + 1);
	sprintf(num, "%d", sig);
	if (strcmp(num, str + 1) != 0) {
		return -1;
	}
	return sig;
}

int
lkill(char * argv[]) {
	char* signal_number = NULL;
	char* pid_number;
	if (argv[1] == NULL) {
		return BUILTIN_ERROR;
	}
	else if (argv[2] == NULL) {
		pid_number = argv[1];
	}
	else {
		if (argv[3] != NULL) {
			return BUILTIN_ERROR;
		}
		signal_number = argv[1];
		pid_number = argv[2];
	}

	int sig;
	if (signal_number != NULL) {
		sig = get_signal(signal_number);
		if (sig < 0) {
			return BUILTIN_ERROR;
		}
	}
	else {
		sig = SIGTERM;
	}

	int pid = atoi(pid_number);
	char str[strlen(pid_number)];
	sprintf(str, "%d", pid);
	if (strcmp(str, pid_number) != 0) {
		return BUILTIN_ERROR;	
	}

	if (kill(pid, sig) == -1) {
		return BUILTIN_ERROR;
	}

	return 0;
}

int
lexit(char * argv[]) {
	exit(0);
}

void
print_directory(struct dirent * entry) {
	if (strncmp(entry->d_name, ".", 1) != 0) {
		fprintf(stdout, "%s\n", entry->d_name);
		fflush(stdout);		
	}
}

int
ls(char * argv[]) {
	char* additional_argument = argv[1];
	if (additional_argument == NULL) {
		char path_name[PATH_MAX];
		if (getcwd(path_name, PATH_MAX) == NULL) {
			return BUILTIN_ERROR;
		}
		DIR* files = opendir(path_name);
		if (files == NULL) {
			return BUILTIN_ERROR;
		}
		struct dirent* entry;
		while ((entry = readdir(files)) != NULL) {
			print_directory(entry);
		}
		closedir(files);

		return 0;
	}
	return BUILTIN_ERROR;
}

int 
echo(char * argv[])
{
	int i =1;
	if (argv[i]) printf("%s", argv[i++]);
	while  (argv[i])
		printf(" %s", argv[i++]);

	printf("\n");
	fflush(stdout);
	return 0;
}

int
cd(char * argv[]) {
	char* target_path = argv[1];
	if (target_path == NULL) {
		char* home_directory = getenv(HOME);
		int return_code = chdir(home_directory);
		if (return_code == -1) {
			return BUILTIN_ERROR;
		}
	}
	else {
		char* additional_argument = argv[2];
		if (additional_argument == NULL) {
			int return_code = chdir(target_path);
			if (return_code == -1) {
				return BUILTIN_ERROR;
			}		
		}
		else {
			return BUILTIN_ERROR;
		}
	}

	return 0;
}

int 
undefined(char * argv[])
{
	fprintf(stderr, "Command %s undefined.\n", argv[0]);
	return BUILTIN_ERROR;
}
