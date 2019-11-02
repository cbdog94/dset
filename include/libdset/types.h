/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef LIBDSET_TYPES_H
#define LIBDSET_TYPES_H

#include <stddef.h>				/* NULL */
#include <stdint.h>				/* uintxx_t */

#include <libdset/args.h>			/* enum dset_keywords */
#include <libdset/data.h>			/* enum dset_opt */
#include <libdset/parse.h>			/* dset_parsefn */
#include <libdset/print.h>			/* dset_printfn */
#include <libdset/linux_domain_set.h>		/* DSET_MAXNAMELEN */
#include <libdset/nfproto.h>			/* for NFPROTO_ */

/* Family rules:
 * - NFPROTO_UNSPEC:		type is family-neutral
 * - NFPROTO_IPV4:		type supports IPv4 only
 * - NFPROTO_IPV6:		type supports IPv6 only
 * Special (userspace) ipset-only extra value:
 * - NFPROTO_IPSET_IPV46:	type supports both IPv4 and IPv6
 */
enum {
	NFPROTO_DSET_IPV46 = 255,
};

/* The maximal type dimension userspace supports */
#define DSET_DIM_UMAX		3

/* Parser options */
enum {
	DSET_NO_ARG = -1,
	DSET_OPTIONAL_ARG,
	DSET_MANDATORY_ARG,
	DSET_MANDATORY_ARG2,
};

struct dset_session;

/* Parse and print type-specific arguments */
struct dset_arg {
	const char *name[2];		/* option names */
	int has_arg;			/* mandatory/optional/no arg */
	enum dset_opt opt;		/* argumentum type */
	dset_parsefn parse;		/* parser function */
	dset_printfn print;		/* printing function */
	const char *help;		/* help text */
};

/* Type check against the kernel */
enum {
	DSET_KERNEL_MISMATCH = -1,
	DSET_KERNEL_CHECK_NEEDED,
	DSET_KERNEL_OK,
};

/* How element parts are parsed */
struct dset_elem {
	dset_parsefn parse;			/* elem parser function */
	dset_printfn print;			/* elem print function */
	enum dset_opt opt;			/* elem option */
};

#define DSET_OPTARG_MAX	24

/* How other CADT args are parsed */
struct dset_optarg {
	enum dset_keywords args[DSET_OPTARG_MAX];/* args */
	uint64_t need;				/* needed flags */
	uint64_t full;				/* all possible flags */
	const char *help;			/* help text */
};

/* The set types in userspace
 * we could collapse 'args' and 'mandatory' to two-element lists
 * but for the readability the full list is supported.
  */
struct dset_type {
	const char *name;
	uint8_t revision;			/* revision number */
	uint8_t family;				/* supported family */
	uint8_t dimension;			/* elem dimension */
	int8_t kernel_check;			/* kernel check */
	bool last_elem_optional;		/* last element optional */
	struct dset_elem elem[DSET_DIM_UMAX];	/* parse elem */
	dset_parsefn compat_parse_elem;	/* compatibility parser */
	struct dset_optarg cmd[DSET_CADT_MAX];/* optional arguments */
	const char *usage;			/* terse usage */
	void (*usagefn)(void);			/* additional usage */
	const char *description;		/* short revision description */
	struct dset_type *next;
	const char *alias[];			/* name alias(es) */
};

#ifdef __cplusplus
extern "C" {
#endif

extern int dset_cache_add(const char *name, const struct dset_type *type);
extern int dset_cache_del(const char *name);
extern int dset_cache_rename(const char *from, const char *to);
extern int dset_cache_swap(const char *from, const char *to);

extern int dset_cache_init(void);
extern void dset_cache_fini(void);

extern const struct dset_type *
	dset_type_get(struct dset_session *session, enum dset_cmd cmd);
extern const struct dset_type *
	dset_type_check(struct dset_session *session);
extern const struct dset_type *
	dset_type_higher_rev(const struct dset_type *type);

extern int dset_type_add(struct dset_type *type);
extern const struct dset_type *dset_types(void);
extern const char *dset_typename_resolve(const char *str);
extern bool dset_match_typename(const char *str,
				 const struct dset_type *t);
extern void dset_load_types(void);

#ifdef __cplusplus
}
#endif

#ifdef TYPE_INCLUSIVE
#	ifdef _INIT
#		undef _init
#		define _init _INIT
#	endif
#else
#	undef _init
#	define _init __attribute__((constructor)) _INIT
#endif

#endif /* LIBDSET_TYPES_H */
