/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <arpa/inet.h>	 /* inet_ntop */
#include <libmnl/libmnl.h> /* libmnl backend */

struct dset_attrname
{
	const char *name;
};

static const struct dset_attrname cmdattr2name[] = {
	[DSET_ATTR_PROTOCOL] = {.name = "PROTOCOL"},
	[DSET_ATTR_SETNAME] = {.name = "SETNAME"},
	[DSET_ATTR_TYPENAME] = {.name = "TYPENAME"},
	[DSET_ATTR_REVISION] = {.name = "REVISION"},
	[DSET_ATTR_FAMILY] = {.name = "FAMILY"},
	[DSET_ATTR_FLAGS] = {.name = "FLAGS"},
	[DSET_ATTR_DATA] = {.name = "DATA"},
	[DSET_ATTR_ADT] = {.name = "ADT"},
	[DSET_ATTR_LINENO] = {.name = "LINENO"},
	[DSET_ATTR_PROTOCOL_MIN] = {.name = "PROTO_MIN"},
};

static const struct dset_attrname createattr2name[] = {
	[DSET_ATTR_DOMAIN] = {.name = "DOMAIN"},
	[DSET_ATTR_TIMEOUT] = {.name = "TIMEOUT"},
	[DSET_ATTR_CADT_FLAGS] = {.name = "CADT_FLAGS"},
	[DSET_ATTR_CADT_LINENO] = {.name = "CADT_LINENO"},
	[DSET_ATTR_GC] = {.name = "GC"},
	[DSET_ATTR_HASHSIZE] = {.name = "HASHSIZE"},
	[DSET_ATTR_MAXELEM] = {.name = "MAXELEM"},
	[DSET_ATTR_PROBES] = {.name = "PROBES"},
	[DSET_ATTR_RESIZE] = {.name = "RESIZE"},
	[DSET_ATTR_SIZE] = {.name = "SIZE"},
	[DSET_ATTR_ELEMENTS] = {.name = "ELEMENTS"},
	[DSET_ATTR_REFERENCES] = {.name = "REFERENCES"},
	[DSET_ATTR_MEMSIZE] = {.name = "MEMSIZE"},
};

static const struct dset_attrname adtattr2name[] = {
	[DSET_ATTR_DOMAIN] = {.name = "DOMAIN"},
	[DSET_ATTR_TIMEOUT] = {.name = "TIMEOUT"},
	[DSET_ATTR_CADT_FLAGS] = {.name = "CADT_FLAGS"},
	[DSET_ATTR_CADT_LINENO] = {.name = "CADT_LINENO"},
	[DSET_ATTR_NAME] = {.name = "NAME"},
	[DSET_ATTR_NAMEREF] = {.name = "NAMEREF"},
	[DSET_ATTR_COMMENT] = {.name = "COMMENT"},
	[DSET_ATTR_SKBMARK] = {.name = "SKBMARK"},
	[DSET_ATTR_SKBPRIO] = {.name = "SKBPRIO"},
	[DSET_ATTR_SKBQUEUE] = {.name = "SKBQUEUE"},
};

static void
debug_cadt_attrs(int max, const struct dset_attr_policy *policy,
				 const struct dset_attrname attr2name[],
				 struct nlattr *nla[])
{
	uint64_t tmp;
	uint32_t v;
	int i;

	fprintf(stderr, "\t\t%s attributes:\n",
			policy == create_attrs ? "CREATE" : "ADT");
	for (i = DSET_ATTR_UNSPEC + 1; i <= max; i++)
	{
		if (!nla[i])
			continue;
		switch (policy[i].type)
		{
		case MNL_TYPE_UNSPEC:
			fprintf(stderr, "\t\tpadding\n");
			break;
		case MNL_TYPE_U8:
			v = *(uint8_t *)mnl_attr_get_payload(nla[i]);
			fprintf(stderr, "\t\t%s: %u\n",
					attr2name[i].name, v);
			break;
		case MNL_TYPE_U16:
			v = *(uint16_t *)mnl_attr_get_payload(nla[i]);
			fprintf(stderr, "\t\t%s: %u\n",
					attr2name[i].name, ntohs(v));
			break;
		case MNL_TYPE_U32:
			v = *(uint32_t *)mnl_attr_get_payload(nla[i]);
			fprintf(stderr, "\t\t%s: %u\n",
					attr2name[i].name, ntohl(v));
			break;
		case MNL_TYPE_U64:
			memcpy(&tmp, mnl_attr_get_payload(nla[i]), sizeof(tmp));
			fprintf(stderr, "\t\t%s: 0x%llx\n",
					attr2name[i].name, (long long int)be64toh(tmp));
			break;
		case MNL_TYPE_NUL_STRING:
			fprintf(stderr, "\t\t%s: %s\n",
					attr2name[i].name,
					(const char *)mnl_attr_get_payload(nla[i]));
			break;
		default:
			fprintf(stderr, "\t\t%s: unresolved!\n",
					attr2name[i].name);
		}
	}
}

