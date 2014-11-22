#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "config.h"
#include "siparse.h"
#include "utils.h"
#include "constants.h"
#include "builtins.h"

#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

pid_t children_processes[MAX_LINE_LENGTH];
int child_counter;

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

line* parse_command(char* buffer, const int bytes_read) {
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
		return parsed_lined;
	}
}

void shutdown_pipe(int* fd) {
	if (fd != NULL) {
		close(fd[0]);
		close(fd[1]);
		free(fd);
	}
}

void find_process_redirections(redirection * redirs[], redirection** in_redir, redirection** out_redir) {
	for (int i = 0; redirs[i] != NULL; ++i) {
		if (IS_RIN(redirs[i]->flags)) {
			*in_redir = redirs[i];
		}
		if (IS_ROUT(redirs[i]->flags) || IS_RAPPEND(redirs[i]->flags)) {
			*out_redir = redirs[i];
		}
	}	
}

void handle_open_file_error(const char* filename) {
	char* error = EMPTY_STR;
	if (errno == EACCES) {
		error = PERMISSION_DENIED;
	}
	else if (errno == ENOENT) {
		error = NO_SUCH_FILE_OR_DIR;
	}

	fprintf(stderr, "%s: %s\n", filename, error);
	fflush(stderr);
}

void redirect_pipes(int* read_pipe, int* write_pipe) {
	if (read_pipe != NULL) {
		close(read_pipe[1]);
		dup2(read_pipe[0], STDIN);
		close(read_pipe[0]);
	}
	if (write_pipe != NULL) {
		close(write_pipe[0]);
		dup2(write_pipe[1], STDOUT);
		close(write_pipe[1]);
	}
}

void execute_command(const command* com, int* lfd, int* rfd) {
	if (com == NULL || *(com->argv) == NULL) {
		return;
	}

	char* program = *(com->argv);
	char **arguments = com->argv;
	struct redirection **redirs = com->redirs;

	int shell_command_index = is_shell_command(program);
	if (shell_command_index > -1) {
		int return_code = execute_shell_command(shell_command_index, arguments);
		if (return_code == BUILTIN_ERROR) {
			fprintf(stderr, "Builtin %s error.\n", program);
			fflush(stderr);
		}
		return;
	}
	
	pid_t child_pid = fork();

	if (child_pid == -1) {
		exit(EXEC_FAILURE);
	}
	else if (child_pid == 0) {
		redirect_pipes(lfd, rfd);

		struct redirection *in_redir = NULL;
		struct redirection *out_redir = NULL;
		find_process_redirections(redirs, &in_redir, &out_redir);

		if (in_redir != NULL) {
			int in_fd = open(in_redir->filename, O_RDONLY);

			if (in_fd == -1) {
				handle_open_file_error(in_redir->filename);
				exit(EXEC_FAILURE);
			}
			dup2(in_fd, STDIN);
			close(in_fd);
		}
		if (out_redir != NULL) {
			int out_flags = O_WRONLY | O_CREAT;
			if (IS_RAPPEND(out_redir->flags)) {
				out_flags |= O_APPEND;
			}
			else {
				out_flags |= O_TRUNC;
			}
			int out_fd = open(out_redir->filename, out_flags, S_IRUSR | S_IWUSR);

			if (out_fd == -1) {
				handle_open_file_error(out_redir->filename);
				exit(EXEC_FAILURE);
			}
			dup2(out_fd, STDOUT);
			close(out_fd);
		}

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
		children_processes[child_counter++] = child_pid;
	}
}

int is_pipeline_invalid(pipeline* p) {
	for (command** c = *p + 1; *c; ++c) {
		if ((*c)->argv[0] == NULL) {
			return true;
		}
	}
	return false;
}

void handle_pipeline(pipeline* p) {
	if (is_pipeline_invalid(p)) {
		fprintf(stderr, "%s\n", SYNTAX_ERROR_STR);
		fflush(stderr);
	}
	else {
		int* left = NULL;
		int* right = NULL;
		child_counter = 0;
		for (command** c = *p; *c; ++c) {
			shutdown_pipe(left);
			left = right;
			if (*(c+1) != NULL) {
				right = (int*) malloc(2);
				pipe(right);
			}
			else {
				right = NULL;
			}
			execute_command(*c, left, right);
		}
		shutdown_pipe(left);

		for (int i = 0; i < child_counter; ++i) {
			waitpid(children_processes[i], NULL, 0);
		}
	}
}

void handle_pipeline_seq(line* ln) {
	if (ln == NULL) {
		return;
	}

	for (pipeline* p = ln->pipelines; *p; ++p) {
		handle_pipeline(p);
	}
}

void write_prompt() {
	struct stat stdout_stat;
	if (fstat(STDOUT, &stdout_stat) >= 0 && S_ISCHR(stdout_stat.st_mode)) {
		fprintf(stdout, PROMPT_STR);
		fflush(stdout);
	}
}

void clean_buffers(line* ln, int* was_eof) {
	ln = NULL;
	*was_eof = false;
}

int main(int argc, char *argv[]) {
	char line_buffer[MAX_LINE_LENGTH * 2];
	char read_helper_buffer[2 * MAX_LINE_LENGTH];
	line* parsed_line = NULL;
	int was_end_of_file = false;
	int bytes_in_buffer = 0;

	while(!was_end_of_file) {
		clean_buffers(parsed_line, &was_end_of_file);
		write_prompt();
		int bytes_read = read_command(&was_end_of_file, line_buffer, 
			read_helper_buffer, &bytes_in_buffer);
		parsed_line = parse_command(line_buffer, bytes_read);
		handle_pipeline_seq(parsed_line);
	}

	return 0;
}
