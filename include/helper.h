/**
 * helper.h - Simple helper functions
 */

#ifndef __HELPER_H__
#define __HELPER_H__

#include "csapp.h"

#define MAXFILE (500 << 10)
#define MAXSTR 1024

int str_start_with(char *str, char *tar) {
	while (*str == *tar && *tar != '\0') {
		++str, ++tar;
	}
	return *tar == '\0';
}

int str_end_with(char *str, char *tar) {
	size_t str_len, tar_len;
	char *str_end = str, *tar_end = tar;

	while (*str_end != '\0') ++str_end;
	while (*tar_end != '\0') ++tar_end;
	str_len = str_end - str;
	tar_len = tar_end - str;

	if (str_len < tar_len) return 0;

	// str_len >= tar_len, so no need to check str <= str_end
	while (*str_end == *tar_end && tar <= tar_end) {
		--str_end, --tar_end;
	}

	return tar_end < tar;
}

// Return 1 if success, 0 if error
int decode_url(char *url) {
	int val;
	char *l, *r;

	l = r = url;
	while (*r != '\0') {
		if (*r != '%') {
			*l = *r;
			++l, ++r;
			continue;
		}

		val = 0;
		for (int i = 1; i <= 2; ++i) {
			if (r[i] == '\0') return 0;
			val = val * 16 + r[i];
			if ('0' <= r[i] && r[i] <= '9') val -= '0';
			else if ('a' <= r[i] && r[i] <= 'z') val -= 'a' - 10;
			else if ('A' <= r[i] && r[i] <= 'Z') val -= 'A' - 10;
			else return 0;
		}
		// printf("val = %d\n", val);

		*l = (char)val;
		++l, r += 3;
	}

	*l = *r;
	return 1;
}

int Decode_url(char *url) {
	if (!decode_url(url)) {
		unix_error("Decode_url error");
	}
	return 1;
}

void copy_str_until(char *dest, char *src, char end) {
	char *p, *q;
	p = dest, q = src;
	while (*q != '\0' && *q != end) {
		*p = *q;
		++p, ++q;
	}
	*p = '\0';
}

char *str_after(char *src, char *tar) {
	char *p, *q;

	if (src == NULL || tar == NULL) return NULL;

	for (p = src, q = tar; *p == *q && *q != '\0'; ++p, ++q);

	return (*q == '\0') ? p : NULL;
}

char *find_str_after(char *src, char *tar) {
	return str_after(strstr(src, tar), tar);
}

#endif	// __HELPER_H__
