/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef LIBDSET_ERRCODE_H
#define LIBDSET_ERRCODE_H

#include <libdset/linux_domain_set.h>		/* enum dset_cmd */

struct dset_session;

/* Kernel error code to message table */
struct dset_errcode_table {
	int errcode;		/* error code returned by the kernel */
	enum dset_cmd cmd;	/* issued command */
	const char *message;	/* error message the code translated to */
};

#ifdef __cplusplus
extern "C" {
#endif

extern int dset_errcode(struct dset_session *session, enum dset_cmd cmd,
			 int errcode);

#ifdef __cplusplus
}
#endif

#endif /* LIBDSET_ERRCODE_H */
