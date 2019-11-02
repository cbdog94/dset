/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <assert.h>	/* assert */
#include <errno.h>	 /* errno */
#include <stdlib.h>	/* calloc, free */
#include <time.h>	  /* time */
#include <arpa/inet.h> /* hto* */

#include <libdset/linux_domain_set.h> /* enum dset_cmd */
#include <libdset/debug.h>			  /* D() */
#include <libdset/session.h>		  /* dset_session_handle */
#include <libdset/dset.h>			  /* DSET_ENV_EXIST */
#include <libdset/utils.h>			  /* UNUSED */
#include <libdset/mnl.h>			  /* prototypes */

#ifndef NFNL_SUBSYS_DSET
#define NFNL_SUBSYS_DSET 12
#endif

/* Internal data structure for the kernel-userspace communication parameters */
struct dset_handle
{
	struct mnl_socket *h; /* the mnl socket */
	unsigned int seq;	 /* netlink message sequence number */
	unsigned int portid;  /* the socket port identifier */
	mnl_cb_t *cb_ctl;	 /* control block callbacks */
	void *data;			  /* data pointer */
};

/* Netlink flags of the commands */
static const uint16_t cmdflags[] = {
	[DSET_CMD_CREATE - 1] = NLM_F_REQUEST | NLM_F_ACK |
							NLM_F_CREATE | NLM_F_EXCL,
	[DSET_CMD_DESTROY - 1] = NLM_F_REQUEST | NLM_F_ACK,
	[DSET_CMD_FLUSH - 1] = NLM_F_REQUEST | NLM_F_ACK,
	[DSET_CMD_RENAME - 1] = NLM_F_REQUEST | NLM_F_ACK,
	[DSET_CMD_SWAP - 1] = NLM_F_REQUEST | NLM_F_ACK,
	[DSET_CMD_LIST - 1] = NLM_F_REQUEST | NLM_F_ACK | NLM_F_DUMP,
	[DSET_CMD_SAVE - 1] = NLM_F_REQUEST | NLM_F_ACK | NLM_F_DUMP,
	[DSET_CMD_ADD - 1] = NLM_F_REQUEST | NLM_F_ACK | NLM_F_EXCL,
	[DSET_CMD_DEL - 1] = NLM_F_REQUEST | NLM_F_ACK | NLM_F_EXCL,
	[DSET_CMD_TEST - 1] = NLM_F_REQUEST | NLM_F_ACK,
	[DSET_CMD_HEADER - 1] = NLM_F_REQUEST,
	[DSET_CMD_TYPE - 1] = NLM_F_REQUEST,
	[DSET_CMD_PROTOCOL - 1] = NLM_F_REQUEST,
};

/**
 * dset_get_nlmsg_type - get dset netlink message type
 * @nlh: pointer to the netlink message header
 *
 * Returns the dset netlink message type, i.e. the dset command.
 */
int dset_get_nlmsg_type(const struct nlmsghdr *nlh)
{
	return nlh->nlmsg_type & ~(NFNL_SUBSYS_DSET << 8);
}

static void
dset_mnl_fill_hdr(struct dset_handle *handle, enum dset_cmd cmd,
				  void *buffer, size_t len UNUSED, uint8_t envflags)
{
	struct nlmsghdr *nlh;
	struct nfgenmsg *nfg;

	assert(handle);
	assert(buffer);
	assert(cmd > DSET_CMD_NONE && cmd < DSET_MSG_MAX);

	nlh = mnl_nlmsg_put_header(buffer);
	nlh->nlmsg_type = cmd | (NFNL_SUBSYS_DSET << 8);
	nlh->nlmsg_flags = cmdflags[cmd - 1];
	if (envflags & DSET_ENV_EXIST)
		nlh->nlmsg_flags &= ~NLM_F_EXCL;

	nfg = mnl_nlmsg_put_extra_header(nlh, sizeof(struct nfgenmsg));
	nfg->nfgen_family = AF_INET;
	nfg->version = NFNETLINK_V0;
	nfg->res_id = htons(0);
}

static int
dset_mnl_query(struct dset_handle *handle, void *buffer, size_t len)
{
	struct nlmsghdr *nlh = buffer;
	int ret;

	assert(handle);
	assert(buffer);

	nlh->nlmsg_seq = ++handle->seq;
#ifdef DSET_DEBUG
	dset_debug_msg("sent", nlh, nlh->nlmsg_len);
#endif
	if (mnl_socket_sendto(handle->h, nlh, nlh->nlmsg_len) < 0)
		return -ECOMM;

	ret = mnl_socket_recvfrom(handle->h, buffer, len);
#ifdef DSET_DEBUG
	dset_debug_msg("received", buffer, ret);
#endif
	while (ret > 0)
	{
		ret = mnl_cb_run2(buffer, ret,
						  handle->seq, handle->portid,
						  handle->cb_ctl[NLMSG_MIN_TYPE],
						  handle->data,
						  handle->cb_ctl, NLMSG_MIN_TYPE);
		D("nfln_cb_run2, ret: %d, errno %d", ret, errno);
		if (ret <= 0)
			break;
		ret = mnl_socket_recvfrom(handle->h, buffer, len);
		D("message received, ret: %d", ret);
	}
	return ret;
}

static struct dset_handle *
dset_mnl_init(mnl_cb_t *cb_ctl, void *data)
{
	struct dset_handle *handle;

	assert(cb_ctl);
	assert(data);

	handle = calloc(1, sizeof(*handle));
	if (!handle)
		return NULL;

	handle->h = mnl_socket_open(NETLINK_NETFILTER);
	if (!handle->h)
		goto free_handle;

	if (mnl_socket_bind(handle->h, 0, MNL_SOCKET_AUTOPID) < 0)
		goto close_nl;

	handle->portid = mnl_socket_get_portid(handle->h);
	handle->cb_ctl = cb_ctl;
	handle->data = data;
	handle->seq = time(NULL);

	return handle;

close_nl:
	mnl_socket_close(handle->h);
free_handle:
	free(handle);

	return NULL;
}

static int
dset_mnl_fini(struct dset_handle *handle)
{
	assert(handle);

	if (handle->h)
		mnl_socket_close(handle->h);

	free(handle);
	return 0;
}

const struct dset_transport dset_mnl_transport = {
	.init = dset_mnl_init,
	.fini = dset_mnl_fini,
	.fill_hdr = dset_mnl_fill_hdr,
	.query = dset_mnl_query,
};
