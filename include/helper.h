/**
 * helper.h - Simple helper functions
 */

#ifndef __HELPER_H__
#define __HELPER_H__

#include "csapp.h"

int str_start_with(char *str, char *tar) {
	while (*str == *tar && *tar != '\0') {
		++str, ++tar;
	}
	return *tar == '\0';
}

#endif	// __HELPER_H__
