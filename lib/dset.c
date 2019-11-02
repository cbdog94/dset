/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <assert.h>  /* assert */
#include <ctype.h>   /* isspace */
#include <errno.h>   /* errno */
#include <stdarg.h>  /* va_* */
#include <stdbool.h> /* bool */
#include <stdio.h>   /* printf */
#include <stdlib.h>  /* exit */
#include <string.h>  /* str* */

#include <config.h>

#include <libdset/debug.h>			  /* D() */
#include <libdset/linux_domain_set.h> /* DSET_CMD_* */
#include <libdset/data.h>			  /* enum dset_data */
#include <libdset/types.h>			  /* DSET_*_ARG */
#include <libdset/session.h>		  /* dset_envopt_parse */
#include <libdset/parse.h>			  /* dset_parse_family */
#include <libdset/print.h>			  /* dset_print_family */
#include <libdset/utils.h>			  /* STREQ */
#include <libdset/dset.h>			  /* prototypes */

static char program_name[] = PACKAGE;
static char program_version[] = PACKAGE_VERSION;

#define MAX_CMDLINE_CHARS 1024
#define MAX_ARGS 32

/* The dset structure */
struct dset
{
	dset_custom_errorfn custom_error;
	/* Custom error message function */
	dset_standard_errorfn standard_error;
	/* Standard error message function */
	struct dset_session *session;	/* Session */
	uint32_t restore_line;			 /* Restore lineno */
	bool interactive;				 /* "Interactive" CLI */
	bool full_io;					 /* Use session ios */
	bool no_vhi;					 /* No version/help/interactive */
	char cmdline[MAX_CMDLINE_CHARS]; /* For restore mode */
	char *newargv[MAX_ARGS];
	int newargc;
	const char *filename; /* Input/output filename */
};

/* Commands and environment options */

const struct dset_commands dset_commands[] = {
	/* Order is important */

	{
		/* c[reate], --create, n[ew], -N */
		.cmd = DSET_CMD_CREATE,
		.name = {"create", "new", "-N"},
		.has_arg = DSET_MANDATORY_ARG2,
		.help = "SETNAME TYPENAME [type-specific-options]\n"
				"        Create a new set",
	},
	{
		/* a[dd], --add, -A  */
		.cmd = DSET_CMD_ADD,
		.name = {"add", "-A", NULL},
		.has_arg = DSET_MANDATORY_ARG2,
		.help = "SETNAME ENTRY\n"
				"        Add entry to the named set",
	},
	{
		/* d[el], --del, -D */
		.cmd = DSET_CMD_DEL,
		.name = {"del", "-D", NULL},
		.has_arg = DSET_MANDATORY_ARG2,
		.help = "SETNAME ENTRY\n"
				"        Delete entry from the named set",
	},
	{
		/* t[est], --test, -T */
		.cmd = DSET_CMD_TEST,
		.name = {"test", "-T", NULL},
		.has_arg = DSET_MANDATORY_ARG2,
		.help = "SETNAME ENTRY\n"
				"        Test entry in the named set",
	},
	{
		/* des[troy], --destroy, x, -X */
		.cmd = DSET_CMD_DESTROY,
		.name = {"destroy", "x", "-X"},
		.has_arg = DSET_OPTIONAL_ARG,
		.help = "[SETNAME]\n"
				"        Destroy a named set or all sets",
	},
	{
		/* l[ist], --list, -L */
		.cmd = DSET_CMD_LIST,
		.name = {"list", "-L", NULL},
		.has_arg = DSET_OPTIONAL_ARG,
		.help = "[SETNAME]\n"
				"        List the entries of a named set or all sets",
	},
	{
		/* s[save], --save, -S */
		.cmd = DSET_CMD_SAVE,
		.name = {"save", "-S", NULL},
		.has_arg = DSET_OPTIONAL_ARG,
		.help = "[SETNAME]\n"
				"        Save the named set or all sets to stdout",
	},
	{
		/* r[estore], --restore, -R */
		.cmd = DSET_CMD_RESTORE,
		.name = {"restore", "-R", NULL},
		.has_arg = DSET_NO_ARG,
		.help = "\n"
				"        Restore a saved state",
	},
	{
		/* f[lush], --flush, -F */
		.cmd = DSET_CMD_FLUSH,
		.name = {"flush", "-F", NULL},
		.has_arg = DSET_OPTIONAL_ARG,
		.help = "[SETNAME]\n"
				"        Flush a named set or all sets",
	},
	{
		/* ren[ame], --rename, e, -E */
		.cmd = DSET_CMD_RENAME,
		.name = {"rename", "e", "-E"},
		.has_arg = DSET_MANDATORY_ARG2,
		.help = "FROM-SETNAME TO-SETNAME\n"
				"        Rename two sets",
	},
	{
		/* sw[ap], --swap, w, -W */
		.cmd = DSET_CMD_SWAP,
		.name = {"swap", "w", "-W"},
		.has_arg = DSET_MANDATORY_ARG2,
		.help = "FROM-SETNAME TO-SETNAME\n"
				"        Swap the contect of two existing sets",
	},
	{
		/* h[elp, --help, -H */
		.cmd = DSET_CMD_HELP,
		.name = {"help", "-h", "-H"},
		.has_arg = DSET_OPTIONAL_ARG,
		.help = "[TYPENAME]\n"
				"        Print help, and settype specific help",
	},
	{
		/* v[ersion], --version, -V */
		.cmd = DSET_CMD_VERSION,
		.name = {"version", "-v", "-V"},
		.has_arg = DSET_NO_ARG,
		.help = "\n"
				"        Print version information",
	},
	{
		/* q[uit] */
		.cmd = DSET_CMD_QUIT,
		.name = {"quit", NULL},
		.has_arg = DSET_NO_ARG,
		.help = "\n"
				"        Quit interactive mode",
	},
	{},
};

