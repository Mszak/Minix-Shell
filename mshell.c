#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "config.h"
#include "siparse.h"
#include "utils.h"
#include "constants.h"

#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define STDIN 0
#define STDOUT 1
#define STDERR 2

void clean_stdin(char last_sign) { //TODO clean to eof
	if (last_sign != LINE_FEED) {
		char trash_input;
		while(1) {
			read(STDIN, &trash_input, 1);
			if (trash_input == LINE_FEED) {
				break;
			}
		}
	}
}

int find_end_of_line(char const * buffer, int index, int last_index) {
	while (index < last_index) {
		if (buffer[index] == LINE_FEED || buffer[index] == EOF) {
			return index;
		}
		++index;
	}
	return -1;
}

int check_if_end_of_file(char last_character) {
	return last_character == EOF;
}

void prepare_line_buffer(char* target, char* source, int buffer_length) {
	memcpy(target, source, sizeof(char) * buffer_length);
	target[buffer_length] = 0;
}

void clean_helepr_buffer(char* helper_buffer, int first, int length) {
	memmove(helper_buffer, helper_buffer + first, sizeof(char) * length);
}

int read_command(int *was_eof, char* buffer, char* helper_buffer, int *current_bytes_in_buffer) {
	int total_bytes_read = *current_bytes_in_buffer;
	int bytes_read = 0;
	int last_read_bytes = 0;
	while (1) {
		int end_of_line = find_end_of_line(helper_buffer, last_read_bytes, total_bytes_read);

		if (end_of_line == -1) {
			if (total_bytes_read > MAX_LINE_LENGTH) {
				clean_stdin(helper_buffer[bytes_read - 1]);
				fprintf(stderr, "%s\n", SYNTAX_ERROR_STR);
				// write(STDERR, SYNTAX_ERROR_STR, sizeof(SYNTAX_ERROR_STR) -1);
				// write(STDERR, NEW_LINE, sizeof(NEW_LINE) -1);

				*current_bytes_in_buffer = 0;

				return 0;
			}
		}
		else {
			*was_eof = check_if_end_of_file(helper_buffer[total_bytes_read - 1]);
			*current_bytes_in_buffer = total_bytes_read - end_of_line - 1;
			if (end_of_line > MAX_LINE_LENGTH) {
				clean_helepr_buffer(helper_buffer, end_of_line + 1, total_bytes_read - end_of_line - 1);
				fprintf(stderr, "%s\n", SYNTAX_ERROR_STR);
				// write(STDERR, SYNTAX_ERROR_STR, sizeof(SYNTAX_ERROR_STR) -1);
				// write(STDERR, NEW_LINE, sizeof(NEW_LINE) -1);
				return 0;
			}
			else {
				prepare_line_buffer(buffer, helper_buffer, end_of_line);
				clean_helepr_buffer(helper_buffer, end_of_line + 1, total_bytes_read - end_of_line - 1);
				return end_of_line;
			}
		}

		bytes_read = read(STDIN, helper_buffer + total_bytes_read, MAX_LINE_LENGTH + 1);
		if (bytes_read == 0) {
			*was_eof = 1;
			return 0;
		}
		last_read_bytes = total_bytes_read;
		total_bytes_read += bytes_read;
	}
}

command* parse_command(char* buffer, const int bytes_read) {
	if (buffer == NULL || bytes_read == 0) {
		return NULL;
	}

	line* parsed_lined = parseline(buffer);
	if (parsed_lined == NULL) {
		fprintf(stderr, "%s\n", SYNTAX_ERROR_STR);
		// write(STDERR, SYNTAX_ERROR_STR, sizeof(SYNTAX_ERROR_STR) - 1);
		// write(STDERR, NEW_LINE, sizeof(NEW_LINE) - 1);
		return NULL;
	}
	else {
		// printf("NOT NULL \n");
		// printparsedline(parsed_lined);
		command* command_to_execute = pickfirstcommand(parsed_lined);
		return command_to_execute;
	}
}

int count_string_bytes(const char* str) {
	if (str == NULL) {
		return 0;
	}

	int bytes = 0, index = 0;
	while(str[index++]) {
		++bytes;
	}

	return bytes * sizeof(char); 
}

void execute_command(const command* com) {
	if (com == NULL || *(com->argv) == NULL) {
		return;
	}

	char* program = *(com->argv);
	// printf("[%s]\n", program);
	char **arguments = com->argv;
	int program_name_bytes = count_string_bytes(program);
	
	pid_t child_pid = fork();

	if (child_pid == -1) {
		exit(EXEC_FAILURE);
	}
	else if (child_pid == 0) {
		if (execvp(program, arguments) < 0) {
			if (errno == EACCES || errno == EPERM) {
				fprintf(stderr, "%s: %s\n", program, PERMISSION_DENIED);
				// write(STDERR, program, program_name_bytes);
				// write(STDERR, PERMISSION_DENIED, sizeof(PERMISSION_DENIED) - 1);
			}
			else if (errno == ENOENT) {
				fprintf(stderr, "%s: %s\n", program, NO_SUCH_FILE_OR_DIR);
				// write(STDERR, program, program_name_bytes);
				// write(STDERR, NO_SUCH_FILE_OR_DIR, sizeof(NO_SUCH_FILE_OR_DIR) - 1);
			}
			else {
				fprintf(stderr, "%s: %s\n", program, EXEC_ERROR);
				// write(STDERR, program, program_name_bytes);
				// write(STDERR, EXEC_ERROR, sizeof(EXEC_ERROR) - 1);
			}
			exit(EXEC_FAILURE);		
		}
	}
	else {
		waitpid(child_pid, NULL, 0);
	}
}

void write_prompt() {
	struct stat stdout_stat;
	if (fstat(STDOUT, &stdout_stat) >= 0 && S_ISCHR(stdout_stat.st_mode)) {
		printf(PROMPT_STR);
		// write(STDOUT, PROMPT_STR, sizeof(PROMPT_STR));
	}
}

void clean_buffers(command* com, int* was_eof) {
	com = NULL;
	*was_eof = 0;
}

int main(int argc, char *argv[]) {
	char line_buffer[2 * MAX_LINE_LENGTH];
	char read_helper_buffer[2 * MAX_LINE_LENGTH];
	command* command_to_execute = NULL;
	int was_end_of_file = 0;
	int bytes_in_buffer = 0;


	while(!was_end_of_file) {
		clean_buffers(command_to_execute, &was_end_of_file);
		write_prompt();
		int bytes_read = read_command(&was_end_of_file, line_buffer, 
			read_helper_buffer, &bytes_in_buffer);
		command_to_execute = parse_command(line_buffer, bytes_read);
		execute_command(command_to_execute);
	}

	return 0;
}