static void
debug_cmd_attrs(int cmd, struct nlattr *nla[])
{
	struct nlattr *adt[DSET_ATTR_ADT_MAX + 1] = {};
	struct nlattr *cattr[DSET_ATTR_CREATE_MAX + 1] = {};
	uint32_t v;
	int i;

	fprintf(stderr, "\tCommand attributes:\n");
	for (i = DSET_ATTR_UNSPEC + 1; i <= DSET_ATTR_CMD_MAX; i++)
	{
		if (!nla[i])
			continue;
		switch (cmd_attrs[i].type)
		{
		case MNL_TYPE_U8:
			v = *(uint8_t *)mnl_attr_get_payload(nla[i]);
			fprintf(stderr, "\t%s: %u\n",
					cmdattr2name[i].name, v);
			break;
		case MNL_TYPE_U16:
			v = *(uint16_t *)mnl_attr_get_payload(nla[i]);
			fprintf(stderr, "\t%s: %u\n",
					cmdattr2name[i].name, ntohs(v));
			break;
		case MNL_TYPE_U32:
			v = *(uint32_t *)mnl_attr_get_payload(nla[i]);
			fprintf(stderr, "\t%s: %u\n",
					cmdattr2name[i].name, ntohl(v));
			break;
		case MNL_TYPE_NUL_STRING:
			fprintf(stderr, "\t%s: %s\n",
					cmdattr2name[i].name,
					(const char *)mnl_attr_get_payload(nla[i]));
			break;
		case MNL_TYPE_NESTED:
			if (i == DSET_ATTR_DATA)
			{
				switch (cmd)
				{
				case DSET_CMD_ADD:
				case DSET_CMD_DEL:
				case DSET_CMD_TEST:
					if (mnl_attr_parse_nested(nla[i],
											  adt_attr_cb, adt) < 0)
					{
						fprintf(stderr,
								"\tADT: cannot validate "
								"and parse attributes\n");
						continue;
					}
					debug_cadt_attrs(DSET_ATTR_ADT_MAX,
									 adt_attrs,
									 adtattr2name,
									 adt);
					break;
				default:
					if (mnl_attr_parse_nested(nla[i],
											  create_attr_cb,
											  cattr) < 0)
					{
						fprintf(stderr,
								"\tCREATE: cannot validate "
								"and parse attributes\n");
						continue;
					}
					debug_cadt_attrs(DSET_ATTR_CREATE_MAX,
									 create_attrs,
									 createattr2name,
									 cattr);
				}
			}
			else
			{
				struct nlattr *tb;
				mnl_attr_for_each_nested(tb, nla[i])
				{
					memset(adt, 0, sizeof(adt));
					if (mnl_attr_parse_nested(tb,
											  adt_attr_cb, adt) < 0)
					{
						fprintf(stderr,
								"\tADT: cannot validate "
								"and parse attributes\n");
						continue;
					}
					debug_cadt_attrs(DSET_ATTR_ADT_MAX,
									 adt_attrs,
									 adtattr2name,
									 adt);
				}
			}
			break;
		default:
			fprintf(stderr, "\t%s: unresolved!\n",
					cmdattr2name[i].name);
		}
	}
}

void dset_debug_msg(const char *dir, void *buffer, int len)
{
	const struct nlmsghdr *nlh = buffer;
	struct nlattr *nla[DSET_ATTR_CMD_MAX + 1] = {};
	int cmd, nfmsglen = MNL_ALIGN(sizeof(struct nfgenmsg));

	debug = 0;
	if (!mnl_nlmsg_ok(nlh, len))
	{
		fprintf(stderr, "Broken message received!\n");
		if (len < (int)sizeof(struct nlmsghdr))
		{
			fprintf(stderr, "len (%d) < sizeof(struct nlmsghdr) (%d)\n",
					len, (int)sizeof(struct nlmsghdr));
		}
		else if (nlh->nlmsg_len < sizeof(struct nlmsghdr))
		{
			fprintf(stderr, "nlmsg_len (%u) < sizeof(struct nlmsghdr) (%d)\n",
					nlh->nlmsg_len, (int)sizeof(struct nlmsghdr));
		}
		else if ((int)nlh->nlmsg_len > len)
		{
			fprintf(stderr, "nlmsg_len (%u) > len (%d)\n",
					nlh->nlmsg_len, len);
		}
	}
	while (mnl_nlmsg_ok(nlh, len))
	{
		switch (nlh->nlmsg_type)
		{
		case NLMSG_NOOP:
		case NLMSG_DONE:
		case NLMSG_OVERRUN:
			fprintf(stderr, "Message header: %s msg %s\n"
							"\tlen %d\n"
							"\tseq  %u\n",
					dir,
					nlh->nlmsg_type == NLMSG_NOOP ? "NOOP" : nlh->nlmsg_type == NLMSG_DONE ? "DONE" : "OVERRUN",
					len, nlh->nlmsg_seq);
			goto next_msg;
		case NLMSG_ERROR:
		{
			const struct nlmsgerr *err = mnl_nlmsg_get_payload(nlh);
			fprintf(stderr, "Message header: %s msg ERROR\n"
							"\tlen %d\n"
							"\terrcode %d\n"
							"\tseq %u\n",
					dir, len, err->error, nlh->nlmsg_seq);
			goto next_msg;
		}
		default:;
		}
		cmd = dset_get_nlmsg_type(nlh);
		fprintf(stderr, "Message header: %s cmd  %s (%d)\n"
						"\tlen %d\n"
						"\tflag %s\n"
						"\tseq %u\n",
				dir,
				cmd <= DSET_CMD_NONE ? "NONE!" : cmd >= DSET_CMD_MAX ? "MAX!" : cmd2name[cmd], cmd,
				len,
				!(nlh->nlmsg_flags & NLM_F_EXCL) ? "EXIST" : "none",
				nlh->nlmsg_seq);
		if (cmd <= DSET_CMD_NONE || cmd >= DSET_CMD_MAX)
			goto next_msg;
		memset(nla, 0, sizeof(nla));
		if (mnl_attr_parse(nlh, nfmsglen,
						   cmd_attr_cb, nla) < MNL_CB_STOP)
		{
			fprintf(stderr, "\tcannot validate "
							"and parse attributes\n");
			goto next_msg;
		}
		debug_cmd_attrs(cmd, nla);
	next_msg:
		nlh = mnl_nlmsg_next(nlh, &len);
	}
	debug = 1;
}