/**
 * dset_match_cmd - try to match as a prefix or letter-command
 * @arg: possible command string
 * @name: command and it's aliases
 *
 * Returns true if @arg is a known command.
 */
bool dset_match_cmd(const char *arg, const char *const name[])
{
	size_t len, skip = 0;
	int i;

	assert(arg);
	assert(name && name[0]);

	/* Ignore two leading dashes */
	if (arg[0] == '-' && arg[1] == '-')
		skip = 2;

	len = strlen(arg);
	if (len <= skip || (len == 1 && arg[0] == '-'))
		return false;

	for (i = 0; i < DSET_CMD_ALIASES && name[i] != NULL; i++)
	{
		/* New command name options */
		if (STRNEQ(arg + skip, name[i], len - skip))
			return true;
	}
	return false;
}

/* Used up so far
 *
 *	-A		add
 *	-D		del
 *	-E		rename
 *	-f		-file
 *	-F		flush
 *	-h		help
 *	-H		help
 *	-L		list
 *	-n		-name
 *	-N		create
 *	-o		-output
 *	-r		-resolve
 *	-R		restore
 *	-s		-sorted
 *	-S		save
 *	-t		-terse
 *	-T		test
 *	-q		-quiet
 *	-X		destroy
 *	-v		version
 *	-V		version
 *	-W		swap
 *	-!		-exist
 */

const struct dset_envopts dset_envopts[] = {
	{
		.name = {"-o", "-output"},
		.has_arg = DSET_MANDATORY_ARG,
		.flag = DSET_OPT_MAX,
		.parse = dset_parse_output,
		.help = "plain|save|xml\n"
				"       Specify output mode for listing sets.\n"
				"       Default value for \"list\" command is mode \"plain\"\n"
				"       and for \"save\" command is mode \"save\".",
	},
	{
		.name = {"-s", "-sorted"},
		.parse = dset_envopt_parse,
		.has_arg = DSET_NO_ARG,
		.flag = DSET_ENV_SORTED,
		.help = "\n"
				"        Print elements sorted (if supported by the set type).",
	},
	{
		.name = {"-q", "-quiet"},
		.parse = dset_envopt_parse,
		.has_arg = DSET_NO_ARG,
		.flag = DSET_ENV_QUIET,
		.help = "\n"
				"        Suppress any notice or warning message.",
	},
	{
		.name = {"-!", "-exist"},
		.parse = dset_envopt_parse,
		.has_arg = DSET_NO_ARG,
		.flag = DSET_ENV_EXIST,
		.help = "\n"
				"        Ignore errors when creating or adding sets or\n"
				"        elements that do exist or when deleting elements\n"
				"        that don't exist.",
	},
	{
		.name = {"-n", "-name"},
		.parse = dset_envopt_parse,
		.has_arg = DSET_NO_ARG,
		.flag = DSET_ENV_LIST_SETNAME,
		.help = "\n"
				"        When listing, just list setnames from the kernel.\n",
	},
	{
		.name = {"-t", "-terse"},
		.parse = dset_envopt_parse,
		.has_arg = DSET_NO_ARG,
		.flag = DSET_ENV_LIST_HEADER,
		.help = "\n"
				"        When listing, list setnames and set headers\n"
				"        from kernel only.",
	},
	{
		.name = {"-f", "-file"},
		.parse = dset_parse_filename,
		.has_arg = DSET_MANDATORY_ARG,
		.flag = DSET_OPT_MAX,
		.help = "\n"
				"        Read from the given file instead of standard\n"
				"        input (restore) or write to given file instead\n"
				"        of standard output (list/save).",
	},
	{},
};

/**
 * dset_match_option - strict option matching
 * @arg: possible option string
 * @name: known option and it's alias
 *
 * Two leading dashes are ignored.
 *
 * Returns true if @arg is a known option.
 */
bool dset_match_option(const char *arg, const char *const name[])
{
	assert(arg);
	assert(name && name[0]);

	/* Skip two leading dashes */
	if (arg[0] == '-' && arg[1] == '-')
		arg++, arg++;

	return STREQ(arg, name[0]) ||
		   (name[1] != NULL && STREQ(arg, name[1]));
}

/**
 * dset_match_envopt - strict envopt matching
 * @arg: possible envopt string
 * @name: known envopt and it's alias
 *
 * One leading dash is ignored.
 *
 * Returns true if @arg is a known envopt.
 */
bool dset_match_envopt(const char *arg, const char *const name[])
{
	assert(arg);
	assert(name && name[0]);

	/* Skip one leading dash */
	if (arg[0] == '-' && arg[1] == '-')
		arg++;

	return STREQ(arg, name[0]) ||
		   (name[1] != NULL && STREQ(arg, name[1]));
}

static void
dset_shift_argv(int *argc, char *argv[], int from)
{
	int i;

	assert(*argc >= from + 1);

	for (i = from + 1; i <= *argc; i++)
		argv[i - 1] = argv[i];
	(*argc)--;
	return;
}

