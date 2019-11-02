/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <libdset/data.h>  /* DSET_OPT_* */
#include <libdset/parse.h> /* parser functions */
#include <libdset/print.h> /* printing functions */
#include <libdset/types.h> /* prototypes */

/* Initial release */
static struct dset_type dset_hash_domain0 = {
	.name = "hash:domain",
	.alias = {"dhash", NULL},
	.revision = 0,
	.family = NFPROTO_UNSPEC,
	.dimension = DSET_DIM_ONE,
	.elem = {
		[DSET_DIM_ONE - 1] = {
			.parse = dset_parse_domain,
			.print = dset_print_domain,
			.opt = DSET_OPT_DOMAIN},
	},
	.cmd = {
		[DSET_CREATE] = {
			.args = {
				/* Aliases */
				DSET_ARG_HASHSIZE,
				DSET_ARG_MAXELEM,
				DSET_ARG_TIMEOUT,
				/* Ignored options: backward compatibilty */
				DSET_ARG_PROBES,
				DSET_ARG_RESIZE,
				DSET_ARG_GC,
				DSET_ARG_NONE,
			},
			.need = 0,
			.full = 0,
			.help = "",
		},
		[DSET_ADD] = {
			.args = {
				DSET_ARG_TIMEOUT,
				DSET_ARG_NONE,
			},
			.need = DSET_FLAG(DSET_OPT_DOMAIN),
			.full = DSET_FLAG(DSET_OPT_DOMAIN),
			.help = "DOMAIN",
		},
		[DSET_DEL] = {
			.args = {
				DSET_ARG_NONE,
			},
			.need = DSET_FLAG(DSET_OPT_DOMAIN),
			.full = DSET_FLAG(DSET_OPT_DOMAIN),
			.help = "DOMAIN",
		},
		[DSET_TEST] = {
			.args = {
				DSET_ARG_NONE,
			},
			.need = DSET_FLAG(DSET_OPT_DOMAIN),
			.full = DSET_FLAG(DSET_OPT_DOMAIN),
			.help = "DOMAIN",
		},
	},
	.usage = "Domain supported.",
	.description = "Initial revision",
};

void _init(void);
void _init(void)
{
	dset_type_add(&dset_hash_domain0);
}
