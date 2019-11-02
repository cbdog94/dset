/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef LIBDSET_PARSE_H
#define LIBDSET_PARSE_H

#include <libdset/data.h>			/* enum dset_opt */

/* For parsing/printing data */
#define DSET_CIDR_SEPARATOR	"/"
#define DSET_RANGE_SEPARATOR	"-"
#define DSET_ELEM_SEPARATOR	","
#define DSET_NAME_SEPARATOR	","
#define DSET_PROTO_SEPARATOR	":"
#define DSET_ESCAPE_START	"["
#define DSET_ESCAPE_END	"]"

struct dset_session;
struct dset_arg;

typedef int (*dset_parsefn)(struct dset_session *s,
			     enum dset_opt opt, const char *str);

#ifdef __cplusplus
extern "C" {
#endif
extern int dset_parse_domain(struct dset_session *session,
			     enum dset_opt opt, const char *str);
extern int dset_parse_name(struct dset_session *session,
			    enum dset_opt opt, const char *str);
extern int dset_parse_before(struct dset_session *session,
			      enum dset_opt opt, const char *str);
extern int dset_parse_after(struct dset_session *session,
			     enum dset_opt opt, const char *str);
extern int dset_parse_setname(struct dset_session *session,
			       enum dset_opt opt, const char *str);
extern int dset_parse_timeout(struct dset_session *session,
			       enum dset_opt opt, const char *str);
extern int dset_parse_uint64(struct dset_session *session,
			      enum dset_opt opt, const char *str);
extern int dset_parse_uint32(struct dset_session *session,
			      enum dset_opt opt, const char *str);
extern int dset_parse_uint16(struct dset_session *session,
			      enum dset_opt opt, const char *str);
extern int dset_parse_uint8(struct dset_session *session,
			     enum dset_opt opt, const char *str);
extern int dset_parse_flag(struct dset_session *session,
			    enum dset_opt opt, const char *str);
extern int dset_parse_typename(struct dset_session *session,
				enum dset_opt opt, const char *str);
extern int dset_parse_comment(struct dset_session *session,
			       enum dset_opt opt, const char *str);
extern int dset_parse_skbmark(struct dset_session *session,
			       enum dset_opt opt, const char *str);
extern int dset_parse_skbprio(struct dset_session *session,
			       enum dset_opt opt, const char *str);
extern int dset_parse_ignored(struct dset_session *session,
			       enum dset_opt opt, const char *str);
extern int dset_parse_elem(struct dset_session *session,
			    bool optional, const char *str);
extern int dset_call_parser(struct dset_session *session,
			     const struct dset_arg *arg,
			     const char *str);

/* Compatibility parser functions */
extern int dset_parse_name_compat(struct dset_session *session,
				   enum dset_opt opt, const char *str);

#ifdef __cplusplus
}
#endif

#endif /* LIBDSET_PARSE_H */
