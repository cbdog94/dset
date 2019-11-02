/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef LIBDSET_DATA_H
#define LIBDSET_DATA_H

#include <stdbool.h>				/* bool */
#include <stdint.h>
#include <stddef.h>

/* Data options */
enum dset_opt {
	DSET_OPT_NONE = 0,
	/* Common ones */
	DSET_SETNAME,
	DSET_OPT_TYPENAME,
	DSET_OPT_FAMILY,
	/* CADT options */
	DSET_OPT_DOMAIN,
	DSET_OPT_TIMEOUT,
	/* Create-specific options */
	DSET_OPT_GC,
	DSET_OPT_HASHSIZE,
	DSET_OPT_MAXELEM,
	DSET_OPT_PROBES,
	DSET_OPT_RESIZE,
	DSET_OPT_SIZE,
	DSET_OPT_FORCEADD,
	/* Create-specific options, filled out by the kernel */
	DSET_OPT_ELEMENTS,
	DSET_OPT_REFERENCES,
	DSET_OPT_MEMSIZE,
	/* ADT-specific options */
	DSET_OPT_NAME,
	DSET_OPT_NAMEREF,
	/* Swap/rename to */
	DSET_OPT_SETNAME2,
	/* Flags */
	DSET_OPT_EXIST,
	DSET_OPT_BEFORE,
	DSET_OPT_PHYSDEV,
	DSET_OPT_NOMATCH,
	DSET_OPT_COUNTERS,
	DSET_OPT_PACKETS,
	DSET_OPT_BYTES,
	DSET_OPT_CREATE_COMMENT,
	DSET_OPT_ADT_COMMENT,
	DSET_OPT_SKBINFO,
	DSET_OPT_SKBMARK,
	DSET_OPT_SKBPRIO,
	DSET_OPT_SKBQUEUE,
	/* Internal options */
	DSET_OPT_FLAGS = 48,	/* DSET_FLAG_EXIST| */
	DSET_OPT_CADT_FLAGS,	/* DSET_FLAG_BEFORE| */
	DSET_OPT_ELEM,
	DSET_OPT_TYPE,
	DSET_OPT_LINENO,
	DSET_OPT_REVISION,
	DSET_OPT_REVISION_MIN,
	DSET_OPT_INDEX,
	DSET_OPT_MAX,
};

#define DSET_FLAG(opt)		(1ULL << (opt))
#define DSET_FLAGS_ALL		(~0ULL)

#define DSET_CREATE_FLAGS		\
	(DSET_FLAG(DSET_OPT_FAMILY)	\
	| DSET_FLAG(DSET_OPT_TYPENAME)	\
	| DSET_FLAG(DSET_OPT_TYPE)	\
	| DSET_FLAG(DSET_OPT_DOMAIN)	\
	| DSET_FLAG(DSET_OPT_TIMEOUT)	\
	| DSET_FLAG(DSET_OPT_GC)	\
	| DSET_FLAG(DSET_OPT_HASHSIZE)	\
	| DSET_FLAG(DSET_OPT_MAXELEM)	\
	| DSET_FLAG(DSET_OPT_PROBES)	\
	| DSET_FLAG(DSET_OPT_RESIZE)	\
	| DSET_FLAG(DSET_OPT_SIZE)	\
	| DSET_FLAG(DSET_OPT_COUNTERS)	\
	| DSET_FLAG(DSET_OPT_CREATE_COMMENT)\
	| DSET_FLAG(DSET_OPT_FORCEADD)	\
	| DSET_FLAG(DSET_OPT_SKBINFO))

#define DSET_ADT_FLAGS			\
	(DSET_FLAG(DSET_OPT_TIMEOUT)	\
	| DSET_FLAG(DSET_OPT_NAME)	\
	| DSET_FLAG(DSET_OPT_NAMEREF)	\
	| DSET_FLAG(DSET_OPT_CADT_FLAGS)\
	| DSET_FLAG(DSET_OPT_BEFORE)	\
	| DSET_FLAG(DSET_OPT_PHYSDEV)	\
	| DSET_FLAG(DSET_OPT_NOMATCH)	\
	| DSET_FLAG(DSET_OPT_PACKETS)	\
	| DSET_FLAG(DSET_OPT_BYTES)	\
	| DSET_FLAG(DSET_OPT_ADT_COMMENT)	\
	| DSET_FLAG(DSET_OPT_SKBMARK)	\
	| DSET_FLAG(DSET_OPT_SKBPRIO)	\
	| DSET_FLAG(DSET_OPT_SKBQUEUE))

struct dset_data;

#ifdef __cplusplus
extern "C" {
#endif

extern void dset_strlcpy(char *dst, const char *src, size_t len);
extern void dset_strlcat(char *dst, const char *src, size_t len);
extern bool dset_data_flags_test(const struct dset_data *data,
				  uint64_t flags);
extern void dset_data_flags_set(struct dset_data *data, uint64_t flags);
extern void dset_data_flags_unset(struct dset_data *data, uint64_t flags);
extern bool dset_data_ignored(struct dset_data *data, enum dset_opt opt);
extern bool dset_data_test_ignored(struct dset_data *data,
				    enum dset_opt opt);

extern int dset_data_set(struct dset_data *data, enum dset_opt opt,
			  const void *value);
extern const void *dset_data_get(const struct dset_data *data,
				  enum dset_opt opt);

static inline bool
dset_data_test(const struct dset_data *data, enum dset_opt opt)
{
	return dset_data_flags_test(data, DSET_FLAG(opt));
}

/* Shortcuts */
extern const char *dset_data_setname(const struct dset_data *data);
extern uint64_t dset_data_flags(const struct dset_data *data);

extern void dset_data_reset(struct dset_data *data);
extern struct dset_data *dset_data_init(void);
extern void dset_data_fini(struct dset_data *data);

extern size_t dset_data_sizeof(enum dset_opt opt, uint8_t family);

#ifdef __cplusplus
}
#endif

#endif /* LIBDSET_DATA_H */
