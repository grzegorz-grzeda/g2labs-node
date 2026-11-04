/* SPDX-License-Identifier: MIT */
/* Small JSON writer and reader for the core's MQTT payloads. */
#include "json.h"

#include <stdio.h>
#include <string.h>

void json_putc(struct json_writer *w, char c)
{
	if (w->len + 1 < w->size) {
		w->buf[w->len++] = c;
		w->buf[w->len] = '\0';
	} else {
		w->overflow = true;
	}
}

void json_raw(struct json_writer *w, const char *s)
{
	while (*s != '\0') {
		json_putc(w, *s++);
	}
}

/* A JSON string: quotes, backslashes and control characters escaped. */
void json_string(struct json_writer *w, const char *s)
{
	static const char hex[] = "0123456789abcdef";

	json_putc(w, '"');
	for (; *s != '\0'; s++) {
		unsigned char c = (unsigned char)*s;

		if (c == '"' || c == '\\') {
			json_putc(w, '\\');
			json_putc(w, (char)c);
		} else if (c < 0x20) {
			json_raw(w, "\\u00");
			json_putc(w, hex[c >> 4]);
			json_putc(w, hex[c & 0x0f]);
		} else {
			json_putc(w, (char)c);
		}
	}
	json_putc(w, '"');
}

void json_key(struct json_writer *w, const char *key)
{
	if (w->has_member) {
		json_putc(w, ',');
	}
	w->has_member = true;
	json_string(w, key);
	json_putc(w, ':');
}

void json_unsigned(struct json_writer *w, unsigned long v)
{
	char num[24];

	snprintf(num, sizeof(num), "%lu", v);
	json_raw(w, num);
}

void json_int(struct json_writer *w, int v)
{
	char num[24];

	snprintf(num, sizeof(num), "%d", v);
	json_raw(w, num);
}

static bool json_skip_container(struct json_reader *r, char close, int depth);

void json_skip_space(struct json_reader *r)
{
	while (r->p < r->end && (*r->p == ' ' || *r->p == '\t' || *r->p == '\n' || *r->p == '\r')) {
		r->p++;
	}
}

/* Consume `c`, after any whitespace. */
bool json_expect(struct json_reader *r, char c)
{
	json_skip_space(r);
	if (r->p < r->end && *r->p == c) {
		r->p++;
		return true;
	}
	return false;
}

static int hex_value(char c)
{
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	}
	if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}
	return -1;
}

/*
 * Escapes that don't stand for a printable ASCII character decode to DEL,
 * which no command name or key contains, so such a string never matches one.
 */
bool json_read_string(struct json_reader *r, char *out)
{
	size_t n = 0;

	if (!json_expect(r, '"')) {
		return false;
	}
	while (r->p < r->end && *r->p != '"') {
		unsigned char c = (unsigned char)*r->p++;

		if (c < 0x20) {
			return false;
		}
		if (c == '\\') {
			if (r->p == r->end) {
				return false;
			}
			c = (unsigned char)*r->p++;
			switch (c) {
			case '"':
			case '\\':
			case '/':
				break;
			case 'b':
			case 'f':
			case 'n':
			case 'r':
			case 't':
				c = 0x7f;
				break;
			case 'u': {
				unsigned int v = 0;

				for (int i = 0; i < 4; i++) {
					int h = r->p < r->end ? hex_value(*r->p++) : -1;

					if (h < 0) {
						return false;
					}
					v = v << 4 | (unsigned int)h;
				}
				c = v >= 0x20 && v < 0x7f ? (unsigned char)v : 0x7f;
				break;
			}
			default:
				return false;
			}
		}
		if (out != NULL) {
			out[n++] = (char)c;
		}
	}
	if (out != NULL) {
		out[n] = '\0';
	}
	return json_expect(r, '"');
}

/* The members of an object or the items of an array, up to `close`. */
static bool json_skip_container(struct json_reader *r, char close, int depth)
{
	if (json_expect(r, close)) {
		return true;
	}
	do {
		if (close == '}' && (!json_read_string(r, NULL) || !json_expect(r, ':'))) {
			return false;
		}
		if (!json_skip_value(r, depth)) {
			return false;
		}
	} while (json_expect(r, ','));
	return json_expect(r, close);
}

static bool json_skip_literal(struct json_reader *r, const char *literal)
{
	size_t len = strlen(literal);

	if ((size_t)(r->end - r->p) < len || memcmp(r->p, literal, len) != 0) {
		return false;
	}
	r->p += len;
	return true;
}

/* Any value, checked only loosely: the Node ignores members it doesn't know. */
bool json_skip_value(struct json_reader *r, int depth)
{
	json_skip_space(r);
	if (r->p == r->end || depth > JSON_MAX_DEPTH) {
		return false;
	}
	switch (*r->p) {
	case '"':
		return json_read_string(r, NULL);
	case '{':
		r->p++;
		return json_skip_container(r, '}', depth + 1);
	case '[':
		r->p++;
		return json_skip_container(r, ']', depth + 1);
	case 't':
		return json_skip_literal(r, "true");
	case 'f':
		return json_skip_literal(r, "false");
	case 'n':
		return json_skip_literal(r, "null");
	default: {
		const char *start = r->p;

		while (r->p < r->end && strchr("+-.0123456789eE", *r->p) != NULL) {
			r->p++;
		}
		return r->p != start;
	}
	}
}
