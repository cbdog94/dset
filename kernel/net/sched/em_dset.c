/*
 * net/sched/em_dset.c	dset ematch
 *
 * Copyright (c) 2012 Florian Westphal <fw@strlen.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#include <linux/gfp.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/skbuff.h>
#include <linux/netfilter/xt_dset.h>
#include <linux/netfilter/dset/domain_set_compat.h>
#include <linux/ipv6.h>
#include <net/ip.h>
#include <net/pkt_cls.h>

#ifdef HAVE_TCF_EMATCH_OPS_CHANGE_ARG_NET
static int em_dset_change(struct net *net, void *data, int data_len,
			   struct tcf_ematch *em)
#else
static int em_dset_change(struct tcf_proto *tp, void *data, int data_len,
			   struct tcf_ematch *em)
#endif
{
	struct xt_set_info *set = data;
	domain_set_id_t index;
#ifndef HAVE_TCF_EMATCH_OPS_CHANGE_ARG_NET
	struct net *net = dev_net(qdisc_dev(tp->q));
#endif

	if (data_len != sizeof(*set))
		return -EINVAL;

	index = domain_set_nfnl_get_byindex(net, set->index);
	if (index == DSET_INVALID_ID)
		return -ENOENT;

	em->datalen = sizeof(*set);
	em->data = (unsigned long)kmemdup(data, em->datalen, GFP_KERNEL);
	if (em->data)
		return 0;

	domain_set_nfnl_put(net, index);
	return -ENOMEM;
}

#ifdef HAVE_TCF_EMATCH_STRUCT_NET
static void em_dset_destroy(struct tcf_ematch *em)
#else
static void em_dset_destroy(struct tcf_proto *p, struct tcf_ematch *em)
#endif
{
	const struct xt_set_info *set = (const void *) em->data;

	if (set) {
#ifdef HAVE_TCF_EMATCH_STRUCT_NET
		domain_set_nfnl_put(em->net, set->index);
#else
		domain_set_nfnl_put(dev_net(qdisc_dev(p->q)), set->index);
#endif
		kfree((void *) em->data);
	}
}

static int em_dset_match(struct sk_buff *skb, struct tcf_ematch *em,
			  struct tcf_pkt_info *info)
{
	struct domain_set_adt_opt opt;
	struct xt_action_param acpar;
	const struct xt_set_info *set = (const void *) em->data;
	struct net_device *dev, *indev = NULL;
#ifdef HAVE_STATE_IN_XT_ACTION_PARAM
	struct nf_hook_state state = {
		.net	= em->net,
	};
#endif
	int ret, network_offset;

#ifdef HAVE_STATE_IN_XT_ACTION_PARAM
#define ACPAR_FAMILY(f)		state.pf = f
#else
#define ACPAR_FAMILY(f)		acpar.family = f
#endif
	switch (tc_skb_protocol(skb)) {
	case htons(ETH_P_IP):
		ACPAR_FAMILY(NFPROTO_IPV4);
		if (!pskb_network_may_pull(skb, sizeof(struct iphdr)))
			return 0;
		acpar.thoff = ip_hdrlen(skb);
		break;
	case htons(ETH_P_IPV6):
		ACPAR_FAMILY(NFPROTO_IPV6);
		if (!pskb_network_may_pull(skb, sizeof(struct ipv6hdr)))
			return 0;
		/* doesn't call ipv6_find_hdr() because dset doesn't use
		 * thoff, yet
		 */
		acpar.thoff = sizeof(struct ipv6hdr);
		break;
	default:
		return 0;
	}

#ifdef HAVE_STATE_IN_XT_ACTION_PARAM
	opt.family = state.pf;
#else
	acpar.hooknum = 0;

	opt.family = acpar.family;
#endif
	opt.dim = set->dim;
	opt.flags = set->flags;
	opt.cmdflags = 0;
	opt.ext.timeout = ~0u;

	network_offset = skb_network_offset(skb);
	skb_pull(skb, network_offset);

	dev = skb->dev;

	rcu_read_lock();

	if (skb->skb_iif)
#ifdef HAVE_TCF_EMATCH_STRUCT_NET
		indev = dev_get_by_index_rcu(em->net, skb->skb_iif);
#else
		indev = dev_get_by_index_rcu(dev_net(dev), skb->skb_iif);
#endif

#ifdef HAVE_STATE_IN_XT_ACTION_PARAM
	state.in      = indev ? indev : dev;
	state.out     = dev;
	acpar.state   = &state;
#else
#ifdef HAVE_NET_IN_XT_ACTION_PARAM
	acpar.net     = em->net;
#endif
	acpar.in      = indev ? indev : dev;
	acpar.out     = dev;
#endif /* HAVE_STATE_IN_XT_ACTION_PARAM */

	ret = domain_set_test(set->index, skb, &acpar, &opt);

	rcu_read_unlock();

	skb_push(skb, network_offset);
	return ret;
}

static struct tcf_ematch_ops em_dset_ops = {
	.kind	  = TCF_EM_DSET,
	.change	  = em_dset_change,
	.destroy  = em_dset_destroy,
	.match	  = em_dset_match,
	.owner	  = THIS_MODULE,
	.link	  = LIST_HEAD_INIT(em_dset_ops.link)
};

static int __init init_em_dset(void)
{
	return tcf_em_register(&em_dset_ops);
}

static void __exit exit_em_dset(void)
{
	tcf_em_unregister(&em_dset_ops);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Florian Westphal <fw@strlen.de>");
MODULE_DESCRIPTION("TC extended match for IP sets");

module_init(init_em_dset);
module_exit(exit_em_dset);

MODULE_ALIAS_TCF_EMATCH(TCF_EM_DSET);
