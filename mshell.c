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

void clean_stdin() {
	char trash_input = -1;
	int bytes_read = -1;
	while(trash_input != LINE_FEED && bytes_read) {
		bytes_read = read(STDIN, &trash_input, 1);
	}
}

int find_end_of_line(char const * buffer, const int first_index, const int last_index) {
	for (int index = first_index; index < last_index; ++index) {
		if (buffer[index] == LINE_FEED) {
			return index;
		}
	}

	return -1;
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
	int last_read_byte_pos = 0;
	while (true) {
		int end_of_line = find_end_of_line(helper_buffer, last_read_byte_pos, total_bytes_read);

		if (end_of_line == -1) {
			if (total_bytes_read > MAX_LINE_LENGTH) {
				*current_bytes_in_buffer = 0;
				if (helper_buffer[bytes_read - 1]) {
					clean_stdin();
				}
				fprintf(stderr, "%s\n", SYNTAX_ERROR_STR);
				fflush(stderr);

				return 0;
			}
		}
		else {
			*current_bytes_in_buffer = total_bytes_read - end_of_line - 1;
			if (end_of_line > MAX_LINE_LENGTH) {
				fprintf(stderr, "%s\n", SYNTAX_ERROR_STR);
				fflush(stderr);
				bytes_read = 0;
			}
			else {
				prepare_line_buffer(buffer, helper_buffer, end_of_line);
				bytes_read = end_of_line;
			}
			clean_helepr_buffer(helper_buffer, end_of_line + 1, *current_bytes_in_buffer);
			return bytes_read;
		}

		bytes_read = read(STDIN, helper_buffer + total_bytes_read, MAX_LINE_LENGTH + 1);
		if (bytes_read == 0) {
			*was_eof = true;
			return 0;
		}
		last_read_byte_pos = total_bytes_read;
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
		fflush(stderr);
		return NULL;
	}
	else {
		command* command_to_execute = pickfirstcommand(parsed_lined);
		return command_to_execute;
	}
}

void execute_command(const command* com) {
	if (com == NULL || *(com->argv) == NULL) {
		return;
	}

	char* program = *(com->argv);
	char **arguments = com->argv;
	
	pid_t child_pid = fork();

	if (child_pid == -1) {
		exit(EXEC_FAILURE);
	}
	else if (child_pid == 0) {
		if (execvp(program, arguments) < 0) {
			char* error_occured = NULL;
			if (errno == EACCES || errno == EPERM) {
				error_occured = PERMISSION_DENIED;
			}
			else if (errno == ENOENT) {
				error_occured = NO_SUCH_FILE_OR_DIR;
			}
			else {
				error_occured = EXEC_ERROR;
			}
			fprintf(stderr, "%s: %s\n", program, error_occured);
			fflush(stderr);
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
		fprintf(stdout, PROMPT_STR);
		fflush(stdout);
	}
}

void clean_buffers(command* com, int* was_eof) {
	com = NULL;
	*was_eof = false;
}

int main(int argc, char *argv[]) {
	char line_buffer[MAX_LINE_LENGTH * 2];
	char read_helper_buffer[2 * MAX_LINE_LENGTH];
	command* command_to_execute = NULL;
	int was_end_of_file = false;
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
