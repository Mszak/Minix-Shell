#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "config.h"
#include "read_utils.h"
#include "siparse.h"

#include <unistd.h>

static char line_buffer[2 * MAX_LINE_LENGTH];
static char helper_buffer[2 * MAX_LINE_LENGTH];
static int bytes_in_buffer;

void clean_stdin() {
	char trash_input = -1;
	int bytes_read = -1;
	while(trash_input != LINE_FEED && bytes_read) {
		bytes_read = read(STDIN_FILENO, &trash_input, 1);
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

int read_command() {
	int total_bytes_read = bytes_in_buffer;
	int bytes_read = 0;
	int last_read_byte_pos = 0;
	while (true) {
		int end_of_line = find_end_of_line(helper_buffer, last_read_byte_pos, total_bytes_read);

		if (end_of_line == -1) {
			if (total_bytes_read > MAX_LINE_LENGTH) {
				bytes_in_buffer = 0;
				if (helper_buffer[bytes_read - 1]) {
					clean_stdin();
				}
				fprintf(stderr, "%s\n", SYNTAX_ERROR_STR);
				fflush(stderr);

				return 0;
			}
		}
		else {
			bytes_in_buffer = total_bytes_read - end_of_line - 1;
			if (end_of_line > MAX_LINE_LENGTH) {
				fprintf(stderr, "%s\n", SYNTAX_ERROR_STR);
				fflush(stderr);
				bytes_read = 0;
			}
			else {
				prepare_line_buffer(line_buffer, helper_buffer, end_of_line);
				bytes_read = end_of_line;
			}
			clean_helepr_buffer(helper_buffer, end_of_line + 1, bytes_in_buffer);
			return bytes_read;
		}

		bytes_read = read(STDIN_FILENO, helper_buffer + total_bytes_read, MAX_LINE_LENGTH + 1);
		if (bytes_read == 0) {
			was_end_of_file = true;
			return 0;
		}
		else if (bytes_read == -1) {
			bytes_read = 0;
			continue;
		}

		last_read_byte_pos = total_bytes_read;
		total_bytes_read += bytes_read;
	}
}

line* parse_command(const int bytes_read) {
	if (bytes_read == 0) {
		return NULL;
	}

	line* ln = parseline(line_buffer);
	if (ln == NULL) {
		fprintf(stderr, "%s\n", SYNTAX_ERROR_STR);
		fflush(stderr);
		return NULL;
	}
	else {
		return ln;
	}
}

void clean_buffers() {
	parsed_line = NULL;
	was_end_of_file = false;
}

void init_buffers() {
	parsed_line = NULL;
	was_end_of_file = false;
	bytes_in_buffer = 0;
}