/**
 * dset_parse_filename - parse filename
 * @dset: dset structure
 * @opt: option kind of the data
 * @str: string to parse
 *
 * Parse filename of "-file" option, which can be used once only.
 *
 * Returns 0 on success or a negative error code.
 */
int dset_parse_filename(struct dset *dset,
						int opt UNUSED, const char *str)
{
	void *p = dset_session_printf_private(dset->session);

	if (dset->filename)
		return dset->custom_error(dset, p, DSET_PARAMETER_PROBLEM,
								  "-file option cannot be used when full io is activated");
	dset->filename = str;

	return 0;
}

/**
 * dset_parse_output - parse output format name
 * @dset: dset structure
 * @opt: option kind of the data
 * @str: string to parse
 *
 * Parse output format names and set session mode.
 * The value is stored in the session.
 *
 * Returns 0 on success or a negative error code.
 */
int dset_parse_output(struct dset *dset,
					  int opt UNUSED, const char *str)
{
	struct dset_session *session;

	assert(dset);
	assert(str);

	session = dset_session(dset);
	if (STREQ(str, "plain"))
		return dset_session_output(session, DSET_LIST_PLAIN);
	else if (STREQ(str, "xml"))
		return dset_session_output(session, DSET_LIST_XML);
	else if (STREQ(str, "save"))
		return dset_session_output(session, DSET_LIST_SAVE);

	return dset_err(session,
					"Syntax error: unknown output mode '%s'", str);
}

/**
 * dset_envopt_parse - parse/set environment option
 * @dset: dset structure
 * @opt: environment option
 * @arg: option argument (unused)
 *
 * Parse and set an environment option.
 *
 * Returns 0 on success or a negative error code.
 */
int dset_envopt_parse(struct dset *dset, int opt,
					  const char *arg UNUSED)
{
	struct dset_session *session;
	assert(dset);

	session = dset_session(dset);
	switch (opt)
	{
	case DSET_ENV_SORTED:
	case DSET_ENV_QUIET:
	case DSET_ENV_RESOLVE:
	case DSET_ENV_EXIST:
	case DSET_ENV_LIST_SETNAME:
	case DSET_ENV_LIST_HEADER:
		dset_envopt_set(session, opt);
		return 0;
	default:
		break;
	}
	return -1;
}

static int __attribute__((format(printf, 4, 5)))
default_custom_error(struct dset *dset, void *p UNUSED,
					 int status, const char *msg, ...)
{
	struct dset_session *session = dset_session(dset);
	bool is_interactive = dset_is_interactive(dset);
	bool quiet = !is_interactive &&
				 session &&
				 dset_envopt_test(session, DSET_ENV_QUIET);

	if (status && msg && !quiet)
	{
		va_list args;

		fprintf(stderr, "%s v%s: ", program_name, program_version);
		va_start(args, msg);
		vfprintf(stderr, msg, args);
		va_end(args);
		if (status != DSET_SESSION_PROBLEM)
			fprintf(stderr, "\n");

		if (status == DSET_PARAMETER_PROBLEM)
			fprintf(stderr,
					"Try `%s help' for more information.\n",
					program_name);
	}
	/* Ignore errors in interactive mode */
	if (status && is_interactive)
	{
		if (session)
			dset_session_report_reset(session);
		return -1;
	}

	D("status: %u", status);
	dset_fini(dset);
	exit(status > DSET_VERSION_PROBLEM ? DSET_OTHER_PROBLEM : status);
	/* Unreached */
	return -1;
}

static int
default_standard_error(struct dset *dset, void *p)
{
	struct dset_session *session = dset_session(dset);
	bool is_interactive = dset_is_interactive(dset);
	enum dset_err_type err_type = dset_session_report_type(session);

	if ((err_type == DSET_WARNING || err_type == DSET_NOTICE) &&
		!dset_envopt_test(session, DSET_ENV_QUIET))
		fprintf(stderr, "%s%s",
				err_type == DSET_WARNING ? "Warning: " : "",
				dset_session_report_msg(session));
	if (err_type == DSET_ERROR)
		return dset->custom_error(dset, p,
								  DSET_SESSION_PROBLEM, "%s",
								  dset_session_report_msg(session));

	if (!is_interactive)
	{
		dset_fini(dset);
		/* Warnings are not errors */
		exit(err_type <= DSET_WARNING ? 0 : DSET_OTHER_PROBLEM);
	}

	dset_session_report_reset(session);
	return -1;
}

static void
default_help(void)
{
	const struct dset_commands *c;
	const struct dset_envopts *opt = dset_envopts;

	printf("%s v%s\n\n"
		   "Usage: %s [options] COMMAND\n\nCommands:\n",
		   program_name, program_version, program_name);

	for (c = dset_commands; c->cmd; c++)
		printf("%s %s\n", c->name[0], c->help);
	printf("\nOptions:\n");

	while (opt->flag)
	{
		if (opt->help)
			printf("%s %s\n", opt->name[0], opt->help);
		opt++;
	}
}

static void
reset_argv(struct dset *dset)
{
	int i;

	/* Reset */
	for (i = 1; i < dset->newargc; i++)
	{
		if (dset->newargv[i])
			free(dset->newargv[i]);
		dset->newargv[i] = NULL;
	}
	dset->newargc = 1;
}

