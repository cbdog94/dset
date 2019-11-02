/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <assert.h>		  /* assert */
#include <endian.h>		  /* htobe64 */
#include <errno.h>		  /* errno */
#include <setjmp.h>		  /* setjmp, longjmp */
#include <stdio.h>		  /* snprintf */
#include <stdarg.h>		  /* va_* */
#include <stdbool.h>	  /* bool */
#include <stdlib.h>		  /* free */
#include <string.h>		  /* str* */
#include <unistd.h>		  /* getpagesize */
#include <net/ethernet.h> /* ETH_ALEN */
#include <net/if.h>		  /* IFNAMSIZ */

#include <libdset/compat.h>	/* be64toh() */
#include <libdset/debug.h>	 /* D() */
#include <libdset/data.h>	  /* DSET_OPT_* */
#include <libdset/errcode.h>   /* dset_errcode */
#include <libdset/print.h>	 /* dset_print_* */
#include <libdset/types.h>	 /* struct dset_type */
#include <libdset/transport.h> /* transport */
#include <libdset/mnl.h>	   /* default backend */
#include <libdset/utils.h>	 /* STREQ */
#include <libdset/dset.h>	  /* DSET_ENV_* */
#include <libdset/list_sort.h> /* list_sort */
#include <libdset/session.h>   /* prototypes */

#define DSET_NEST_MAX 4

/* When we want to sort the entries */
struct dset_sorted
{
	struct list_head list;
	size_t offset; /* Offset in outbuf */
};

/* The session structure */
struct dset_session
{
	const struct dset_transport *transport; /* Transport protocol */
	struct dset_handle *handle;				/* Transport handler */
	struct dset_data *data;					/* Input/output data */
	/* Command state */
	enum dset_cmd cmd;					  /* Current command */
	uint32_t lineno;					  /* Current lineno in restore mode */
	uint32_t printed_set;				  /* Printed sets so far */
	char saved_setname[DSET_MAXNAMELEN];  /* Saved setname */
	const struct dset_type *saved_type;   /* Saved type */
	struct nlattr *nested[DSET_NEST_MAX]; /* Pointer to nest levels */
	uint8_t nestid;						  /* Current nest level */
	uint8_t protocol;					  /* The protocol used */
	bool version_checked;				  /* Version checked */
	/* Output buffer */
	char *outbuf;				  /* Output buffer */
	size_t outbuflen;			  /* Output buffer size */
	size_t pos;					  /* Printing position in outbuf */
	struct list_head sorted;	  /* Sorted entries */
	struct list_head pool;		  /* Pool to reuse */
	enum dset_output_mode mode;   /* Output mode */
	dset_print_outfn print_outfn; /* Output function to file */
	void *p;					  /* Private data for print_outfn */
	bool sort;					  /* Print sorted hash:* types */
	/* Session IO */
	bool normal_io, full_io; /* Default/normal/full IO */
	FILE *istream, *ostream; /* Session input/output stream */
	/* Error/warning reporting */
	char report[DSET_ERRORBUFLEN]; /* Error/report buffer */
	enum dset_err_type err_type;   /* ERROR/WARNING/NOTICE */
	uint8_t envopts;			   /* Session env opts */
	/* Kernel message buffer */
	size_t bufsize;
	void *buffer;
};

/*
 * Glue functions
 */

/**
 * dset_session_data - return pointer to the data
 * @session: session structure
 *
 * Returns the pointer to the data structure of the session.
 */
struct dset_data *
dset_session_data(const struct dset_session *session)
{
	assert(session);
	return session->data;
}

/**
 * dset_session_handle - return pointer to the handle
 * @session: session structure
 *
 * Returns the pointer to the transport handle structure of the session.
 */
struct dset_handle *
dset_session_handle(const struct dset_session *session)
{
	assert(session);
	return session->handle;
}

/**
 * dset_saved_type - return pointer to the saved type
 * @session: session structure
 *
 * Returns the pointer to the saved type from the last dset_cmd
 * It is required to decode type-specific error codes in restore mode.
 */
const struct dset_type *
dset_saved_type(const struct dset_session *session)
{
	assert(session);
	return session->saved_type;
}

/**
 * dset_session_lineno - set session lineno
 * @session: session structure
 *
 * Set session lineno to report parser errors correctly.
 */
void dset_session_lineno(struct dset_session *session, uint32_t lineno)
{
	assert(session);
	session->lineno = lineno;
}

/**
 * dset_session_printf_private - returns the session private pointer
 * @session: session structure
 *
 * Returns the private pointer in the session structure,
 * for private/custom print fuctions.
 */
void *
dset_session_printf_private(struct dset_session *session)
{
	assert(session);
	return session->p;
}

/*
 * Environment options
 */

/**
 * dset_envopt_test - test environment option
 * @session: session structure
 * @opt: environment option
 *
 * Test whether the environment option is set in the session.
 *
 * Returns true or false.
 */
bool dset_envopt_test(struct dset_session *session, enum dset_envopt opt)
{
	assert(session);
	return session->envopts & opt;
}

/**
 * dset_envopt_set - set environment option
 * @session: session structure
 * @opt: environment option
 *
 * Set an environment option of the session.
 */
void dset_envopt_set(struct dset_session *session, enum dset_envopt opt)
{
	assert(session);
	session->envopts |= opt;
}

/**
 * dset_envopt_unset - unset environment option
 * @session: session structure
 * @opt: environment option
 *
 * Unset an environment option of the session.
 */
void dset_envopt_unset(struct dset_session *session, enum dset_envopt opt)
{
	assert(session);
	session->envopts &= ~opt;
}

/**
 * dset_session_output - set the session output mode
 * @session: session structure
 * @mode: output mode
 *
 * Set the output mode for the session.
 *
 * Returns 0 on success or a negative error code.
 */
int dset_session_output(struct dset_session *session,
						enum dset_output_mode mode)
{
	assert(session);
	session->mode = mode;
	return 0;
}

/*
 * Error and warning reporting
 */

/**
 * dset_session_report - fill the report buffer
 * @session: session structure
 * @type: report type
 * @fmt: message format
 *
 * Fill the report buffer with an error or warning message.
 * Depending on the report type, set the error or warning
 * message pointer.
 *
 * Returns -1.
 */
int __attribute__((format(printf, 3, 4)))
dset_session_report(struct dset_session *session,
					enum dset_err_type type,
					const char *fmt, ...)
{
	int len, offset = 0;
	va_list args;

	assert(session);
	assert(fmt);

	/* Suppress warning/notice when more important message is required */
	if (session->err_type > DSET_NO_ERROR && session->err_type < type)
		session->report[0] = '\0';

	if (session->lineno != 0 && type == DSET_ERROR)
	{
		sprintf(session->report, "Error in line %u: ",
				session->lineno);
	}
	offset = strlen(session->report);

	va_start(args, fmt);
	len = vsnprintf(session->report + offset,
					DSET_ERRORBUFLEN - 1 - offset,
					fmt, args);
	va_end(args);

	if (len >= DSET_ERRORBUFLEN - 1 - offset)
		session->report[DSET_ERRORBUFLEN - 1] = '\0';
	if (strlen(session->report) < DSET_ERRORBUFLEN - 1)
		strcat(session->report, "\n");

	session->err_type = type;
	if (type == DSET_ERROR)
		dset_data_reset(dset_session_data(session));

	return -1;
}

/**
 * dset_session_warning_as_error - set warning as error
 * @session: session structrure
 *
 * Returns -1.
 */
int dset_session_warning_as_error(struct dset_session *session)
{
	session->err_type = DSET_ERROR;
	dset_data_reset(dset_session_data(session));
	return -1;
}

/**
 * dset_session_reset - reset the report buffer
 * @session: session structure
 *
 * Reset the report buffer, the error and warning pointers.
 */
void dset_session_report_reset(struct dset_session *session)
{
	assert(session);
	session->report[0] = '\0';
	session->err_type = DSET_NO_ERROR;
}

/**
 * dset_session_report_msg - return the report buffer
 * @session: session structure
 *
 * Return the pointer to the report buffer.
 * If there is no error message, the buffer is empty.
 */
const char *
dset_session_report_msg(const struct dset_session *session)
{
	assert(session);

	return session->report;
}

/**
 * dset_session_report_type - return the type of the report
 * @session: session structure
 *
 * Return the type of the message in the report buffer.
 */
enum dset_err_type
dset_session_report_type(const struct dset_session *session)
{
	assert(session);

	return session->err_type;
}

/*
 * Receive data from the kernel
 */

struct dset_attr_policy
{
	uint16_t type;
	uint16_t len;
	enum dset_opt opt;
};

