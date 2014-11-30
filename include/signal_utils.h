#ifndef _SIGNAL_UTILS_H_
#define _SIGNAL_UTILS_H_

#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include "config.h"

pid_t foreground_processes[MAX_CHILDREN_SIZE];
volatile int foreground_counter;
volatile int foregorund_processes_size;
sigset_t wait_mask;

void set_signal_handers();
void write_background_info();
void restore_default_settings();
void block_chld_sig();
void unblock_chld_sig();

#endif /* !_SIGNAL_UTILS_H_ */