/* Build fake argv from parsed line */
static int
build_argv(struct dset *dset, char *buffer)
{
	void *p = dset_session_printf_private(dset->session);
	char *tmp, *arg;
	int i;
	bool quoted = false;

	reset_argv(dset);
	arg = calloc(strlen(buffer) + 1, sizeof(*buffer));
	if (!arg)
		return dset->custom_error(dset, p, DSET_OTHER_PROBLEM,
								  "Cannot allocate memory.");
	for (tmp = buffer, i = 0; *tmp; tmp++)
	{
		if ((dset->newargc + 1) ==
			(int)(sizeof(dset->newargv) / sizeof(char *)))
		{
			free(arg);
			return dset->custom_error(dset,
									  p, DSET_PARAMETER_PROBLEM,
									  "Line is too long to parse.");
		}
		switch (*tmp)
		{
		case '"':
			quoted = !quoted;
			if (*(tmp + 1))
				continue;
			break;
		case ' ':
		case '\r':
		case '\n':
		case '\t':
			if (!quoted)
				break;
			arg[i++] = *tmp;
			continue;
		default:
			arg[i++] = *tmp;
			if (*(tmp + 1))
				continue;
			break;
		}
		if (!*(tmp + 1) && quoted)
		{
			free(arg);
			return dset->custom_error(dset,
									  p, DSET_PARAMETER_PROBLEM,
									  "Missing close quote!");
		}
		if (!*arg)
			continue;
		dset->newargv[dset->newargc] =
			calloc(strlen(arg) + 1, sizeof(*arg));
		if (!dset->newargv[dset->newargc])
		{
			free(arg);
			return dset->custom_error(dset,
									  p, DSET_OTHER_PROBLEM,
									  "Cannot allocate memory.");
		}
		dset_strlcpy(dset->newargv[dset->newargc++],
					 arg, strlen(arg) + 1);
		memset(arg, 0, strlen(arg) + 1);
		i = 0;
	}

	free(arg);
	return 0;
}

static int
restore(struct dset *dset)
{
	struct dset_session *session = dset_session(dset);
	int ret = 0;
	FILE *f = stdin; /* Default from stdin */

	if (dset->filename)
	{
		ret = dset_session_io_normal(session, dset->filename,
									 DSET_IO_INPUT);
		if (ret < 0)
			return ret;
		f = dset_session_io_stream(session, DSET_IO_INPUT);
	}
	return dset_parse_stream(dset, f);
}

static bool do_parse(const struct dset_arg *arg, bool family)
{
	return family != true;
}

static int
call_parser(struct dset *dset, int *argc, char *argv[],
			const struct dset_type *type, enum dset_adt cmd, bool family)
{
	void *p = dset_session_printf_private(dset->session);
	const struct dset_arg *arg;
	const char *optstr;
	const struct dset_type *t = type;
	uint8_t revision = type->revision;
	int ret = 0, i = 1, j;

	/* Currently CREATE and ADT may have got additional arguments */
	if (type->cmd[cmd].args[0] == DSET_ARG_NONE && *argc > 1)
		return dset->custom_error(dset,
								  p, DSET_PARAMETER_PROBLEM,
								  "Unknown argument: `%s'", argv[i]);

	while (*argc > i)
	{
		ret = -1;
		for (j = 0; type->cmd[cmd].args[j] != DSET_ARG_NONE; j++)
		{
			arg = dset_keyword(type->cmd[cmd].args[j]);
			D("argc: %u, %s vs %s", i, argv[i], arg->name[0]);
			if (!(dset_match_option(argv[i], arg->name)))
				continue;

			optstr = argv[i];
			/* Matched option */
			D("match %s, argc %u, i %u, %s",
			  arg->name[0], *argc, i + 1,
			  do_parse(arg, family) ? "parse" : "skip");
			i++;
			ret = 0;
			switch (arg->has_arg)
			{
			case DSET_MANDATORY_ARG:
				if (*argc - i < 1)
					return dset->custom_error(dset, p,
											  DSET_PARAMETER_PROBLEM,
											  "Missing mandatory argument "
											  "of option `%s'",
											  arg->name[0]);
				/* Fall through */
			case DSET_OPTIONAL_ARG:
				if (*argc - i >= 1)
				{
					if (do_parse(arg, family))
					{
						ret = dset_call_parser(
							dset->session,
							arg, argv[i]);
						if (ret < 0)
							return ret;
					}
					i++;
					break;
				}
				/* Fall through */
			default:
				if (do_parse(arg, family))
				{
					ret = dset_call_parser(
						dset->session, arg, optstr);
					if (ret < 0)
						return ret;
				}
			}
			break;
		}
		if (ret < 0)
			goto err_unknown;
	}
	if (!family)
		*argc = 0;
	return ret;

err_unknown:
	while ((type = dset_type_higher_rev(t)) != t)
	{
		for (j = 0; type->cmd[cmd].args[j] != DSET_ARG_NONE; j++)
		{
			arg = dset_keyword(type->cmd[cmd].args[j]);
			D("argc: %u, %s vs %s", i, argv[i], arg->name[0]);
			if (dset_match_option(argv[i], arg->name))
				return dset->custom_error(dset, p,
										  DSET_PARAMETER_PROBLEM,
										  "Argument `%s' is supported in the kernel module "
										  "of the set type %s starting from the revision %u "
										  "and you have installed revision %u only. "
										  "Your kernel is behind your dset utility.",
										  argv[i], type->name,
										  type->revision, revision);
		}
		t = type;
	}
	return dset->custom_error(dset, p, DSET_PARAMETER_PROBLEM,
							  "Unknown argument: `%s'", argv[i]);
}

