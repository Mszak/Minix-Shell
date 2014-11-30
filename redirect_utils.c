#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "redirect_utils.h"

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

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

void redirect_pipes(int* read_pipe, int* write_pipe) {
	if (read_pipe != NULL) {
		close(read_pipe[1]);
		dup2(read_pipe[0], STDIN_FILENO);
		close(read_pipe[0]);
	}
	if (write_pipe != NULL) {
		close(write_pipe[0]);
		dup2(write_pipe[1], STDOUT_FILENO);
		close(write_pipe[1]);
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

void redirect_in_out(struct redirection **redirs) {
	struct redirection *in_redir = NULL;
	struct redirection *out_redir = NULL;
	find_process_redirections(redirs, &in_redir, &out_redir);

	if (in_redir != NULL) {
		int in_fd = open(in_redir->filename, O_RDONLY);

		if (in_fd == -1) {
			handle_open_file_error(in_redir->filename);
			exit(EXEC_FAILURE);
		}
		dup2(in_fd, STDIN_FILENO);
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
		dup2(out_fd, STDOUT_FILENO);
		close(out_fd);
	}
}