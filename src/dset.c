/* Copyright 2000-2002 Joakim Axelsson (gozem@linux.nu)
 *                     Patrick Schaaf (bof@bof.de)
 * Copyright 2003-2010 Jozsef Kadlecsik (kadlec@netfilter.org)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <assert.h> /* assert */
#include <stdio.h>  /* fprintf */
#include <stdlib.h> /* exit */

#include <config.h>
#include <libdset/dset.h> /* dset library */

int main(int argc, char *argv[])
{
	struct dset *dset;
	int ret;

	/* Load set types */
	dset_load_types();

	/* Initialize ipset library */
	dset = dset_init();
	if (dset == NULL)
	{
		fprintf(stderr, "Cannot initialize dset, aborting.");
		exit(1);
	}

	ret = dset_parse_argv(dset, argc, argv);

	dset_fini(dset);

	return ret;
}