static enum dset_adt
cmd2cmd(int cmd)
{
	switch (cmd)
	{
	case DSET_CMD_ADD:
		return DSET_ADD;
	case DSET_CMD_DEL:
		return DSET_DEL;
	case DSET_CMD_TEST:
		return DSET_TEST;
	case DSET_CMD_CREATE:
		return DSET_CREATE;
	default:
		return 0;
	}
}

static void
check_mandatory(struct dset *dset,
				const struct dset_type *type, enum dset_cmd command)
{
	enum dset_adt cmd = cmd2cmd(command);
	struct dset_session *session = dset->session;
	void *p = dset_session_printf_private(session);
	uint64_t flags = dset_data_flags(dset_session_data(session));
	uint64_t mandatory = type->cmd[cmd].need;
	const struct dset_arg *arg;
	int i;

	mandatory &= ~flags;
	if (!mandatory)
		return;
	if (type->cmd[cmd].args[0] == DSET_ARG_NONE)
	{
		dset->custom_error(dset, p, DSET_OTHER_PROBLEM,
						   "There are missing mandatory flags "
						   "but can't check them. "
						   "It's a bug, please report the problem.");
		return;
	}

	for (i = 0; type->cmd[cmd].args[i] != DSET_ARG_NONE; i++)
	{
		arg = dset_keyword(type->cmd[cmd].args[i]);
		if (mandatory & DSET_FLAG(arg->opt))
		{
			dset->custom_error(dset, p, DSET_PARAMETER_PROBLEM,
							   "Mandatory option `%s' is missing",
							   arg->name[0]);
			return;
		}
	}
}

static const char *
cmd2name(enum dset_cmd cmd)
{
	const struct dset_commands *c;

	for (c = dset_commands; c->cmd; c++)
		if (cmd == c->cmd)
			return c->name[0];
	return "unknown command";
}

static const char *
session_family(struct dset_session *session)
{
	return "unspec";
}

static void
check_allowed(struct dset *dset,
			  const struct dset_type *type, enum dset_cmd command)
{
	struct dset_session *session = dset->session;
	void *p = dset_session_printf_private(session);
	uint64_t flags = dset_data_flags(dset_session_data(session));
	enum dset_adt cmd = cmd2cmd(command);
	uint64_t allowed = type->cmd[cmd].full;
	uint64_t cmdflags = command == DSET_CMD_CREATE
							? DSET_CREATE_FLAGS
							: DSET_ADT_FLAGS;
	const struct dset_arg *arg;
	enum dset_opt i;
	int j;

	for (i = DSET_OPT_DOMAIN; i < DSET_OPT_FLAGS; i++)
	{
		if (!(cmdflags & DSET_FLAG(i)) ||
			(allowed & DSET_FLAG(i)) ||
			!(flags & DSET_FLAG(i)))
			continue;

		/* Other options */
		if (type->cmd[cmd].args[0] == DSET_ARG_NONE)
		{
			dset->custom_error(dset, p, DSET_OTHER_PROBLEM,
							   "There are not allowed options (%u) "
							   "but option list is empty. "
							   "It's a bug, please report the problem.",
							   i);
			return;
		}
		for (j = 0; type->cmd[cmd].args[j] != DSET_ARG_NONE; j++)
		{
			arg = dset_keyword(type->cmd[cmd].args[j]);
			if (arg->opt != i)
				continue;
			dset->custom_error(dset, p, DSET_OTHER_PROBLEM,
							   "%s parameter is not allowed in command %s "
							   "with set type %s and family %s",
							   arg->name[0],
							   cmd2name(command), type->name,
							   session_family(dset->session));
			return;
		}
		dset->custom_error(dset, p, DSET_OTHER_PROBLEM,
						   "There are not allowed options (%u) "
						   "but can't resolve them. "
						   "It's a bug, please report the problem.",
						   i);
		return;
	}
}

static const struct dset_type *
type_find(const char *name)
{
	const struct dset_type *t = dset_types();

	while (t)
	{
		if (dset_match_typename(name, t))
			return t;
		t = t->next;
	}
	return NULL;
}

static enum dset_adt cmd_help_order[] = {
	DSET_CREATE,
	DSET_ADD,
	DSET_DEL,
	DSET_TEST,
	DSET_CADT_MAX,
};

static const char *cmd_prefix[] = {
	[DSET_CREATE] = "create SETNAME",
	[DSET_ADD] = "add    SETNAME",
	[DSET_DEL] = "del    SETNAME",
	[DSET_TEST] = "test   SETNAME",
};

/* Workhorses */

/**
 * dset_parse_argv - parse and argv array and execute the command
 * @dset: dset structure
 * @argc: length of the array
 * @argv: array of strings
 *
 * Parse an array of strings and execute the dset command.
 *
 * Returns 0 on success or a negative error code.
 */
