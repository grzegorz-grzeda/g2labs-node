/* SPDX-License-Identifier: MIT */
/*
 * Host side of the publish dump: built into the native simulator runner, so it
 * can use the host's stdio. The fake ports call it with one JSON line per
 * publish. The file lands in the test's working directory, which under twister
 * is the test's build directory in the twister output.
 */
#include <stdio.h>

#define PUBLISH_DUMP_FILE "contract_publishes.jsonl"

void publish_dump_bottom_write_line(const char *line)
{
	static FILE *file;

	if (file == NULL) {
		file = fopen(PUBLISH_DUMP_FILE, "w");
		if (file == NULL) {
			perror("fake ports: can't open " PUBLISH_DUMP_FILE);
			return;
		}
	}
	fputs(line, file);
	fputc('\n', file);
	fflush(file);
}
