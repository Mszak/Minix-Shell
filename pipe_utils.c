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

void shutdown_pipe(int* fd) {
	if (fd != NULL) {
		close(fd[0]);
		close(fd[1]);
		free(fd);
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

void handle_pipeline(pipeline* p, const int is_background) {
	if (is_pipeline_invalid(p)) {
		fprintf(stderr, "%s\n", SYNTAX_ERROR_STR);
		fflush(stderr);
	}
	else {
		int* left = NULL;
		int* right = NULL;
		foreground_counter = 0;

		block_chld_sig();

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
			execute_command(*c, left, right, is_background);
		}
		shutdown_pipe(left);

		if (!is_background) {
			while (foreground_counter) {
				sigsuspend(&wait_mask);
			}
		}
		foregorund_processes_size = 0;

		unblock_chld_sig();
	}
}

void handle_pipeline_seq() {
	if (parsed_line == NULL) {
		return;
	}

	for (pipeline* p = parsed_line->pipelines; *p; ++p) {
		handle_pipeline(p, IS_BACKGROUND(parsed_line->flags));
	}
}