int dset_parse_argv(struct dset *dset, int oargc, char *oargv[])
{
	int ret = 0;
	enum dset_cmd cmd = DSET_CMD_NONE;
	int i;
	char *arg0 = NULL, *arg1 = NULL;
	const struct dset_envopts *opt;
	const struct dset_commands *command;
	const struct dset_type *type;
	struct dset_session *session = dset->session;
	void *p = dset_session_printf_private(session);
	int argc = oargc;
	char *argv[MAX_ARGS] = {};

	/* We need a local copy because of dset_shift_argv */
	memcpy(argv, oargv, sizeof(char *) * argc);

	/* Set session lineno to report parser errors correctly */
	dset_session_lineno(session, dset->restore_line);

	/* Commandline parsing, somewhat similar to that of 'ip' */

	/* First: parse core options */
	for (opt = dset_envopts; opt->flag; opt++)
	{
		for (i = 1; i < argc;)
		{
			if (!dset_match_envopt(argv[i], opt->name))
			{
				i++;
				continue;
			}
			/* Shift off matched option */
			dset_shift_argv(&argc, argv, i);
			switch (opt->has_arg)
			{
			case DSET_MANDATORY_ARG:
				if (i + 1 > argc)
					return dset->custom_error(dset, p,
											  DSET_PARAMETER_PROBLEM,
											  "Missing mandatory argument "
											  "to option %s",
											  opt->name[0]);
				/* Fall through */
			case DSET_OPTIONAL_ARG:
				if (i + 1 <= argc)
				{
					ret = opt->parse(dset, opt->flag,
									 argv[i]);
					if (ret < 0)
						return dset->standard_error(dset, p);
					dset_shift_argv(&argc, argv, i);
				}
				break;
			case DSET_NO_ARG:
				ret = opt->parse(dset, opt->flag,
								 opt->name[0]);
				if (ret < 0)
					return dset->standard_error(dset, p);
				break;
			default:
				break;
			}
		}
	}

	/* Second: parse command */
	for (command = dset_commands;
		 argc > 1 && command->cmd && cmd == DSET_CMD_NONE;
		 command++)
	{
		if (!dset_match_cmd(argv[1], command->name))
			continue;

		if (dset->restore_line != 0 &&
			(command->cmd == DSET_CMD_RESTORE ||
			 command->cmd == DSET_CMD_VERSION ||
			 command->cmd == DSET_CMD_HELP))
			return dset->custom_error(dset, p,
									  DSET_PARAMETER_PROBLEM,
									  "Command `%s' is invalid "
									  "in restore mode.",
									  command->name[0]);
		if (dset->interactive && command->cmd == DSET_CMD_RESTORE)
		{
			printf("Restore command is not supported "
				   "in interactive mode\n");
			return 0;
		}

		/* Shift off matched command arg */
		dset_shift_argv(&argc, argv, 1);
		cmd = command->cmd;
		switch (command->has_arg)
		{
		case DSET_MANDATORY_ARG:
		case DSET_MANDATORY_ARG2:
			if (argc < 2)
				return dset->custom_error(dset, p,
										  DSET_PARAMETER_PROBLEM,
										  "Missing mandatory argument "
										  "to command %s",
										  command->name[0]);
			/* Fall through */
		case DSET_OPTIONAL_ARG:
			arg0 = argv[1];
			if (argc >= 2)
				/* Shift off first arg */
				dset_shift_argv(&argc, argv, 1);
			break;
		default:
			break;
		}
		if (command->has_arg == DSET_MANDATORY_ARG2)
		{
			if (argc < 2)
				return dset->custom_error(dset, p,
										  DSET_PARAMETER_PROBLEM,
										  "Missing second mandatory "
										  "argument to command %s",
										  command->name[0]);
			arg1 = argv[1];
			/* Shift off second arg */
			dset_shift_argv(&argc, argv, 1);
		}
		break;
	}

	/* Third: catch interactive mode, handle help, version */
	switch (cmd)
	{
	case DSET_CMD_NONE:
		if (dset->interactive)
		{
			printf("No command specified\n");
			if (session)
				dset_envopt_parse(dset, 0, "reset");
			return 0;
		}
		if (argc > 1 && STREQ(argv[1], "-"))
		{
			if (dset->no_vhi)
				return 0;
			dset->interactive = true;
			printf("%s> ", program_name);
			while (fgets(dset->cmdline,
						 sizeof(dset->cmdline), stdin))
			{
				/* Execute line: ignore soft errors */
				if (dset_parse_line(dset, dset->cmdline) < 0)
					dset->standard_error(dset, p);
				printf("%s> ", program_name);
			}
			return dset->custom_error(dset, p,
									  DSET_NO_PROBLEM, NULL);
		}
		if (argc > 1)
			return dset->custom_error(dset,
									  p, DSET_PARAMETER_PROBLEM,
									  "No command specified: unknown argument %s",
									  argv[1]);
		return dset->custom_error(dset, p, DSET_PARAMETER_PROBLEM,
								  "No command specified.");
	case DSET_CMD_VERSION:
		if (dset->no_vhi)
			return 0;
		printf("%s v%s, protocol version: %u\n",
			   program_name, program_version, DSET_PROTOCOL);
		/* Check kernel protocol version */
		dset_cmd(session, DSET_CMD_NONE, 0);
		if (dset_session_report_type(session) != DSET_NO_ERROR)
			dset->standard_error(dset, p);
		if (dset->interactive)
			return 0;
		return dset->custom_error(dset, p, DSET_NO_PROBLEM, NULL);
	case DSET_CMD_HELP:
		if (dset->no_vhi)
			return 0;
		default_help();

		if (dset->interactive ||
			!dset_envopt_test(session, DSET_ENV_QUIET))
		{
			if (arg0)
			{
				const struct dset_arg *arg;
				int k;

				/* Type-specific help, without kernel checking */
				type = type_find(arg0);
				if (!type)
					return dset->custom_error(dset, p,
											  DSET_PARAMETER_PROBLEM,
											  "Unknown settype: `%s'", arg0);
				printf("\n%s type specific options:\n\n", type->name);
				for (i = 0; cmd_help_order[i] != DSET_CADT_MAX; i++)
				{
					cmd = cmd_help_order[i];
					printf("%s %s %s\n",
						   cmd_prefix[cmd], type->name, type->cmd[cmd].help);
					for (k = 0; type->cmd[cmd].args[k] != DSET_ARG_NONE; k++)
					{
						arg = dset_keyword(type->cmd[cmd].args[k]);
						if (!arg->help || arg->help[0] == '\0')
							continue;
						printf("               %s\n", arg->help);
					}
				}
				printf("\n%s\n", type->usage);
				if (type->usagefn)
					type->usagefn();
				if (type->family == NFPROTO_UNSPEC)
					printf("\nType %s is family neutral.\n",
						   type->name);
				else if (type->family == NFPROTO_DSET_IPV46)
					printf("\nType %s supports inet "
						   "and inet6.\n",
						   type->name);
				else
					printf("\nType %s supports family "
						   "%s only.\n",
						   type->name,
						   type->family == NFPROTO_IPV4
							   ? "inet"
							   : "inet6");
			}
			else
			{
				printf("\nSupported set types:\n");
				type = dset_types();
				while (type)
				{
					printf("    %s\t%s%u\t%s\n",
						   type->name,
						   strlen(type->name) < 12 ? "\t" : "",
						   type->revision,
						   type->description);
					type = type->next;
				}
			}
		}
		if (dset->interactive)
			return 0;
		return dset->custom_error(dset, p, DSET_NO_PROBLEM, NULL);
	case DSET_CMD_QUIT:
		return dset->custom_error(dset, p, DSET_NO_PROBLEM, NULL);
	default:
		break;
	}

	/* Forth: parse command args and issue the command */
	switch (cmd)
	{
	case DSET_CMD_CREATE:
		/* Args: setname typename [type specific options] */
		ret = dset_parse_setname(session, DSET_SETNAME, arg0);
		if (ret < 0)
			return dset->standard_error(dset, p);

		ret = dset_parse_typename(session, DSET_OPT_TYPENAME, arg1);
		if (ret < 0)
			return dset->standard_error(dset, p);

		type = dset_type_get(session, cmd);
		if (type == NULL)
			return dset->standard_error(dset, p);

		/* Parse create options: first check INET family */
		ret = call_parser(dset, &argc, argv, type, DSET_CREATE, true);
		if (ret < 0)
			return dset->standard_error(dset, p);
		else if (ret)
			return ret;

		/* Parse create options: then check all options */
		ret = call_parser(dset, &argc, argv, type, DSET_CREATE, false);
		if (ret < 0)
			return dset->standard_error(dset, p);
		else if (ret)
			return ret;

		/* Check mandatory, then allowed options */
		check_mandatory(dset, type, cmd);
		check_allowed(dset, type, cmd);

		break;
	case DSET_CMD_LIST:
	case DSET_CMD_SAVE:
		if (dset->filename != NULL)
		{
			ret = dset_session_io_normal(session,
										 dset->filename, DSET_IO_OUTPUT);
			if (ret < 0)
				return ret;
		}
		/* Fall through to parse optional setname */
	case DSET_CMD_DESTROY:
	case DSET_CMD_FLUSH:
		/* Args: [setname] */
		if (arg0)
		{
			ret = dset_parse_setname(session,
									 DSET_SETNAME, arg0);
			if (ret < 0)
				return dset->standard_error(dset, p);
		}
		break;

	case DSET_CMD_RENAME:
	case DSET_CMD_SWAP:
		/* Args: from-setname to-setname */
		ret = dset_parse_setname(session, DSET_SETNAME, arg0);
		if (ret < 0)
			return dset->standard_error(dset, p);
		ret = dset_parse_setname(session, DSET_OPT_SETNAME2, arg1);
		if (ret < 0)
			return dset->standard_error(dset, p);
		break;

	case DSET_CMD_RESTORE:
		/* Restore mode */
		if (argc > 1)
			return dset->custom_error(dset,
									  p, DSET_PARAMETER_PROBLEM,
									  "Unknown argument %s", argv[1]);
		return restore(dset);
	case DSET_CMD_ADD:
	case DSET_CMD_DEL:
	case DSET_CMD_TEST:
		D("ADT: setname %s", arg0);
		/* Args: setname domain [options] */
		ret = dset_parse_setname(session, DSET_SETNAME, arg0);
		if (ret < 0)
			return dset->standard_error(dset, p);

		type = dset_type_get(session, cmd);
		if (type == NULL)
			return dset->standard_error(dset, p);

		ret = dset_parse_elem(session, type->last_elem_optional, arg1);
		if (ret < 0)
			return dset->standard_error(dset, p);

		/* Parse additional ADT options */
		ret = call_parser(dset, &argc, argv, type, cmd2cmd(cmd), false);
		if (ret < 0)
			return dset->standard_error(dset, p);
		else if (ret)
			return ret;

		/* Check mandatory, then allowed options */
		check_mandatory(dset, type, cmd);
		check_allowed(dset, type, cmd);

		break;
	default:
		break;
	}

	if (argc > 1)
		return dset->custom_error(dset, p, DSET_PARAMETER_PROBLEM,
								  "Unknown argument %s", argv[1]);
	ret = dset_cmd(session, cmd, dset->restore_line);
	D("ret %d", ret);
	/* In the case of warning, the return code is success */
	if (ret < 0 || dset_session_report_type(session) > DSET_NO_ERROR)
		dset->standard_error(dset, p);

	return ret;
}

