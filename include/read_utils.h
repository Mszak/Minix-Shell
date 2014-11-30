#ifndef _READ_UTILS_H_
#define _READ_UTILS_H_

#include "siparse.h"

line* parsed_line;
int was_end_of_file;

int read_command();
line* parse_command(const int bytes_read);
void clean_buffers();
void init_buffers();

#endif /* !_READ_UTILS_H_ */
