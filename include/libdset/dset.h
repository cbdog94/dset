/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef LIBDSET_DSET_H
#define LIBDSET_DSET_H

#include <stdbool.h>				/* bool */
#include <libdset/linux_domain_set.h>		/* enum dset_cmd */
#include <libdset/session.h>			/* dset_session_* */
#include <libdset/types.h>			/* dset_load_types */

#define DSET_CMD_ALIASES	3

/* Commands in userspace */
struct dset_commands {
	enum dset_cmd cmd;
	int has_arg;
	const char *name[DSET_CMD_ALIASES];
	const char *help;
};

#ifdef __cplusplus
extern "C" {
#endif

extern const struct dset_commands dset_commands[];

struct dset_session;
struct dset_data;
struct dset;


/* Environment options */
struct dset_envopts {
	int flag;
	int has_arg;
	const char *name[2];
	const char *help;
	int (*parse)(struct dset *dset, int flag, const char *str);
	int (*print)(char *buf, unsigned int len,
		     const struct dset_data *data, int flag, uint8_t env);
};

extern const struct dset_envopts dset_envopts[];

extern bool dset_match_cmd(const char *arg, const char * const name[]);
extern bool dset_match_option(const char *arg, const char * const name[]);
extern bool dset_match_envopt(const char *arg, const char * const name[]);
extern int dset_parse_filename(struct dset *dset, int opt, const char *str);
extern int dset_parse_output(struct dset *dset,
			      int opt, const char *str);
extern int dset_envopt_parse(struct dset *dset,
			      int env, const char *str);

enum dset_exittype {
	DSET_NO_PROBLEM = 0,
	DSET_OTHER_PROBLEM,
	DSET_PARAMETER_PROBLEM,
	DSET_VERSION_PROBLEM,
	DSET_SESSION_PROBLEM,
};

typedef int (*dset_custom_errorfn)(struct dset *dset, void *p,
	int status, const char *msg, ...)
	__attribute__ ((format (printf, 4, 5)));
typedef int (*dset_standard_errorfn)(struct dset *dset, void *p);

extern struct dset_session * dset_session(struct dset *dset);
extern bool dset_is_interactive(struct dset *dset);
extern int dset_custom_printf(struct dset *dset,
	dset_custom_errorfn custom_error,
	dset_standard_errorfn standard_error,
	dset_print_outfn outfn,
	void *p);

extern int dset_parse_argv(struct dset *dset, int argc, char *argv[]);
extern int dset_parse_line(struct dset *dset, char *line);
extern int dset_parse_stream(struct dset *dset, FILE *f);
extern struct dset * dset_init(void);
extern int dset_fini(struct dset *dset);

#ifdef __cplusplus
}
#endif

#endif /* LIBDSET_DSET_H */