/**
 * dset_parse_line - parse a string as a command line and execute it
 * @dset: dset structure
 * @line: string of line
 *
 * Parse a string as a command line and execute the dset command.
 *
 * Returns 0 on success or a negative error code.
 */
int dset_parse_line(struct dset *dset, char *line)
{
	char *c = line;
	int ret;

	reset_argv(dset);

	while (isspace(c[0]))
		c++;
	if (c[0] == '\0' || c[0] == '#')
	{
		if (dset->interactive)
			printf("%s> ", program_name);
		return 0;
	}
	/* Build fake argv, argc */
	ret = build_argv(dset, c);
	if (ret < 0)
		return ret;
	/* Parse and execute line */
	return dset_parse_argv(dset, dset->newargc, dset->newargv);
}

/**
 * dset_parse_stream - parse an stream and execute the commands
 * @dset: dset structure
 * @f: stream
 *
 * Parse an already opened file as stream and execute the commands.
 *
 * Returns 0 on success or a negative error code.
 */
int dset_parse_stream(struct dset *dset, FILE *f)
{
	struct dset_session *session = dset_session(dset);
	void *p = dset_session_printf_private(session);
	int ret = 0;
	char *c;

	while (fgets(dset->cmdline, sizeof(dset->cmdline), f))
	{
		dset->restore_line++;
		c = dset->cmdline;
		while (isspace(c[0]))
			c++;
		if (c[0] == '\0' || c[0] == '#')
			continue;
		else if (STREQ(c, "COMMIT\n") || STREQ(c, "COMMIT\r\n"))
		{
			ret = dset_commit(dset->session);
			if (ret < 0)
				dset->standard_error(dset, p);
			continue;
		}
		/* Build faked argv, argc */
		ret = build_argv(dset, c);
		if (ret < 0)
			return ret;

		/* Execute line */
		ret = dset_parse_argv(dset, dset->newargc, dset->newargv);
		if (ret < 0)
			dset->standard_error(dset, p);
	}
	/* implicit "COMMIT" at EOF */
	ret = dset_commit(dset->session);
	if (ret < 0)
		dset->standard_error(dset, p);

	return ret;
}

