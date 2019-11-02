/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef LIBDSET_UTILS_H
#define LIBDSET_UTILS_H

#include <string.h>				/* strcmp */
#include <netinet/in.h>				/* struct in[6]_addr */
#ifndef IPPROTO_UDPLITE
#define IPPROTO_UDPLITE		136
#endif

/* String equality tests */
#define STREQ(a, b)		(strcmp(a, b) == 0)
#define STRNEQ(a, b, n)		(strncmp(a, b, n) == 0)
#define STRCASEQ(a, b)		(strcasecmp(a, b) == 0)
#define STRNCASEQ(a, b, n)	(strncasecmp(a, b, n) == 0)

/* Stringify tokens */
#define _STR(c)			#c
#define STR(c)			_STR(c)

/* Min/max */
#define MIN(a, b)		(a < b ? a : b)
#define MAX(a, b)		(a > b ? a : b)

#define UNUSED			__attribute__ ((unused))
#ifdef NDEBUG
#define ASSERT_UNUSED		UNUSED
#else
#define ASSERT_UNUSED
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x)		(sizeof(x) / sizeof(*(x)))
#endif

#endif	/* LIBDSET_UTILS_H */
