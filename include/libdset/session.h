/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef LIBDSET_SESSION_H
#define LIBDSET_SESSION_H

#include <stdbool.h>				/* bool */
#include <stdint.h>				/* uintxx_t */
#include <stdio.h>				/* printf */

#include <libdset/linux_domain_set.h>		/* enum dset_cmd */

/* Report and output buffer sizes */
#define DSET_ERRORBUFLEN		1024
#define DSET_OUTBUFLEN			8192

struct dset_session;
struct dset_data;

#ifdef __cplusplus
extern "C" {
#endif

extern struct dset_data *
	dset_session_data(const struct dset_session *session);
extern struct dset_handle *
	dset_session_handle(const struct dset_session *session);
extern const struct dset_type *
	dset_saved_type(const struct dset_session *session);
extern void dset_session_lineno(struct dset_session *session,
				 uint32_t lineno);
extern void * dset_session_printf_private(struct dset_session *session);

enum dset_err_type {
	DSET_NO_ERROR,
	DSET_WARNING,		/* Success code when exit */
	DSET_NOTICE,		/* Error code and exit in non interactive mode */
	DSET_ERROR,		/* Error code and exit */
};

extern int dset_session_report(struct dset_session *session,
				enum dset_err_type type,
				const char *fmt, ...);
extern int dset_session_warning_as_error(struct dset_session *session);

#define dset_err(session, fmt, args...) \
	dset_session_report(session, DSET_ERROR, fmt , ## args)

#define dset_warn(session, fmt, args...) \
	dset_session_report(session, DSET_WARNING, fmt , ## args)

#define dset_notice(session, fmt, args...) \
	dset_session_report(session, DSET_NOTICE, fmt , ## args)

#define dset_errptr(session, fmt, args...) ({				\
	dset_session_report(session, DSET_ERROR, fmt , ## args);	\
	NULL;								\
})

extern void dset_session_report_reset(struct dset_session *session);
extern const char *dset_session_report_msg(const struct dset_session *session);
extern enum dset_err_type dset_session_report_type(
	const struct dset_session *session);

#define dset_session_data_set(session, opt, value)	\
	dset_data_set(dset_session_data(session), opt, value)
#define dset_session_data_get(session, opt)		\
	dset_data_get(dset_session_data(session), opt)

/* Environment option flags */
enum dset_envopt {
	DSET_ENV_BIT_SORTED	= 0,
	DSET_ENV_SORTED	= (1 << DSET_ENV_BIT_SORTED),
	DSET_ENV_BIT_QUIET	= 1,
	DSET_ENV_QUIET		= (1 << DSET_ENV_BIT_QUIET),
	DSET_ENV_BIT_RESOLVE	= 2,
	DSET_ENV_RESOLVE	= (1 << DSET_ENV_BIT_RESOLVE),
	DSET_ENV_BIT_EXIST	= 3,
	DSET_ENV_EXIST		= (1 << DSET_ENV_BIT_EXIST),
	DSET_ENV_BIT_LIST_SETNAME = 4,
	DSET_ENV_LIST_SETNAME	= (1 << DSET_ENV_BIT_LIST_SETNAME),
	DSET_ENV_BIT_LIST_HEADER = 5,
	DSET_ENV_LIST_HEADER	= (1 << DSET_ENV_BIT_LIST_HEADER),
};

extern bool dset_envopt_test(struct dset_session *session,
			      enum dset_envopt env);
extern void dset_envopt_set(struct dset_session *session,
			     enum dset_envopt env);
extern void dset_envopt_unset(struct dset_session *session,
			       enum dset_envopt env);

enum dset_output_mode {
	DSET_LIST_NONE,
	DSET_LIST_PLAIN,
	DSET_LIST_SAVE,
	DSET_LIST_XML,
};

extern int dset_session_output(struct dset_session *session,
				enum dset_output_mode mode);

extern int dset_commit(struct dset_session *session);
extern int dset_cmd(struct dset_session *session, enum dset_cmd cmd,
		     uint32_t lineno);

typedef int (*dset_print_outfn)(struct dset_session *session,
	void *p, const char *fmt, ...)
	__attribute__ ((format (printf, 3, 4)));

extern int dset_session_print_outfn(struct dset_session *session,
				     dset_print_outfn outfn,
				     void *p);

enum dset_io_type {
	DSET_IO_INPUT,
	DSET_IO_OUTPUT,
};

extern int dset_session_io_full(struct dset_session *session,
		const char *filename, enum dset_io_type what);
extern int dset_session_io_normal(struct dset_session *session,
		const char *filename, enum dset_io_type what);
extern FILE * dset_session_io_stream(struct dset_session *session,
				      enum dset_io_type what);
extern int dset_session_io_close(struct dset_session *session,
				  enum dset_io_type what);

extern struct dset_session *dset_session_init(dset_print_outfn outfn,
						void *p);
extern int dset_session_fini(struct dset_session *session);

extern void dset_debug_msg(const char *dir, void *buffer, int len);

#ifdef __cplusplus
}
#endif

#endif /* LIBDSET_SESSION_H */
