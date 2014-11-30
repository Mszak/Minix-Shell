#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>

#include "signal_utils.h"

static pid_t background_processes[MAX_CHILDREN_SIZE];
static int background_status[MAX_CHILDREN_SIZE];
static volatile int background_counter;

static struct sigaction def_sigint;
static struct sigaction def_sigchld;
static struct sigaction shell_sigint;
static struct sigaction shell_sigchld;

static sigset_t chld_block_mask;

int is_background_child(pid_t pid) {
	for (int i = 0; i < foregorund_processes_size; ++i) {
		if (foreground_processes[i] == pid) {
			return false;
		}
	}

	return true;
}

void shell_chld_handler(int sig_nb) {
	pid_t child_pid;
	int child_status;

	while ((child_pid = waitpid(-1, &child_status, WNOHANG)) > 0) {
		if (is_background_child(child_pid)) {
			background_status[background_counter] = child_status;
			background_processes[background_counter++] = child_pid;
		}
		else {
			--foreground_counter;
		}
	}
}

void set_signal_handers() {
	shell_sigint.sa_handler = SIG_IGN;
	shell_sigchld.sa_handler = shell_chld_handler;
	shell_sigchld.sa_flags = SA_RESTART | SA_NOCLDSTOP;

	sigprocmask(SIG_BLOCK, NULL, &wait_mask);
	sigaction(SIGINT, &shell_sigint, &def_sigint);
	sigfillset(&shell_sigchld.sa_mask);
	sigaction(SIGCHLD, &shell_sigchld, &def_sigchld);
}

void write_background_info() {
	for (int i = 0; i < background_counter; ++i) {
		char *reason = WIFSIGNALED(background_status[i]) ? KILLED : TERMINATED; 
		int number = WIFSIGNALED(background_status[i]) ? WTERMSIG(background_status[i]) : background_status[i];

		fprintf(stdout, "Background process %d terminated. (%s %d)\n", 
			background_processes[i], reason, number);
	}
	background_counter = 0;
}

void restore_default_settings() {
	sigaction(SIGINT, &def_sigint, NULL);
	sigaction(SIGCHLD, &def_sigchld, NULL);
	unblock_chld_sig();
}

void block_chld_sig() {
	sigemptyset(&chld_block_mask);
	sigaddset(&chld_block_mask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &chld_block_mask, NULL);
}

void unblock_chld_sig() {
	sigprocmask(SIG_UNBLOCK, &chld_block_mask, NULL);
}