/**
 * dset_session - returns the session pointer of an dset structure
 * @dset: dset structure
 *
 * Returns the session pointer of an dset structure.
 */
struct dset_session *
dset_session(struct dset *dset)
{
	return dset->session;
}

/**
 * dset_is_interactive - is the interactive mode enabled?
 * @dset: dset structure
 *
 * Returns true if the interactive mode is enabled.
 */
bool dset_is_interactive(struct dset *dset)
{
	return dset->interactive;
}

/**
 * dset_custom_printf - set custom print functions
 * @dset: dset structure
 * @custom_error: custom error function
 * @standard_error: standard error function
 * @print_outfn: output/printing function
 * @p: pointer to private data area
 *
 * The function makes possible to set custom error and
 * output functions for the library. The private data
 * pointer can be used to pass arbitrary data to these functions.
 * If a function argument is NULL, the default printing function is set.
 *
 * Returns 0 on success or a negative error code.
 */
int dset_custom_printf(struct dset *dset,
					   dset_custom_errorfn custom_error,
					   dset_standard_errorfn standard_error,
					   dset_print_outfn print_outfn,
					   void *p)
{
	dset->no_vhi = !!(custom_error || standard_error || print_outfn);
	dset->custom_error =
		custom_error ? custom_error : default_custom_error;
	dset->standard_error =
		standard_error ? standard_error : default_standard_error;

	return dset_session_print_outfn(dset->session, print_outfn, p);
}

/**
 * dset_init - initialize dset library interface
 *
 * Initialize the dset library interface.
 *
 * Returns the created dset structure for success or NULL for failure.
 */
struct dset *
dset_init(void)
{
	struct dset *dset;

	dset = calloc(1, sizeof(struct dset));
	if (dset == NULL)
		return NULL;
	dset->newargv[0] =
		calloc(strlen(program_name) + 1, sizeof(*program_name));
	if (!dset->newargv[0])
	{
		free(dset);
		return NULL;
	}
	dset_strlcpy(dset->newargv[0], program_name,
				 strlen(program_name) + 1);
	dset->newargc = 1;
	dset->session = dset_session_init(NULL, NULL);
	if (dset->session == NULL)
	{
		free(dset->newargv[0]);
		free(dset);
		return NULL;
	}
	dset_custom_printf(dset, NULL, NULL, NULL, NULL);
	return dset;
}

/**
 * dset_fini - destroy an dset library interface
 * @dset: dset structure
 *
 * Destroys an dset library interface
 *
 * Returns 0 on success or a negative error code.
 */
int dset_fini(struct dset *dset)
{
	assert(dset);

	if (dset->session)
		dset_session_fini(dset->session);
	reset_argv(dset);
	if (dset->newargv[0])
		free(dset->newargv[0]);

	free(dset);
	return 0;
}
