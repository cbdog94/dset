/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef LIBDSET_TRANSPORT_H
#define LIBDSET_TRANSPORT_H

#include <stdint.h>				/* uintxx_t */
#include <linux/netlink.h>			/* struct nlmsghdr  */

#include <libmnl/libmnl.h>			/* mnl_cb_t */

#include <libdset/linux_domain_set.h>		/* enum dset_cmd */

struct dset_handle;

struct dset_transport {
	struct dset_handle * (*init)(mnl_cb_t *cb_ctl, void *data);
	int (*fini)(struct dset_handle *handle);
	void (*fill_hdr)(struct dset_handle *handle, enum dset_cmd cmd,
			 void *buffer, size_t len, uint8_t envflags);
	int (*query)(struct dset_handle *handle, void *buffer, size_t len);
};

#endif /* LIBDSET_TRANSPORT_H */
