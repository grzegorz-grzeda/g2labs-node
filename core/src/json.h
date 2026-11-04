/* SPDX-License-Identifier: MIT */
/*
 * Small JSON writer and reader for the core's MQTT payloads: the writer
 * builds Status, the reader takes commands apart. Private to the core.
 */
#ifndef NODE_CORE_JSON_H
#define NODE_CORE_JSON_H

#include <stdbool.h>
#include <stddef.h>

/* Appends to a fixed buffer; output past the end is dropped and flagged. */
struct json_writer {
	char *buf;
	size_t size;
	size_t len;
	bool overflow;
	/* A member has been written, so the next needs a comma. */
	bool has_member;
};

void json_putc(struct json_writer *w, char c);
void json_raw(struct json_writer *w, const char *s);
/* A JSON string: quotes, backslashes and control characters escaped. */
void json_string(struct json_writer *w, const char *s);
/* An object member's key, after a comma if it isn't the first. */
void json_key(struct json_writer *w, const char *key);
void json_unsigned(struct json_writer *w, unsigned long v);
void json_int(struct json_writer *w, int v);

/*
 * Reads JSON from a buffer that isn't NUL-terminated. Each function that
 * returns bool returns false on malformed input.
 */
struct json_reader {
	const char *p;
	const char *end;
};

/* Nesting allowed inside a value json_skip_value() skips. */
#define JSON_MAX_DEPTH 8

void json_skip_space(struct json_reader *r);
/* Consume `c`, after any whitespace. */
bool json_expect(struct json_reader *r, char c);
/*
 * A string, decoded into `out` (NUL-terminated; it must hold as many bytes
 * as remain in the input), or skipped when `out` is NULL. Escapes that don't
 * stand for a printable ASCII character decode to DEL (0x7f).
 */
bool json_read_string(struct json_reader *r, char *out);
/* Any value, checked only loosely; `depth` is the current nesting. */
bool json_skip_value(struct json_reader *r, int depth);

#endif /* NODE_CORE_JSON_H */
