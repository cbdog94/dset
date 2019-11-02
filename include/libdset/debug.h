/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef LIBDSET_DEBUG_H
#define LIBDSET_DEBUG_H

#ifdef DSET_DEBUG
#include <stdio.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#define D(fmt, args...) \
	fprintf(stderr, "%s: %s: " fmt "\n", __FILE__, __func__, ##args)
#define IF_D(test, fmt, args...) \
	if (test)                    \
	D(fmt, ##args)

static inline void
dump_nla(struct nlattr *nla[], int maxlen)
{
	int i;
	for (i = 0; i < maxlen; i++)
		D("nla[%u] does%s exist", i, nla[i] ? "" : " NOT");
}
#else
#define D(fmt, args...)
#define IF_D(test, fmt, args...)
#define dump_nla(nla, maxlen)
#endif

#endif /* LIBDSET_DEBUG_H */
