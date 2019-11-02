/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef LIBDSET_MNL_H
#define LIBDSET_MNL_H

#include <stdint.h>				/* uintxx_t */
#include <libmnl/libmnl.h>			/* libmnl backend */

#include <libdset/transport.h>			/* struct ipset_transport */

#ifndef NFNETLINK_V0
#define NFNETLINK_V0		0

struct nfgenmsg {
	uint8_t nfgen_family;
	uint8_t version;
	uint16_t res_id;
};
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern int dset_get_nlmsg_type(const struct nlmsghdr *nlh);
extern const struct dset_transport dset_mnl_transport;

#ifdef __cplusplus
}
#endif

#endif /* LIBDSET_MNL_H */