/* Attribute policies and mapping to options */
static const struct dset_attr_policy cmd_attrs[] = {
	[DSET_ATTR_PROTOCOL] = {
		.type = MNL_TYPE_U8,
	},
	[DSET_ATTR_SETNAME] = {
		.type = MNL_TYPE_NUL_STRING,
		.opt = DSET_SETNAME,
		.len = DSET_MAXNAMELEN,
	},
	[DSET_ATTR_TYPENAME] = {
		.type = MNL_TYPE_NUL_STRING,
		.opt = DSET_OPT_TYPENAME,
		.len = DSET_MAXNAMELEN,
	},
	/* DSET_ATTR_SETNAME2 is an alias for DSET_ATTR_TYPENAME */
	[DSET_ATTR_REVISION] = {
		.type = MNL_TYPE_U8,
		.opt = DSET_OPT_REVISION,
	},
	[DSET_ATTR_FAMILY] = {
		.type = MNL_TYPE_U8,
		.opt = DSET_OPT_FAMILY,
	},
	[DSET_ATTR_FLAGS] = {
		.type = MNL_TYPE_U32,
		.opt = DSET_OPT_FLAGS,
	},
	[DSET_ATTR_DATA] = {
		.type = MNL_TYPE_NESTED,
	},
	[DSET_ATTR_ADT] = {
		.type = MNL_TYPE_NESTED,
	},
	[DSET_ATTR_REVISION_MIN] = {
		.type = MNL_TYPE_U8,
		.opt = DSET_OPT_REVISION_MIN,
	},
	/* DSET_ATTR_PROTOCOL_MIN is an alias for DSET_ATTR_REVISION_MIN */
	[DSET_ATTR_LINENO] = {
		.type = MNL_TYPE_U32,
		.opt = DSET_OPT_LINENO,
	},
	[DSET_ATTR_INDEX] = {
		.type = MNL_TYPE_U16,
		.opt = DSET_OPT_INDEX,
	},
};

static const struct dset_attr_policy create_attrs[] = {
	[DSET_ATTR_DOMAIN] = {
		.type = MNL_TYPE_NUL_STRING,
		.opt = DSET_OPT_DOMAIN,
	},
	[DSET_ATTR_TIMEOUT] = {
		.type = MNL_TYPE_U32,
		.opt = DSET_OPT_TIMEOUT,
	},
	[DSET_ATTR_CADT_FLAGS] = {
		.type = MNL_TYPE_U32,
		.opt = DSET_OPT_CADT_FLAGS,
	},
	[DSET_ATTR_GC] = {
		.type = MNL_TYPE_U32,
		.opt = DSET_OPT_GC,
	},
	[DSET_ATTR_HASHSIZE] = {
		.type = MNL_TYPE_U32,
		.opt = DSET_OPT_HASHSIZE,
	},
	[DSET_ATTR_MAXELEM] = {
		.type = MNL_TYPE_U32,
		.opt = DSET_OPT_MAXELEM,
	},
	[DSET_ATTR_PROBES] = {
		.type = MNL_TYPE_U8,
		.opt = DSET_OPT_PROBES,
	},
	[DSET_ATTR_RESIZE] = {
		.type = MNL_TYPE_U8,
		.opt = DSET_OPT_RESIZE,
	},
	[DSET_ATTR_SIZE] = {
		.type = MNL_TYPE_U32,
		.opt = DSET_OPT_SIZE,
	},
	[DSET_ATTR_ELEMENTS] = {
		.type = MNL_TYPE_U32,
		.opt = DSET_OPT_ELEMENTS,
	},
	[DSET_ATTR_REFERENCES] = {
		.type = MNL_TYPE_U32,
		.opt = DSET_OPT_REFERENCES,
	},
	[DSET_ATTR_MEMSIZE] = {
		.type = MNL_TYPE_U32,
		.opt = DSET_OPT_MEMSIZE,
	},
};

static const struct dset_attr_policy adt_attrs[] = {
	[DSET_ATTR_DOMAIN] = {
		.type = MNL_TYPE_NUL_STRING,
		.opt = DSET_OPT_DOMAIN,
		.len = DSET_MAX_DOMAIN_LEN,
	},
	[DSET_ATTR_TIMEOUT] = {
		.type = MNL_TYPE_U32,
		.opt = DSET_OPT_TIMEOUT,
	},
	[DSET_ATTR_CADT_FLAGS] = {
		.type = MNL_TYPE_U32,
		.opt = DSET_OPT_CADT_FLAGS,
	},
	[DSET_ATTR_LINENO] = {
		.type = MNL_TYPE_U32,
		.opt = DSET_OPT_LINENO,
	},
	[DSET_ATTR_NAME] = {
		.type = MNL_TYPE_NUL_STRING,
		.opt = DSET_OPT_NAME,
		.len = DSET_MAXNAMELEN,
	},
	[DSET_ATTR_NAMEREF] = {
		.type = MNL_TYPE_NUL_STRING,
		.opt = DSET_OPT_NAMEREF,
		.len = DSET_MAXNAMELEN,
	},
	[DSET_ATTR_PACKETS] = {
		.type = MNL_TYPE_U64,
		.opt = DSET_OPT_PACKETS,
	},
	[DSET_ATTR_BYTES] = {
		.type = MNL_TYPE_U64,
		.opt = DSET_OPT_BYTES,
	},
	[DSET_ATTR_COMMENT] = {
		.type = MNL_TYPE_NUL_STRING,
		.opt = DSET_OPT_ADT_COMMENT,
		.len = DSET_MAX_COMMENT_SIZE + 1,
	},
	[DSET_ATTR_SKBMARK] = {
		.type = MNL_TYPE_U64,
		.opt = DSET_OPT_SKBMARK,
	},
	[DSET_ATTR_SKBPRIO] = {
		.type = MNL_TYPE_U32,
		.opt = DSET_OPT_SKBPRIO,
	},
	[DSET_ATTR_SKBQUEUE] = {
		.type = MNL_TYPE_U16,
		.opt = DSET_OPT_SKBQUEUE,
	},
	[DSET_ATTR_PAD] = {
		.type = MNL_TYPE_UNSPEC,
		.len = 0,
	},
};

#ifdef DSET_DEBUG
static int debug = 1;
#endif

static int
generic_data_attr_cb(const struct nlattr *attr, void *data,
					 int attr_max, const struct dset_attr_policy *policy)
{
	const struct nlattr **tb = data;
	int type = mnl_attr_get_type(attr);

	IF_D(debug, "attr type: %u, len %u", type, attr->nla_len);
	if (mnl_attr_type_valid(attr, attr_max) < 0)
	{
		IF_D(debug, "attr type: %u INVALID", type);
		return MNL_CB_ERROR;
	}
	if (mnl_attr_validate(attr, policy[type].type) < 0)
	{
		IF_D(debug, "attr type: %u POLICY, attrlen %u", type,
			 mnl_attr_get_payload_len(attr));
		return MNL_CB_ERROR;
	}
	if (policy[type].type == MNL_TYPE_NUL_STRING &&
		mnl_attr_get_payload_len(attr) > policy[type].len)
		return MNL_CB_ERROR;
	tb[type] = attr;
	return MNL_CB_OK;
}

static int
create_attr_cb(const struct nlattr *attr, void *data)
{
	return generic_data_attr_cb(attr, data,
								DSET_ATTR_CREATE_MAX, create_attrs);
}

static int
adt_attr_cb(const struct nlattr *attr, void *data)
{
	return generic_data_attr_cb(attr, data,
								DSET_ATTR_ADT_MAX, adt_attrs);
}

