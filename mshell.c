#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "config.h"
#include "siparse.h"
#include "builtins.h"
#include "redirect_utils.h"
#include "signal_utils.h"
#include "read_utils.h"
#include "pipe_utils.h"

#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

void execute_command(const command* com, int* lfd, int* rfd, int is_background) {
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
		if (is_background) {
			setsid();
		}

		restore_default_settings();
		redirect_pipes(lfd, rfd);
		redirect_in_out(redirs);

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
		foreground_processes[foreground_counter++] = child_pid;
		++foregorund_processes_size;
	}
}

void write_prompt() {
	struct stat stdout_stat;
	if (fstat(STDOUT_FILENO, &stdout_stat) >= 0 && S_ISCHR(stdout_stat.st_mode)) {
		block_chld_sig();

		write_background_info();
		fprintf(stdout, PROMPT_STR);
		fflush(stdout);

		unblock_chld_sig();
	}
}

void init() {
	init_buffers();
	set_signal_handers();
}

int main(int argc, char *argv[]) {
	init();

	while(!was_end_of_file) {
		clean_buffers();
		write_prompt();
		int bytes_read = read_command();
		parsed_line = parse_command(bytes_read);
		handle_pipeline_seq();
	}

	return 0;
}
