/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <libdset/types.h> /* dset_args[] */

static const struct dset_arg dset_args[] = {
	/* Hash types */
	[DSET_ARG_HASHSIZE] = {
		.name = {"hashsize", NULL},
		.has_arg = DSET_MANDATORY_ARG,
		.opt = DSET_OPT_HASHSIZE,
		.parse = dset_parse_uint32,
		.print = dset_print_number,
		.help = "[hashsize VALUE]",
	},
	[DSET_ARG_MAXELEM] = {
		.name = {"maxelem", NULL},
		.has_arg = DSET_MANDATORY_ARG,
		.opt = DSET_OPT_MAXELEM,
		.parse = dset_parse_uint32,
		.print = dset_print_number,
		.help = "[maxelem VALUE]",
	},
	/* Ignored options: backward compatibilty */
	[DSET_ARG_PROBES] = {
		.name = {"probes", NULL},
		.has_arg = DSET_MANDATORY_ARG,
		.opt = DSET_OPT_PROBES,
		.parse = dset_parse_ignored,
		.print = dset_print_number,
	},
	[DSET_ARG_RESIZE] = {
		.name = {"resize", NULL},
		.has_arg = DSET_MANDATORY_ARG,
		.opt = DSET_OPT_RESIZE,
		.parse = dset_parse_ignored,
		.print = dset_print_number,
	},
	[DSET_ARG_GC] = {
		.name = {"gc", NULL},
		.has_arg = DSET_MANDATORY_ARG,
		.opt = DSET_OPT_GC,
		.parse = dset_parse_ignored,
		.print = dset_print_number,
	},
	/* List type */
	[DSET_ARG_SIZE] = {
		.name = {"size", NULL},
		.has_arg = DSET_MANDATORY_ARG,
		.opt = DSET_OPT_SIZE,
		.parse = dset_parse_uint32,
		.print = dset_print_number,
		.help = "[size VALUE]",
	},
	/* Setname type elements */
	[DSET_ARG_BEFORE] = {
		.name = {"before", NULL},
		.has_arg = DSET_MANDATORY_ARG,
		.opt = DSET_OPT_NAMEREF,
		.parse = dset_parse_before,
		.help = "[before|after NAME]",
	},
	[DSET_ARG_AFTER] = {
		.name = {"after", NULL},
		.has_arg = DSET_MANDATORY_ARG,
		.opt = DSET_OPT_NAMEREF,
		.parse = dset_parse_after,
	},
	/* Extra flags, options */
	[DSET_ARG_FORCEADD] = {
		.name = {"forceadd", NULL},
		.has_arg = DSET_NO_ARG,
		.opt = DSET_OPT_FORCEADD,
		.parse = dset_parse_flag,
		.print = dset_print_flag,
		.help = "[forceadd]",
	},
	[DSET_ARG_NOMATCH] = {
		.name = {"nomatch", NULL},
		.has_arg = DSET_NO_ARG,
		.opt = DSET_OPT_NOMATCH,
		.parse = dset_parse_flag,
		.print = dset_print_flag,
		.help = "[nomatch]",
	},
	/* Extensions */
	[DSET_ARG_TIMEOUT] = {
		.name = {"timeout", NULL},
		.has_arg = DSET_MANDATORY_ARG,
		.opt = DSET_OPT_TIMEOUT,
		.parse = dset_parse_timeout,
		.print = dset_print_number,
		.help = "[timeout VALUE]",
	},
	[DSET_ARG_COUNTERS] = {
		.name = {"counters", NULL},
		.has_arg = DSET_NO_ARG,
		.opt = DSET_OPT_COUNTERS,
		.parse = dset_parse_flag,
		.print = dset_print_flag,
		.help = "[counters]",
	},
	[DSET_ARG_PACKETS] = {
		.name = {"packets", NULL},
		.has_arg = DSET_MANDATORY_ARG,
		.opt = DSET_OPT_PACKETS,
		.parse = dset_parse_uint64,
		.print = dset_print_number,
		.help = "[packets VALUE]",
	},
	[DSET_ARG_BYTES] = {
		.name = {"bytes", NULL},
		.has_arg = DSET_MANDATORY_ARG,
		.opt = DSET_OPT_BYTES,
		.parse = dset_parse_uint64,
		.print = dset_print_number,
		.help = "[bytes VALUE]",
	},
	[DSET_ARG_COMMENT] = {
		.name = {"comment", NULL},
		.has_arg = DSET_NO_ARG,
		.opt = DSET_OPT_CREATE_COMMENT,
		.parse = dset_parse_flag,
		.print = dset_print_flag,
		.help = "[comment]",
	},
	[DSET_ARG_ADT_COMMENT] = {
		.name = {"comment", NULL},
		.has_arg = DSET_MANDATORY_ARG,
		.opt = DSET_OPT_ADT_COMMENT,
		.parse = dset_parse_comment,
		.print = dset_print_comment,
		.help = "[comment \"string\"]",
	},
	[DSET_ARG_SKBINFO] = {
		.name = {"skbinfo", NULL},
		.has_arg = DSET_NO_ARG,
		.opt = DSET_OPT_SKBINFO,
		.parse = dset_parse_flag,
		.print = dset_print_flag,
		.help = "[skbinfo]",
	},
	[DSET_ARG_SKBMARK] = {
		.name = {"skbmark", NULL},
		.has_arg = DSET_MANDATORY_ARG,
		.opt = DSET_OPT_SKBMARK,
		.parse = dset_parse_skbmark,
		.print = dset_print_skbmark,
		.help = "[skbmark VALUE]",
	},
	[DSET_ARG_SKBPRIO] = {
		.name = {"skbprio", NULL},
		.has_arg = DSET_MANDATORY_ARG,
		.opt = DSET_OPT_SKBPRIO,
		.parse = dset_parse_skbprio,
		.print = dset_print_skbprio,
		.help = "[skbprio VALUE]",
	},
	[DSET_ARG_SKBQUEUE] = {
		.name = {"skbqueue", NULL},
		.has_arg = DSET_MANDATORY_ARG,
		.opt = DSET_OPT_SKBQUEUE,
		.parse = dset_parse_uint16,
		.print = dset_print_number,
		.help = "[skbqueue VALUE]",
	},
};

const struct dset_arg *
dset_keyword(enum dset_keywords i)
{
	return (i > DSET_ARG_NONE && i < DSET_ARG_MAX)
			   ? &dset_args[i]
			   : NULL;
}

const char *
dset_ignored_optname(unsigned int opt)
{
	enum dset_keywords i;

	for (i = DSET_ARG_NONE + 1; i < DSET_ARG_MAX; i++)
		if (dset_args[i].opt == opt)
			return dset_args[i].name[0];
	return "";
}
