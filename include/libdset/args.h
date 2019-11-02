/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef LIBDSET_ARGS_H
#define LIBDSET_ARGS_H

/* Keywords */
enum dset_keywords {
	DSET_ARG_NONE = 0,
	/* Family and aliases */
	DSET_ARG_DOMAIN,			/* domain */
	/* Hash types */
	DSET_ARG_HASHSIZE,			/* hashsize */
	DSET_ARG_MAXELEM,			/* maxelem */
	/* Ignored options: backward compatibilty */
	DSET_ARG_PROBES,			/* probes */
	DSET_ARG_RESIZE,			/* resize */
	DSET_ARG_GC,				/* gc */
	/* List type */
	DSET_ARG_SIZE,				/* size */
	/* Setname type elements */
	DSET_ARG_BEFORE,			/* before */
	DSET_ARG_AFTER,			/* after */
	/* Extra flags, options */
	DSET_ARG_FORCEADD,			/* forceadd */
	DSET_ARG_NOMATCH,			/* nomatch */
	/* Extensions */
	DSET_ARG_TIMEOUT,			/* timeout */
	DSET_ARG_COUNTERS,			/* counters */
	DSET_ARG_PACKETS,			/* packets */
	DSET_ARG_BYTES,			/* bytes */
	DSET_ARG_COMMENT,			/* comment */
	DSET_ARG_ADT_COMMENT,			/* comment */
	DSET_ARG_SKBINFO,			/* skbinfo */
	DSET_ARG_SKBMARK,			/* skbmark */
	DSET_ARG_SKBPRIO,			/* skbprio */
	DSET_ARG_SKBQUEUE,			/* skbqueue */
	DSET_ARG_MAX,
};

#ifdef __cplusplus
extern "C" {
#endif

extern const struct dset_arg * dset_keyword(enum dset_keywords i);
extern const char * dset_ignored_optname(unsigned int opt);
#ifdef __cplusplus
}
#endif

#endif /* LIBDSET_ARGS_H */