#define FAILURE(format, args...)           \
	{                                      \
		dset_err(session, format, ##args); \
		return MNL_CB_ERROR;               \
	}

static int
attr2data(struct dset_session *session, struct nlattr *nla[],
		  int type, const struct dset_attr_policy attrs[])
{
	struct dset_data *data = session->data;
	const struct dset_attr_policy *attr;
	const void *d;
	uint64_t v64;
	uint32_t v32;
	uint16_t v16;
	int ret;

	attr = &attrs[type];
	d = mnl_attr_get_payload(nla[type]);

	if (attr->type == MNL_TYPE_UNSPEC)
		return 0;

	if (nla[type]->nla_type & NLA_F_NET_BYTEORDER)
	{
		D("netorder attr type %u", type);
		switch (attr->type)
		{
		case MNL_TYPE_U64:
		{
			uint64_t tmp;
			/* Ensure data alignment */
			memcpy(&tmp, d, sizeof(tmp));
			v64 = be64toh(tmp);
			d = &v64;
			break;
		}
		case MNL_TYPE_U32:
		{
			v32 = ntohl(*(const uint32_t *)d);
			d = &v32;
			break;
		}
		case MNL_TYPE_U16:
		{
			v16 = ntohs(*(const uint16_t *)d);
			d = &v16;
			break;
		}
		default:
			break;
		}
	}
	else if (attr->type == MNL_TYPE_NUL_STRING)
	{
		if (!d || strlen(d) >= attr->len)
			FAILURE("Broken kernel message: "
					"string type attribute missing or too long!");
	}
#ifdef DSET_DEBUG
	else
		D("hostorder attr type %u", type);
	if (type == DSET_ATTR_TYPENAME)
		D("nla typename %s", (const char *)d);
#endif
	ret = dset_data_set(data, attr->opt, d);
#ifdef DSET_DEBUG
	if (type == DSET_ATTR_TYPENAME)
		D("nla typename %s",
		  (const char *)dset_data_get(data, DSET_OPT_TYPENAME));
#endif
	return ret;
}

#define ATTR2DATA(session, nla, type, attrs)      \
	if (attr2data(session, nla, type, attrs) < 0) \
	return MNL_CB_ERROR

static const char cmd2name[][9] = {
	[DSET_CMD_NONE] = "NONE",
	[DSET_CMD_CREATE] = "CREATE",
	[DSET_CMD_DESTROY] = "DESTROY",
	[DSET_CMD_FLUSH] = "FLUSH",
	[DSET_CMD_RENAME] = "RENAME",
	[DSET_CMD_SWAP] = "SWAP",
	[DSET_CMD_LIST] = "LIST",
	[DSET_CMD_SAVE] = "SAVE",
	[DSET_CMD_ADD] = "ADD",
	[DSET_CMD_DEL] = "DEL",
	[DSET_CMD_TEST] = "TEST",
	[DSET_CMD_HEADER] = "HEADER",
	[DSET_CMD_TYPE] = "TYPE",
	[DSET_CMD_PROTOCOL] = "PROTOCOL",
};

static inline int
call_outfn(struct dset_session *session)
{
	int ret = session->print_outfn(session, session->p,
								   "%s", session->outbuf);

	session->outbuf[0] = '\0';
	session->pos = 0;

	return ret < 0 ? ret : 0;
}

/* Handle printing failures */
static jmp_buf printf_failure;

static void
realloc_outbuf(struct dset_session *session)
{
	char *buf = realloc(session->outbuf,
						session->outbuflen + DSET_OUTBUFLEN);
	if (!buf)
	{
		dset_err(session,
				 "Could not allocate memory to print sorted!");
		longjmp(printf_failure, 1);
	}
	session->outbuf = buf;
	session->outbuflen += DSET_OUTBUFLEN;
}

static int
handle_snprintf_error(struct dset_session *session,
					  int ret, int loop)
{
	if (ret < 0 || ret + session->pos >= session->outbuflen)
	{
		if (session->sort && !loop)
		{
			realloc_outbuf(session);
			return 1;
		}
		/* Buffer was too small, push it out and retry */
		D("print buffer and try again: outbuflen: %lu, pos %lu, ret: %d",
		  session->outbuflen, session->pos, ret);
		if (loop)
		{
			dset_err(session,
					 "Internal error at printing, loop detected!");
			longjmp(printf_failure, 1);
		}

		session->outbuf[session->pos] = '\0';
		if (call_outfn(session))
		{
			dset_err(session,
					 "Internal error, could not print output buffer!");
			longjmp(printf_failure, 1);
		}
		return 1;
	}
	session->pos += ret;
	return 0;
}

static int __attribute__((format(printf, 2, 3)))
safe_snprintf(struct dset_session *session, const char *fmt, ...)
{
	va_list args;
	int ret, loop = 0;

	do
	{
		D("outbuflen: %lu, pos: %lu, retry %u",
		  session->outbuflen, session->pos, loop);
		va_start(args, fmt);
		ret = vsnprintf(session->outbuf + session->pos,
						session->outbuflen - session->pos,
						fmt, args);
		va_end(args);
		loop = handle_snprintf_error(session, ret, loop);
	} while (loop);

	return ret;
}

static int
safe_dprintf(struct dset_session *session, dset_printfn fn,
			 enum dset_opt opt)
{
	int ret, loop = 0;

	do
	{
		D("outbuflen: %lu, len: %lu, retry %u",
		  session->outbuflen, session->pos, loop);
		ret = fn(session->outbuf + session->pos,
				 session->outbuflen - session->pos,
				 session->data, opt, session->envopts);
		loop = handle_snprintf_error(session, ret, loop);
	} while (loop);

	return ret;
}

static int
list_adt(struct dset_session *session, struct nlattr *nla[])
{
	const struct dset_data *data = session->data;
	const struct dset_type *type;
	const struct dset_arg *arg;
	size_t offset = 0;
	int i, found = 0;

	D("enter");
	/* Check and load type, family */
	if (!dset_data_test(data, DSET_OPT_TYPE))
		type = dset_type_get(session, DSET_CMD_ADD);
	else
		type = dset_data_get(data, DSET_OPT_TYPE);

	if (type == NULL)
		return MNL_CB_ERROR;

	for (i = DSET_ATTR_UNSPEC + 1; i <= DSET_ATTR_ADT_MAX; i++)
		if (nla[i])
		{
			found++;
			ATTR2DATA(session, nla, i, adt_attrs);
		}
	D("attr found %u", found);
	if (!found)
		return MNL_CB_OK;

	if (session->sort)
	{
		if (session->outbuflen <= session->pos + 1)
			realloc_outbuf(session);
		session->pos++; /* \0 */
		offset = session->pos;
	}

	switch (session->mode)
	{
	case DSET_LIST_SAVE:
		safe_snprintf(session, "add %s ", dset_data_setname(data));
		break;
	case DSET_LIST_XML:
		safe_snprintf(session, "<member><elem>");
		break;
	case DSET_LIST_PLAIN:
	default:
		break;
	}

	safe_dprintf(session, dset_print_elem, DSET_OPT_ELEM);
	if (session->mode == DSET_LIST_XML)
		safe_snprintf(session, "</elem>");

	for (i = 0; type->cmd[DSET_ADD].args[i] != DSET_ARG_NONE; i++)
	{
		arg = dset_keyword(type->cmd[DSET_ADD].args[i]);
		D("print arg opt %u (%s) %s", arg->opt, arg->name[0],
		  dset_data_test(data, arg->opt) ? "(yes)" : "(missing)");
		if (!(arg->print && dset_data_test(data, arg->opt)))
			continue;
		switch (session->mode)
		{
		case DSET_LIST_SAVE:
		case DSET_LIST_PLAIN:
			if (arg->has_arg == DSET_NO_ARG)
			{
				safe_snprintf(session, " %s", arg->name[0]);
				break;
			}
			safe_snprintf(session, " %s ", arg->name[0]);
			safe_dprintf(session, arg->print, arg->opt);
			break;
		case DSET_LIST_XML:
			if (arg->has_arg == DSET_NO_ARG)
			{
				safe_snprintf(session,
							  "<%s/>", arg->name[0]);
				break;
			}
			safe_snprintf(session, "<%s>", arg->name[0]);
			safe_dprintf(session, arg->print, arg->opt);
			safe_snprintf(session, "</%s>", arg->name[0]);
			break;
		default:
			break;
		}
	}

	if (session->mode == DSET_LIST_XML)
		safe_snprintf(session, "</member>\n");
	else
		safe_snprintf(session, "\n");

	if (session->sort)
	{
		struct dset_sorted *sorted;

		if (!list_empty(&session->pool))
		{
			sorted = list_first_entry(&session->pool,
									  struct dset_sorted, list);
			list_del(&sorted->list);
		}
		else
		{
			sorted = calloc(1, sizeof(struct dset_sorted));
			if (!sorted)
			{
				dset_err(session,
						 "Could not allocate memory to print sorted!");
				longjmp(printf_failure, 1);
			}
		}
		sorted->offset = offset;
		list_add_tail(&sorted->list, &session->sorted);
	}
	return MNL_CB_OK;
}

#define FAMILY_TO_STR(f) \
	((f) == NFPROTO_IPV4 ? "inet" : (f) == NFPROTO_IPV6 ? "inet6" : "any")

static int
list_create(struct dset_session *session, struct nlattr *nla[])
{
	const struct dset_data *data = session->data;
	const struct dset_type *type;
	const struct dset_arg *arg;
	// uint8_t family;
	int i;

	for (i = DSET_ATTR_UNSPEC + 1; i <= DSET_ATTR_CREATE_MAX; i++)
		if (nla[i])
		{
			D("add attr %u, opt %u", i, create_attrs[i].opt);
			ATTR2DATA(session, nla, i, create_attrs);
		}

	type = dset_type_check(session);
	if (type == NULL)
		return MNL_CB_ERROR;
	// family = dset_data_family(data);

	switch (session->mode)
	{
	case DSET_LIST_SAVE:
		safe_snprintf(session, "create %s %s",
					  dset_data_setname(data),
					  type->name);
		break;
	case DSET_LIST_PLAIN:
		safe_snprintf(session, "%sName: %s\n"
							   "Type: %s\nRevision: %u\nHeader:",
					  session->printed_set ? "\n" : "",
					  dset_data_setname(data),
					  type->name, type->revision);
		break;
	case DSET_LIST_XML:
		safe_snprintf(session,
					  "<dset name=\"%s\">\n"
					  "<type>%s</type>\n"
					  "<revision>%u</revision>\n"
					  "<header>",
					  dset_data_setname(data),
					  type->name, type->revision);
		break;
	default:
		break;
	}

	D("type %s, rev %u", type->name, type->revision);
	for (i = 0; type->cmd[DSET_CREATE].args[i] != DSET_ARG_NONE; i++)
	{
		arg = dset_keyword(type->cmd[DSET_CREATE].args[i]);
		D("create print arg opt %u (%s) %s", arg->opt,
		  arg->name[0] ? arg->name[0] : "",
		  dset_data_test(data, arg->opt) ? "(yes)" : "(missing)");
		if (!arg->print ||
			!dset_data_test(data, arg->opt))
			continue;
		switch (session->mode)
		{
		case DSET_LIST_SAVE:
		case DSET_LIST_PLAIN:
			if (arg->has_arg == DSET_NO_ARG)
			{
				safe_snprintf(session, " %s", arg->name[0]);
				break;
			}
			safe_snprintf(session, " %s ", arg->name[0]);
			safe_dprintf(session, arg->print, arg->opt);
			break;
		case DSET_LIST_XML:
			if (arg->has_arg == DSET_NO_ARG)
			{
				safe_snprintf(session,
							  "<%s/>", arg->name[0]);
				break;
			}
			safe_snprintf(session, "<%s>", arg->name[0]);
			safe_dprintf(session, arg->print, arg->opt);
			safe_snprintf(session, "</%s>", arg->name[0]);
			break;
		default:
			break;
		}
	}
	switch (session->mode)
	{
	case DSET_LIST_SAVE:
		safe_snprintf(session, "\n");
		break;
	case DSET_LIST_PLAIN:
		safe_snprintf(session, "\nSize in memory: ");
		safe_dprintf(session, dset_print_number, DSET_OPT_MEMSIZE);
		safe_snprintf(session, "\nReferences: ");
		safe_dprintf(session, dset_print_number, DSET_OPT_REFERENCES);
		if (dset_data_test(data, DSET_OPT_ELEMENTS))
		{
			safe_snprintf(session, "\nNumber of entries: ");
			safe_dprintf(session, dset_print_number, DSET_OPT_ELEMENTS);
		}
		safe_snprintf(session,
					  session->envopts & DSET_ENV_LIST_HEADER ? "\n" : "\nMembers:\n");
		break;
	case DSET_LIST_XML:
		safe_snprintf(session, "\n<memsize>");
		safe_dprintf(session, dset_print_number, DSET_OPT_MEMSIZE);
		safe_snprintf(session, "</memsize>\n<references>");
		safe_dprintf(session, dset_print_number, DSET_OPT_REFERENCES);
		safe_snprintf(session, "</references>\n");
		if (dset_data_test(data, DSET_OPT_ELEMENTS))
		{
			safe_snprintf(session, "<numentries>");
			safe_dprintf(session, dset_print_number, DSET_OPT_ELEMENTS);
			safe_snprintf(session, "</numentries>\n");
		}
		safe_snprintf(session,
					  session->envopts & DSET_ENV_LIST_HEADER ? "</header>\n" : "</header>\n<members>\n");
		break;
	default:
		break;
	}
	session->printed_set++;

	session->sort = strncmp(type->name, "hash:", 5) == 0 &&
					dset_envopt_test(session, DSET_ENV_SORTED);

	return MNL_CB_OK;
}

static int
bystrcmp(void *priv, struct list_head *a, struct list_head *b)
{
	struct dset_session *session = priv;
	struct dset_sorted *x = list_entry(a, struct dset_sorted, list);
	struct dset_sorted *y = list_entry(b, struct dset_sorted, list);

	return strcmp(session->outbuf + x->offset,
				  session->outbuf + y->offset);
}

static int
print_set_done(struct dset_session *session, bool callback_done)
{
	D("called for %s", session->saved_setname[0] == '\0'
						   ? "NONE"
						   : session->saved_setname);
	if (session->sort)
	{
		struct dset_sorted *pos;
		int ret;

		/* Print set header */
		ret = call_outfn(session);

		if (ret)
			return MNL_CB_ERROR;

		list_sort(session, &session->sorted, bystrcmp);

		list_for_each_entry(pos, &session->sorted, list)
		{
			ret = session->print_outfn(session, session->p,
									   "%s",
									   session->outbuf + pos->offset);
			if (ret < 0)
				return MNL_CB_ERROR;
		}
		list_splice(&session->sorted, &session->pool);
		INIT_LIST_HEAD(&session->sorted);
	}
	switch (session->mode)
	{
	case DSET_LIST_XML:
		if (session->envopts & DSET_ENV_LIST_SETNAME)
			break;
		if (session->envopts & DSET_ENV_LIST_HEADER)
		{
			if (session->saved_setname[0] != '\0')
				safe_snprintf(session, "</dset>\n");
			break;
		}
		if (session->saved_setname[0] != '\0')
			safe_snprintf(session, "</members>\n</dset>\n");
		break;
	default:
		break;
	}
	if (callback_done && session->mode == DSET_LIST_XML)
		safe_snprintf(session, "</dsets>\n");
	return call_outfn(session) ? MNL_CB_ERROR : MNL_CB_STOP;
}

static int
callback_list(struct dset_session *session, struct nlattr *nla[],
			  enum dset_cmd cmd)
{
	struct dset_data *data = session->data;

	if (setjmp(printf_failure))
	{
		session->saved_setname[0] = '\0';
		session->printed_set = 0;
		return MNL_CB_ERROR;
	}

	if (!nla[DSET_ATTR_SETNAME])
		FAILURE("Broken %s kernel message: missing setname!",
				cmd2name[cmd]);

	ATTR2DATA(session, nla, DSET_ATTR_SETNAME, cmd_attrs);
	D("setname %s", dset_data_setname(data));
	if (session->envopts & DSET_ENV_LIST_SETNAME &&
		session->mode != DSET_LIST_SAVE)
	{
		if (session->mode == DSET_LIST_XML)
			safe_snprintf(session, "<dset name=\"%s\"/>\n",
						  dset_data_setname(data));
		else
			safe_snprintf(session, "%s\n",
						  dset_data_setname(data));
		return call_outfn(session) ? MNL_CB_ERROR : MNL_CB_OK;
	}

	if (STREQ(dset_data_setname(data), session->saved_setname))
	{
		/* Header part already seen */
		if (dset_data_test(data, DSET_OPT_TYPE) &&
			nla[DSET_ATTR_DATA] != NULL)
			FAILURE("Broken %s kernel message: "
					"extra DATA received!",
					cmd2name[cmd]);
	}
	else
	{
		if (nla[DSET_ATTR_DATA] == NULL)
			FAILURE("Broken %s kernel message: "
					"missing DATA part!",
					cmd2name[cmd]);

		/* Close previous set printing */
		if (session->saved_setname[0] != '\0')
			print_set_done(session, false);
	}

	if (nla[DSET_ATTR_DATA] != NULL)
	{
		struct nlattr *cattr[DSET_ATTR_CREATE_MAX + 1] = {};

		if (!(nla[DSET_ATTR_TYPENAME] &&
			  nla[DSET_ATTR_FAMILY] &&
			  nla[DSET_ATTR_REVISION]))
			FAILURE("Broken %s kernel message: missing %s!",
					cmd2name[cmd],
					!nla[DSET_ATTR_TYPENAME] ? "typename" : !nla[DSET_ATTR_FAMILY] ? "family" : "revision");

		/* Reset CREATE specific flags */
		dset_data_flags_unset(data, DSET_CREATE_FLAGS);
		D("nla typename %s",
		  (char *)mnl_attr_get_payload(nla[DSET_ATTR_TYPENAME]));

		ATTR2DATA(session, nla, DSET_ATTR_FAMILY, cmd_attrs);
		ATTR2DATA(session, nla, DSET_ATTR_TYPENAME, cmd_attrs);
		ATTR2DATA(session, nla, DSET_ATTR_REVISION, cmd_attrs);
		// D("head: family %u, typename %s",
		//   dset_data_family(data),
		//   (const char *) dset_data_get(data, DSET_OPT_TYPENAME));
		if (mnl_attr_parse_nested(nla[DSET_ATTR_DATA],
								  create_attr_cb, cattr) < 0)
			FAILURE("Broken %s kernel message: "
					"cannot validate DATA attributes!",
					cmd2name[cmd]);
		if (list_create(session, cattr) != MNL_CB_OK)
			return MNL_CB_ERROR;
		strcpy(session->saved_setname, dset_data_setname(data));
	}

	if (nla[DSET_ATTR_ADT] != NULL)
	{
		struct nlattr *tb, *adt[DSET_ATTR_ADT_MAX + 1];

		mnl_attr_for_each_nested(tb, nla[DSET_ATTR_ADT])
		{
			D("ADT attributes for %s", dset_data_setname(data));
			memset(adt, 0, sizeof(adt));
			/* Reset ADT specific flags */
			dset_data_flags_unset(data, DSET_ADT_FLAGS);
			if (mnl_attr_parse_nested(tb, adt_attr_cb, adt) < 0)
				FAILURE("Broken %s kernel message: "
						"cannot validate ADT attributes!",
						cmd2name[cmd]);
			if (list_adt(session, adt) != MNL_CB_OK)
				return MNL_CB_ERROR;
		}
		if (session->sort)
			return MNL_CB_OK;
	}
	return call_outfn(session) ? MNL_CB_ERROR : MNL_CB_OK;
}

#ifndef DSET_PROTOCOL_MIN
#define DSET_PROTOCOL_MIN DSET_PROTOCOL
#endif

#ifndef DSET_PROTOCOL_MAX
#define DSET_PROTOCOL_MAX DSET_PROTOCOL
#endif

static int
callback_version(struct dset_session *session, struct nlattr *nla[])
{
	uint8_t min, max;

	min = max = mnl_attr_get_u8(nla[DSET_ATTR_PROTOCOL]);

	if (nla[DSET_ATTR_PROTOCOL_MIN])
	{
		min = mnl_attr_get_u8(nla[DSET_ATTR_PROTOCOL_MIN]);
		D("min: %u", min);
	}

	if (min > DSET_PROTOCOL_MAX || max < DSET_PROTOCOL_MIN)
		FAILURE("Cannot communicate with kernel: "
				"Kernel support protocol versions %u-%u "
				"while userspace supports protocol versions %u-%u",
				min, max, DSET_PROTOCOL_MIN, DSET_PROTOCOL_MAX);

	if (!(session->envopts & DSET_ENV_QUIET) &&
		max != DSET_PROTOCOL_MAX)
		dset_warn(session,
				  "Kernel support protocol versions %u-%u "
				  "while userspace supports protocol versions %u-%u",
				  min, max, DSET_PROTOCOL_MIN, DSET_PROTOCOL_MAX);

	session->protocol = MIN(max, DSET_PROTOCOL_MAX);
	session->version_checked = true;

	return MNL_CB_STOP;
}

static int
callback_header(struct dset_session *session, struct nlattr *nla[])
{
	const char *setname;
	const struct dset_data *data = session->data;

	if (!nla[DSET_ATTR_SETNAME])
		FAILURE("Broken HEADER kernel message: missing setname!");

	setname = mnl_attr_get_str(nla[DSET_ATTR_SETNAME]);
	if (!STREQ(setname, dset_data_setname(data)))
		FAILURE("Broken HEADER kernel message: sent setname `%s' "
				"does not match with received one `%s'!",
				dset_data_setname(data), setname);

	if (!(nla[DSET_ATTR_TYPENAME] &&
		  nla[DSET_ATTR_REVISION] &&
		  nla[DSET_ATTR_FAMILY]))
		FAILURE("Broken HEADER kernel message: "
				"missing attribute '%s'!",
				!nla[DSET_ATTR_TYPENAME] ? "typename" : !nla[DSET_ATTR_REVISION] ? "revision" : "family");

	ATTR2DATA(session, nla, DSET_ATTR_TYPENAME, cmd_attrs);
	ATTR2DATA(session, nla, DSET_ATTR_REVISION, cmd_attrs);
	ATTR2DATA(session, nla, DSET_ATTR_FAMILY, cmd_attrs);
	// D("got family: %u", dset_data_family(session->data));

	return MNL_CB_STOP;
}

static int
callback_type(struct dset_session *session, struct nlattr *nla[])
{
	const struct dset_data *data = session->data;
	const char *typename, *orig;

	if (!(nla[DSET_ATTR_TYPENAME] &&
		  nla[DSET_ATTR_REVISION] &&
		  nla[DSET_ATTR_FAMILY]))
		FAILURE("Broken TYPE kernel message: "
				"missing attribute '%s'!",
				!nla[DSET_ATTR_TYPENAME] ? "typename" : !nla[DSET_ATTR_REVISION] ? "revision" : "family");

	typename = mnl_attr_get_str(nla[DSET_ATTR_TYPENAME]);
	orig = dset_data_get(data, DSET_OPT_TYPENAME);
	if (!STREQ(typename, orig))
		FAILURE("Broken TYPE kernel message: sent typename `%s' "
				"does not match with received one `%s'!",
				orig, typename);

	ATTR2DATA(session, nla, DSET_ATTR_TYPENAME, cmd_attrs);
	ATTR2DATA(session, nla, DSET_ATTR_REVISION, cmd_attrs);
	ATTR2DATA(session, nla, DSET_ATTR_FAMILY, cmd_attrs);
	if (nla[DSET_ATTR_REVISION_MIN])
		ATTR2DATA(session, nla, DSET_ATTR_REVISION_MIN, cmd_attrs);

	return MNL_CB_STOP;
}

static int
cmd_attr_cb(const struct nlattr *attr, void *data)
{
	return generic_data_attr_cb(attr, data, DSET_ATTR_CMD_MAX, cmd_attrs);
}

#if 0
static int
mnl_attr_parse_dbg(const struct nlmsghdr *nlh, int offset,
		   mnl_attr_cb_t cb, void *data)
{
	int ret = MNL_CB_OK;
	struct nlattr *attr = mnl_nlmsg_get_payload_offset(nlh, offset);
	int len = nlh->nlmsg_len - MNL_NLMSG_HDRLEN - MNL_ALIGN(offset);

	while (mnl_attr_ok(attr, len)) {
		D("attr: type %u, attrlen %u, len %u",
		  mnl_attr_get_type(attr), attr->nla_len, len);
		if (cb && (ret = cb(attr, data)) <= MNL_CB_STOP)
			return ret;
		attr = mnl_attr_next(attr, &len);
	}
	return ret;
}
#endif

static int
callback_data(const struct nlmsghdr *nlh, void *data)
{
	struct dset_session *session = data;
	struct nlattr *nla[DSET_ATTR_CMD_MAX + 1] = {};
	uint8_t proto, cmd;
	int ret = MNL_CB_OK, nfmsglen = MNL_ALIGN(sizeof(struct nfgenmsg));

	D("called, nlmsg_len %u", nlh->nlmsg_len);
	cmd = dset_get_nlmsg_type(nlh);
	if (cmd == DSET_CMD_LIST && session->cmd == DSET_CMD_SAVE)
		/* Kernel always send DSET_CMD_LIST */
		cmd = DSET_CMD_SAVE;

	if (cmd != session->cmd)
		FAILURE("Protocol error, we sent command %s "
				"and received %s[%u]",
				cmd2name[session->cmd],
				cmd < DSET_MSG_MAX ? cmd2name[cmd] : "unknown", cmd);

	if (mnl_attr_parse(nlh, nfmsglen, cmd_attr_cb, nla) < MNL_CB_STOP)
		FAILURE("Broken %s kernel message: "
				"cannot validate and parse attributes",
				cmd2name[cmd]);

	if (!nla[DSET_ATTR_PROTOCOL])
		FAILURE("Sad, sad day: kernel message %s "
				"does not carry the protocol version.",
				cmd2name[cmd]);

	proto = mnl_attr_get_u8(nla[DSET_ATTR_PROTOCOL]);

	/* Check protocol */
	if (cmd != DSET_CMD_PROTOCOL && proto != session->protocol)
		FAILURE("Giving up: kernel protocol version %u "
				"does not match our protocol version %u",
				proto, session->protocol);

	D("Message: %s", cmd2name[cmd]);
	switch (cmd)
	{
	case DSET_CMD_LIST:
	case DSET_CMD_SAVE:
		ret = callback_list(session, nla, cmd);
		D("flag multi: %u", nlh->nlmsg_flags & NLM_F_MULTI);
		if (ret >= MNL_CB_STOP && !(nlh->nlmsg_flags & NLM_F_MULTI))
			ret = print_set_done(session, false);
		break;
	case DSET_CMD_PROTOCOL:
		if (!session->version_checked)
			ret = callback_version(session, nla);
		break;
	case DSET_CMD_HEADER:
		ret = callback_header(session, nla);
		break;
	case DSET_CMD_TYPE:
		ret = callback_type(session, nla);
		break;
	default:
		FAILURE("Data message received when not expected at %s",
				cmd2name[session->cmd]);
	}
	D("return code: %s", ret == MNL_CB_STOP ? "stop" : ret == MNL_CB_OK ? "ok" : "error");
	return ret;
}

static int
callback_done(const struct nlmsghdr *nlh UNUSED, void *data)
{
	struct dset_session *session = data;

	D(" called");
	if (session->cmd == DSET_CMD_LIST || session->cmd == DSET_CMD_SAVE)
		return print_set_done(session, true);

	FAILURE("Invalid message received in non LIST or SAVE state.");
}

static int
decode_errmsg(struct dset_session *session, const struct nlmsghdr *nlh)
{
	const struct nlmsgerr *err = mnl_nlmsg_get_payload(nlh);
	const struct nlmsghdr *msg = &err->msg;
	struct nlattr *nla[DSET_ATTR_CMD_MAX + 1] = {};
	enum dset_cmd cmd;
	int nfmsglen = MNL_ALIGN(sizeof(struct nfgenmsg));

	if (nlh->nlmsg_len < (uint32_t)MNL_ALIGN(sizeof(struct nlmsgerr)) ||
		nlh->nlmsg_len < MNL_ALIGN(sizeof(struct nlmsgerr)) + msg->nlmsg_len)
		FAILURE("Broken error report message received.");

	cmd = dset_get_nlmsg_type(msg);
	D("nlsmg_len: %u", msg->nlmsg_len);
	if (cmd != session->cmd)
		FAILURE("Protocol error, we sent command %s "
				"and received error report for %s[%u]",
				cmd2name[session->cmd],
				cmd < DSET_MSG_MAX ? cmd2name[cmd] : "unknown", cmd);

	if (mnl_attr_parse(msg, nfmsglen, cmd_attr_cb, nla) < MNL_CB_STOP)
		FAILURE("Broken %s error report message: "
				"cannot validate attributes",
				cmd2name[cmd]);

	if (!nla[DSET_ATTR_PROTOCOL])
		FAILURE("Broken %s error report message: "
				"missing protocol attribute",
				cmd2name[cmd]);

	if (nla[DSET_ATTR_LINENO])
	{
		session->lineno = mnl_attr_get_u32(nla[DSET_ATTR_LINENO]);
		if (nla[DSET_ATTR_LINENO]->nla_type & NLA_F_NET_BYTEORDER)
			session->lineno = ntohl(session->lineno);
	}

	return dset_errcode(session, cmd, -err->error);
}

static int
callback_error(const struct nlmsghdr *nlh, void *cbdata)
{
	struct dset_session *session = cbdata;
	struct dset_data *data = session->data;
	const struct nlmsgerr *err = mnl_nlmsg_get_payload(nlh);
	int ret = MNL_CB_ERROR;

	D(" called, cmd %s", cmd2name[session->cmd]);
	if (nlh->nlmsg_len < mnl_nlmsg_size(sizeof(struct nlmsgerr)))
		FAILURE("Broken error message received.");

	if (err->error == 0)
	{
		/* ACK */
		ret = MNL_CB_STOP;

		switch (session->cmd)
		{
		case DSET_CMD_CREATE:
			/* Add successfully created set to the cache */
			dset_cache_add(dset_data_setname(data),
						   dset_data_get(data, DSET_OPT_TYPE));
			break;
		case DSET_CMD_DESTROY:
			/* Delete destroyed sets from the cache */
			dset_cache_del(dset_data_setname(data));
			/* Fall through */
		case DSET_CMD_FLUSH:
			break;
		case DSET_CMD_RENAME:
			dset_cache_rename(dset_data_setname(data),
							  dset_data_get(data,
											DSET_OPT_SETNAME2));
			break;
		case DSET_CMD_SWAP:
			dset_cache_swap(dset_data_setname(data),
							dset_data_get(data,
										  DSET_OPT_SETNAME2));
			break;
		case DSET_CMD_TEST:
			if (!(session->envopts & DSET_ENV_QUIET))
			{
				dset_print_elem(session->report,
								DSET_ERRORBUFLEN,
								session->data,
								DSET_OPT_NONE, 0);
				dset_warn(session, " is in set %s.",
						  dset_data_setname(data));
			}
			/* Fall through */
		case DSET_CMD_ADD:
		case DSET_CMD_DEL:
			break;
		case DSET_CMD_LIST:
		case DSET_CMD_SAVE:
			/* No set in kernel */
			print_set_done(session, true);
			break;
		default:
			FAILURE("ACK message received to command %s[%u], "
					"which is not expected",
					session->cmd < DSET_MSG_MAX
						? cmd2name[session->cmd]
						: "unknown",
					session->cmd);
		}
		return ret;
	}
	D("nlmsgerr error: %u", -err->error);

	/* Error messages */

	/* Special case for DSET_CMD_TEST */
	if (session->cmd == DSET_CMD_TEST &&
		err->error == -DSET_ERR_EXIST)
	{
		if (!(session->envopts & DSET_ENV_QUIET))
		{
			dset_print_elem(session->report, DSET_ERRORBUFLEN,
							session->data, DSET_OPT_NONE, 0);
			dset_notice(session, " is NOT in set %s.",
						dset_data_setname(data));
		}
		return ret;
	}

	decode_errmsg(session, nlh);

	return ret;
}

static int
callback_noop(const struct nlmsghdr *nlh UNUSED, void *data UNUSED)
{
	return MNL_CB_OK;
}
/*
 * Build and send messages
 */

static inline int
open_nested(struct dset_session *session, struct nlmsghdr *nlh, int attr)
{
	if (nlh->nlmsg_len + MNL_ATTR_HDRLEN > session->bufsize)
		return 1;
	session->nested[session->nestid++] = mnl_attr_nest_start(nlh, attr);
	return 0;
}

static inline void
close_nested(struct dset_session *session, struct nlmsghdr *nlh)
{
	mnl_attr_nest_end(nlh, session->nested[session->nestid - 1]);
	session->nested[--session->nestid] = NULL;
}

static size_t
attr_len(const struct dset_attr_policy *attr, uint8_t family, uint16_t *flags)
{
	switch (attr->type)
	{
	case MNL_TYPE_NESTED:
		if (attr->len)
			return attr->len;

		*flags = NLA_F_NET_BYTEORDER;
		return family == NFPROTO_IPV4 ? sizeof(uint32_t)
									  : sizeof(struct in6_addr);
	case MNL_TYPE_U64:
		*flags = NLA_F_NET_BYTEORDER;
		return sizeof(uint64_t);
	case MNL_TYPE_U32:
		*flags = NLA_F_NET_BYTEORDER;
		return sizeof(uint32_t);
	case MNL_TYPE_U16:
		*flags = NLA_F_NET_BYTEORDER;
		return sizeof(uint16_t);
	case MNL_TYPE_U8:
		return sizeof(uint8_t);
	default:
		return attr->len;
	}
}

#define BUFFER_FULL(bufsize, nlmsg_len, nestlen, attrlen)      \
	(nlmsg_len + nestlen + MNL_ATTR_HDRLEN + MNL_ALIGN(alen) + \
		 MNL_ALIGN(sizeof(struct nlmsgerr)) >                  \
	 bufsize)

static int
rawdata2attr(struct dset_session *session, struct nlmsghdr *nlh,
			 const void *d, int type, uint8_t family,
			 const struct dset_attr_policy attrs[])
{
	const struct dset_attr_policy *attr;
	int alen;
	uint16_t flags = 0;

	attr = &attrs[type];
	alen = attr_len(attr, family, &flags);
	if (BUFFER_FULL(session->bufsize, nlh->nlmsg_len, 0, alen))
		return 1;

	switch (attr->type)
	{
	case MNL_TYPE_U64:
	{
		uint64_t value = htobe64(*(const uint64_t *)d);

		mnl_attr_put(nlh, type | flags, alen, &value);
		return 0;
	}
	case MNL_TYPE_U32:
	{
		uint32_t value = htonl(*(const uint32_t *)d);

		mnl_attr_put(nlh, type | flags, alen, &value);
		return 0;
	}
	case MNL_TYPE_U16:
	{
		uint16_t value = htons(*(const uint16_t *)d);

		mnl_attr_put(nlh, type | flags, alen, &value);
		return 0;
	}
	case MNL_TYPE_NUL_STRING:
		alen = strlen((const char *)d) + 1;
		break;
	default:
		break;
	}

	mnl_attr_put(nlh, type | flags, alen, d);

	return 0;
}

static int
data2attr(struct dset_session *session, struct nlmsghdr *nlh,
		  struct dset_data *data, int type, uint8_t family,
		  const struct dset_attr_policy attrs[])
{
	const struct dset_attr_policy *attr = &attrs[type];

	return rawdata2attr(session, nlh, dset_data_get(data, attr->opt),
						type, family, attrs);
}

#define ADDATTR_PROTOCOL(nlh, protocol) \
	mnl_attr_put_u8(nlh, DSET_ATTR_PROTOCOL, protocol)

#define ADDATTR(session, nlh, data, type, family, attrs) \
	data2attr(session, nlh, data, type, family, attrs)

#define ADDATTR_SETNAME(session, nlh, data)                          \
	data2attr(session, nlh, data, DSET_ATTR_SETNAME, NFPROTO_UNSPEC, \
			  cmd_attrs)

#define ADDATTR_IF(session, nlh, data, type, family, attrs) \
	dset_data_test(data, attrs[type].opt) ? data2attr(session, nlh, data, type, family, attrs) : 0

#define ADDATTR_RAW(session, nlh, data, type, attrs) \
	rawdata2attr(session, nlh, data, type, NFPROTO_UNSPEC, attrs)

static void
addattr_create(struct dset_session *session,
			   struct nlmsghdr *nlh, struct dset_data *data, uint8_t family)
{
	int i;

	for (i = DSET_ATTR_UNSPEC + 1; i <= DSET_ATTR_CREATE_MAX; i++)
		ADDATTR_IF(session, nlh, data, i, family, create_attrs);
}

static int
addattr_adt(struct dset_session *session,
			struct nlmsghdr *nlh, struct dset_data *data, uint8_t family)
{
	int i;

	for (i = DSET_ATTR_UNSPEC + 1; i <= DSET_ATTR_ADT_MAX; i++)
		if (ADDATTR_IF(session, nlh, data, i, family, adt_attrs))
			return 1;

	return 0;
}

#define PRIVATE_MSG_BUFLEN 256

static int
build_send_private_msg(struct dset_session *session, enum dset_cmd cmd)
{
	char buffer[PRIVATE_MSG_BUFLEN] __attribute__((aligned)) = {};
	struct nlmsghdr *nlh = (void *)buffer;
	struct dset_data *data = session->data;
	int len = PRIVATE_MSG_BUFLEN, ret;
	enum dset_cmd saved = session->cmd;

	/* Initialize header */
	session->transport->fill_hdr(session->handle, cmd, buffer, len, 0);

	ADDATTR_PROTOCOL(nlh,
					 cmd == DSET_CMD_PROTOCOL ? DSET_PROTOCOL : session->protocol);

	switch (cmd)
	{
	case DSET_CMD_PROTOCOL:
		break;
	case DSET_CMD_HEADER:
		if (!dset_data_test(data, DSET_SETNAME))
			return dset_err(session,
							"Invalid internal HEADER command: "
							"missing setname");
		ADDATTR_SETNAME(session, nlh, data);
		break;
	case DSET_CMD_TYPE:
		if (!dset_data_test(data, DSET_OPT_TYPENAME))
			return dset_err(session,
							"Invalid internal TYPE command: "
							"missing settype");
		ADDATTR(session, nlh, data, DSET_ATTR_TYPENAME,
				NFPROTO_UNSPEC, cmd_attrs);
		mnl_attr_put_u8(nlh, DSET_ATTR_FAMILY, NFPROTO_UNSPEC);
		break;
	default:
		return dset_err(session, "Internal error: "
								 "unknown private command %u",
						cmd);
	}

	/* Backup, then restore real command */
	session->cmd = cmd;
	ret = session->transport->query(session->handle, buffer, len);
	session->cmd = saved;

	return ret;
}

static inline bool
may_aggregate_ad(struct dset_session *session, enum dset_cmd cmd)
{
	return session->lineno != 0 &&
		   (cmd == DSET_CMD_ADD || cmd == DSET_CMD_DEL) &&
		   cmd == session->cmd &&
		   STREQ(dset_data_setname(session->data), session->saved_setname);
}

static int
build_msg(struct dset_session *session, bool aggregate)
{
	struct nlmsghdr *nlh = session->buffer;
	struct dset_data *data = session->data;

	/* Public commands */
	D("cmd %s, nlmsg_len: %u", cmd2name[session->cmd], nlh->nlmsg_len);
	if (nlh->nlmsg_len == 0)
	{
		/* Initialize header */
		aggregate = false;
		session->transport->fill_hdr(session->handle,
									 session->cmd,
									 session->buffer,
									 session->bufsize,
									 session->envopts);
		ADDATTR_PROTOCOL(nlh, session->protocol);
	}
	D("Protocol added, aggregate %s", aggregate ? "yes" : "no");
	switch (session->cmd)
	{
	case DSET_CMD_CREATE:
	{
		const struct dset_type *type;

		/* Sanity checkings */
		if (!dset_data_test(data, DSET_SETNAME))
			return dset_err(session,
							"Invalid create command: missing setname");
		if (!dset_data_test(data, DSET_OPT_TYPE))
			return dset_err(session,
							"Invalid create command: missing settype");

		type = dset_data_get(data, DSET_OPT_TYPE);
		/* Core attributes:
		 * setname, typename, revision, family, flags (optional) */
		ADDATTR_SETNAME(session, nlh, data);
		ADDATTR(session, nlh, data, DSET_ATTR_TYPENAME,
				NFPROTO_UNSPEC, cmd_attrs);
		ADDATTR_RAW(session, nlh, &type->revision,
					DSET_ATTR_REVISION, cmd_attrs);

		mnl_attr_put_u8(nlh, DSET_ATTR_FAMILY, NFPROTO_UNSPEC);

		/* Type-specific create attributes */
		D("call open_nested");
		open_nested(session, nlh, DSET_ATTR_DATA);
		addattr_create(session, nlh, data, type->family);
		D("call close_nested");
		close_nested(session, nlh);
		break;
	}
	case DSET_CMD_DESTROY:
	case DSET_CMD_FLUSH:
	case DSET_CMD_SAVE:
		if (dset_data_test(data, DSET_SETNAME))
			ADDATTR_SETNAME(session, nlh, data);
		break;
	case DSET_CMD_LIST:
	{
		uint32_t flags = 0;

		if (session->envopts & DSET_ENV_LIST_SETNAME)
			flags |= DSET_FLAG_LIST_SETNAME;
		if (session->envopts & DSET_ENV_LIST_HEADER)
			flags |= DSET_FLAG_LIST_HEADER;
		if (dset_data_test(data, DSET_SETNAME))
			ADDATTR_SETNAME(session, nlh, data);
		if (flags && session->mode != DSET_LIST_SAVE)
		{
			dset_data_set(data, DSET_OPT_FLAGS, &flags);
			ADDATTR(session, nlh, data, DSET_ATTR_FLAGS,
					NFPROTO_UNSPEC, cmd_attrs);
		}
		break;
	}
	case DSET_CMD_RENAME:
	case DSET_CMD_SWAP:
		if (!dset_data_test(data, DSET_SETNAME))
			return dset_err(session,
							"Invalid %s command: missing from-setname",
							session->cmd == DSET_CMD_SWAP ? "swap" : "rename");
		if (!dset_data_test(data, DSET_OPT_SETNAME2))
			return dset_err(session,
							"Invalid %s command: missing to-setname",
							session->cmd == DSET_CMD_SWAP ? "swap" : "rename");
		ADDATTR_SETNAME(session, nlh, data);
		ADDATTR_RAW(session, nlh,
					dset_data_get(data, DSET_OPT_SETNAME2),
					DSET_ATTR_SETNAME2, cmd_attrs);
		break;
	case DSET_CMD_ADD:
	case DSET_CMD_DEL:
	{
		const struct dset_type *type;

		if (!aggregate)
		{
			/* Setname, type not checked/added yet */
			if (!dset_data_test(data, DSET_SETNAME))
				return dset_err(session,
								"Invalid %s command: missing setname",
								session->cmd == DSET_CMD_ADD ? "add" : "del");

			if (!dset_data_test(data, DSET_OPT_TYPE))
				return dset_err(session,
								"Invalid %s command: missing settype",
								session->cmd == DSET_CMD_ADD ? "add" : "del");

			/* Core options: setname */
			ADDATTR_SETNAME(session, nlh, data);
			if (session->lineno != 0)
			{
				/* Restore mode */
				ADDATTR_RAW(session, nlh, &session->lineno,
							DSET_ATTR_LINENO, cmd_attrs);
				open_nested(session, nlh, DSET_ATTR_ADT);
			}
		}
		type = dset_data_get(data, DSET_OPT_TYPE);
		// D("family: %u, type family %u",
		//   dset_data_family(data), type->family);
		if (open_nested(session, nlh, DSET_ATTR_DATA))
		{
			D("open_nested failed");
			return 1;
		}
		if (addattr_adt(session, nlh, data, NFPROTO_UNSPEC) ||
			ADDATTR_RAW(session, nlh, &session->lineno,
						DSET_ATTR_LINENO, cmd_attrs))
		{
			/* Cancel last, unfinished nested attribute */
			mnl_attr_nest_cancel(nlh,
								 session->nested[session->nestid - 1]);
			session->nested[--session->nestid] = NULL;
			return 1;
		}
		close_nested(session, nlh);
		break;
	}
	case DSET_CMD_TEST:
	{
		const struct dset_type *type;
		/* Return codes are not aggregated, so tests cannot be either */

		/* Setname, type not checked/added yet */

		if (!dset_data_test(data, DSET_SETNAME))
			return dset_err(session,
							"Invalid test command: missing setname");

		if (!dset_data_test(data, DSET_OPT_TYPE))
			return dset_err(session,
							"Invalid test command: missing settype");

		type = dset_data_get(data, DSET_OPT_TYPE);
		// D("family: %u, type family %u",
		//   dset_data_family(data), type->family);
		ADDATTR_SETNAME(session, nlh, data);
		open_nested(session, nlh, DSET_ATTR_DATA);
		addattr_adt(session, nlh, data, NFPROTO_UNSPEC);
		close_nested(session, nlh);
		break;
	}
	default:
		return dset_err(session, "Internal error: unknown command %u",
						session->cmd);
	}
	return 0;
}

/**
 * dset_commit - commit buffered commands
 * @session: session structure
 *
 * Commit buffered commands, if there are any.
 *
 * Returns 0 on success or a negative error code.
 */
int dset_commit(struct dset_session *session)
{
	struct nlmsghdr *nlh;
	int ret = 0, i;

	assert(session);

	nlh = session->buffer;
	D("send buffer: len %u, cmd %s",
	  nlh->nlmsg_len, cmd2name[session->cmd]);
	if (nlh->nlmsg_len == 0)
		/* Nothing to do */
		return 0;

	/* Close nested data blocks */
	for (i = session->nestid - 1; i >= 0; i--)
		close_nested(session, nlh);

	/* Send buffer */
	ret = session->transport->query(session->handle,
									session->buffer,
									session->bufsize);

	/* Reset saved data and nested state */
	session->saved_setname[0] = '\0';
	session->printed_set = 0;
	for (i = session->nestid - 1; i >= 0; i--)
		session->nested[i] = NULL;
	session->nestid = 0;
	nlh->nlmsg_len = 0;

	D("ret: %d", ret);

	if (ret < 0)
	{
		if (session->report[0] != '\0')
			return -1;
		else
			return dset_err(session,
							"Internal protocol error");
	}
	return 0;
}

static mnl_cb_t cb_ctl[] = {
	[NLMSG_NOOP] = callback_noop,
	[NLMSG_ERROR] = callback_error,
	[NLMSG_DONE] = callback_done,
	[NLMSG_OVERRUN] = callback_noop,
	[NLMSG_MIN_TYPE] = callback_data,
};

static inline struct dset_handle *
init_transport(struct dset_session *session)
{
	session->handle = session->transport->init(cb_ctl, session);

	return session->handle;
}

/**
 * dset_cmd - execute a command
 * @session: session structure
 * @cmd: command to execute
 * @lineno: command line number in restore mode
 *
 * Execute - or prepare/buffer in restore mode - a command.
 * It is the caller responsibility that the data field be filled out
 * with all required parameters for a successful execution.
 * The data field is cleared after this function call for the public
 * commands.
 *
 * Returns 0 on success or a negative error code.
 */
int dset_cmd(struct dset_session *session, enum dset_cmd cmd, uint32_t lineno)
{
	struct dset_data *data;
	bool aggregate = false;
	int ret = -1;

	assert(session);

	if (cmd < DSET_CMD_NONE || cmd >= DSET_MSG_MAX)
		return 0;

	/* Initialize transport method if not done yet */
	if (session->handle == NULL && init_transport(session) == NULL)
		return dset_err(session,
						"Cannot open session to kernel.");

	data = session->data;

	/* Check protocol version once */
	if (!session->version_checked)
	{
		if (build_send_private_msg(session, DSET_CMD_PROTOCOL) < 0)
			return -1;

		if (dset_session_report_type(session) == DSET_WARNING &&
			cmd != DSET_CMD_NONE)
			/* Suppress protocol warning */
			dset_session_report_reset(session);
	}
	/* DSET_CMD_NONE: check protocol version only */
	if (cmd == DSET_CMD_NONE)
		return 0;

	/* Private commands */
	if (cmd == DSET_CMD_TYPE || cmd == DSET_CMD_HEADER)
		return build_send_private_msg(session, cmd);

	/* Check aggregatable commands */
	aggregate = may_aggregate_ad(session, cmd);

	if (!aggregate)
	{
		/* Flush possible aggregated commands */
		ret = dset_commit(session);
		if (ret < 0)
			return ret;
	}

	/* Real command: update lineno too */
	session->cmd = cmd;
	session->lineno = lineno;

	/* Set default output mode */
	if (cmd == DSET_CMD_LIST)
	{
		if (session->mode == DSET_LIST_NONE)
			session->mode = DSET_LIST_PLAIN;
	}
	else if (cmd == DSET_CMD_SAVE)
	{
		if (session->mode == DSET_LIST_NONE)
			session->mode = DSET_LIST_SAVE;
	}
	/* Start the root element in XML mode */
	if ((cmd == DSET_CMD_LIST || cmd == DSET_CMD_SAVE) &&
		session->mode == DSET_LIST_XML)
		safe_snprintf(session, "<dsets>\n");

	D("next: build_msg");
	/* Build new message or append buffered commands */
	ret = build_msg(session, aggregate);

	D("build_msg returned %u", ret);
	if (ret > 0)
	{
		/* Buffer is full, send buffered commands */
		ret = dset_commit(session);
		if (ret < 0)
			goto cleanup;
		ret = build_msg(session, false);
		D("build_msg 2 returned %u", ret);
	}
	if (ret < 0)
		goto cleanup;
	D("past: build_msg");

	/* We have to save the type for error handling */
	session->saved_type = dset_data_get(data, DSET_OPT_TYPE);
	if (session->lineno != 0 &&
		(cmd == DSET_CMD_ADD || cmd == DSET_CMD_DEL))
	{
		/* Save setname for the next possible aggregated restore line */
		strcpy(session->saved_setname, dset_data_setname(data));
		dset_data_reset(data);
		/* Don't commit: we may aggregate next command */
		ret = 0;
		goto cleanup;
	}

	D("call commit");
	ret = dset_commit(session);

cleanup:
	D("reset data");
	dset_data_reset(data);
	return ret;
}

static int __attribute__((format(printf, 3, 4)))
default_print_outfn(struct dset_session *session, void *p UNUSED,
					const char *fmt, ...)
{
	int len;
	va_list args;

	va_start(args, fmt);
	len = vfprintf(session->ostream, fmt, args);
	va_end(args);

	return len;
}

/**
 * dset_session_print_outfn - set session output printing function
 * @session: session structure
 * @outfn: output printing function
 * @p: pointer to private area
 *
 * Set the session output printing function. If the @outfn is NULL,
 * then the default output function is configured. You can set
 * the @p pointer to a private area: the output printing function
 * is called with @p in one of its arguments.
 *
 * Returns 0 on success or a negative error code.
 */
int dset_session_print_outfn(struct dset_session *session,
							 dset_print_outfn outfn,
							 void *p)
{
	session->print_outfn = outfn ? outfn : default_print_outfn;
	session->p = p;
	return 0;
}

/**
 * dset_session_init - initialize an dset session
 * @outfn: output printing function
 * @p: pointer to private area
 *
 * Initialize an dset session by allocating a session structure
 * and filling out with the initialization data. The function
 * calls dset_session_print_outfn() to set @print_outfn, @p.
 *
 * Returns the created session sctructure on success or NULL.
 */
struct dset_session *
dset_session_init(dset_print_outfn print_outfn, void *p)
{
	struct dset_session *session;
	size_t bufsize = getpagesize();

	/* Create session object */
	session = calloc(1, sizeof(struct dset_session) + bufsize);
	if (session == NULL)
		return NULL;
	session->outbuf = calloc(1, DSET_OUTBUFLEN);
	if (session->outbuf == NULL)
		goto free_session;
	session->outbuflen = DSET_OUTBUFLEN;
	session->bufsize = bufsize;
	session->buffer = session + 1;
	session->istream = stdin;
	session->ostream = stdout;
	session->protocol = DSET_PROTOCOL;
	INIT_LIST_HEAD(&session->sorted);
	INIT_LIST_HEAD(&session->pool);

	/* The single transport method yet */
	session->transport = &dset_mnl_transport;

	/* Output function */
	dset_session_print_outfn(session, print_outfn, p);

	/* Initialize data structures */
	session->data = dset_data_init();
	if (session->data == NULL)
		goto free_outbuf;

	dset_cache_init();
	return session;

free_outbuf:
	free(session->outbuf);
free_session:
	free(session);
	return NULL;
}

/**
 * dset_session_io_full - set full IO for the session
 * @session: session structure
 * @filename: filename
 * @what: operate on input/output
 *
 * The normal "-file" CLI interface does not provide an interface
 * to set both the input (restore) and output (list/save) for
 * a session. This function makes it possible to configure those.
 *
 * When a filename for input is passed, then the file will be opened
 * for reading.
 * When a filename for output is passed, then the file will be opened
 * for writing.
 * Previously opened files are closed.
 * If NULL is passed as filename, stdin/stdout is set.
 * Input/output files can be set separatedly.
 * The function returns error if the file cannot be opened or
 * normal IO mode is already set.
 *
 * Returns 0 on success or a negative error code.
 */
int dset_session_io_full(struct dset_session *session, const char *filename,
						 enum dset_io_type what)
{
	FILE *f;

	assert(session);

	if (session->normal_io)
		return dset_err(session,
						"Normal IO is in use, full IO cannot be selected");

	switch (what)
	{
	case DSET_IO_INPUT:
		if (session->istream != stdin)
			fclose(session->istream);
		if (!filename)
		{
			session->istream = stdin;
		}
		else
		{
			f = fopen(filename, "r");
			if (!f)
				return dset_err(session,
								"Cannot open %s for reading: %s",
								filename, strerror(errno));
			session->istream = f;
		}
		break;
	case DSET_IO_OUTPUT:
		if (session->ostream != stdout)
			fclose(session->ostream);
		if (!filename)
		{
			session->ostream = stdout;
		}
		else
		{
			f = fopen(filename, "w");
			if (!f)
				return dset_err(session,
								"Cannot open %s for writing: %s",
								filename, strerror(errno));
			session->ostream = f;
		}
		break;
	default:
		return dset_err(session,
						"Library error, invalid dset_io_type");
	}
	session->full_io = !(session->istream == stdin &&
						 session->ostream == stdout);
	return 0;
}

/**
 * dset_session_io_normal - set normal IO for the session
 * @session: session structure
 * @filename: filename
 * @what: operate on input/output
 *
 * The normal "-file" CLI interface to set either the input (restore)
 * or output (list/save) for a session. This function does not make
 * possible to set both independently.
 *
 * When a filename for input is passed, then the file will be opened
 * for reading.
 * When a filename for output is passed, then the file will be opened
 * for writing.
 * Previously opened files are closed.
 * If NULL is passed as filename, stdin/stdout is set.
 * Input/output files cannot be set separatedly.
 * The function returns error if the file cannot be opened or
 * full IO mode is already set.
 *
 * Returns 0 on success or a negative error code.
 */
int dset_session_io_normal(struct dset_session *session, const char *filename,
						   enum dset_io_type what)
{
	FILE *f;

	assert(session);
	assert(filename);

	if (session->full_io)
		return dset_err(session,
						"Full IO is in use, normal IO cannot be selected");
	if (session->istream != stdin)
	{
		fclose(session->istream);
		session->istream = stdin;
	}
	if (session->ostream != stdout)
	{
		fclose(session->ostream);
		session->ostream = stdout;
	}
	switch (what)
	{
	case DSET_IO_INPUT:
		f = fopen(filename, "r");
		if (!f)
			return dset_err(session,
							"Cannot open %s for reading: %s",
							filename, strerror(errno));
		session->istream = f;
		break;
	case DSET_IO_OUTPUT:
		f = fopen(filename, "w");
		if (!f)
			return dset_err(session,
							"Cannot open %s for writing: %s",
							filename, strerror(errno));
		session->ostream = f;
		break;
	default:
		return dset_err(session,
						"Library error, invalid dset_io_type");
	}
	session->normal_io = !(session->istream == stdin &&
						   session->ostream == stdout);
	return 0;
}

/**
 * dset_session_io_stream - returns the input or output stream
 * @what: operate on input/output
 *
 * Returns the input or output stream of the session.
 */
FILE *
dset_session_io_stream(struct dset_session *session,
					   enum dset_io_type what)
{
	switch (what)
	{
	case DSET_IO_INPUT:
		return session->istream;
	case DSET_IO_OUTPUT:
		return session->ostream;
	default:
		return NULL;
	}
}

/**
 * dset_session_io_close - closes the input or output stream
 * @what: operate on input/output
 *
 * Closes the input or output stream of the session.
 *
 * Returns 0 on success or a negative error code.
 */
int dset_session_io_close(struct dset_session *session,
						  enum dset_io_type what)
{
	switch (what)
	{
	case DSET_IO_INPUT:
		if (session->istream != stdin)
		{
			fclose(session->istream);
			session->istream = stdin;
		}
		break;
	case DSET_IO_OUTPUT:
		if (session->ostream != stdout)
		{
			fclose(session->ostream);
			session->ostream = stdout;
		}
		break;
	default:
		break;
	}
	return 0;
}

/**
 * dset_session_fini - destroy an dset session
 * @session: session structure
 *
 * Destroy an dset session: release the created structures.
 *
 * Returns 0 on success or a negative error code.
 */
int dset_session_fini(struct dset_session *session)
{
	struct dset_sorted *pos, *n;
	assert(session);

	if (session->handle)
		session->transport->fini(session->handle);
	if (session->data)
		dset_data_fini(session->data);
	if (session->istream != stdin)
		fclose(session->istream);
	if (session->ostream != stdout)
		fclose(session->ostream);

	dset_cache_fini();

	list_for_each_entry_safe(pos, n, &session->sorted, list)
	{
		list_del(&pos->list);
		free(pos);
	}
	list_for_each_entry_safe(pos, n, &session->pool, list)
	{
		list_del(&pos->list);
		free(pos);
	}
	free(session->outbuf);
	free(session);
	return 0;
}

#ifdef DSET_DEBUG
#include "debug.c"
#endif
