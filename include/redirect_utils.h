#ifndef _REDIRECT_UTILS_H_
#define _REDIRECT_UTILS_H_

#include "siparse.h"

void find_process_redirections(redirection * redirs[], redirection** in_redir, redirection** out_redir);
void redirect_pipes(int* read_pipe, int* write_pipe);
void redirect_in_out(struct redirection **redirs);

#endif /* !_REDIRECT_UTILS_H_ */
