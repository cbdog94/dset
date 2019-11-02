/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <assert.h> /* assert */
#include <errno.h>  /* errno */
#include <limits.h> /* ULLONG_MAX */
#include <netdb.h>  /* getservbyname, getaddrinfo */
#include <stdlib.h> /* strtoull, etc. */
#include <stdio.h>  /* strtoull, etc. */

#include <libdset/debug.h>   /* D() */
#include <libdset/data.h>	/* DSET_OPT_* */
#include <libdset/session.h> /* dset_err */
#include <libdset/types.h>   /* dset_type_get */
#include <libdset/utils.h>   /* string utilities */
#include <libdset/parse.h>   /* prototypes */
#include "../config.h"

int dset_parse_domain(struct dset_session *session,
					  enum dset_opt opt, const char *str)
{
	struct dset_data *data;

	assert(session);
	assert(opt == DSET_OPT_DOMAIN);
	assert(str);

	data = dset_session_data(session);
	return dset_data_set(data, DSET_OPT_DOMAIN, str);
}

#ifndef ULLONG_MAX
#define ULLONG_MAX 18446744073709551615ULL
#endif

/* Parse input data */
#define elem_separator(str) dset_strchr(str, DSET_ELEM_SEPARATOR)
#define name_separator(str) dset_strchr(str, DSET_NAME_SEPARATOR)

#define syntax_err(fmt, args...) \
	dset_err(session, "Syntax error: " fmt, ##args)

static char *
dset_strchr(const char *str, const char *sep)
{
	char *match;

	assert(str);
	assert(sep);

	for (; *sep != '\0'; sep++)
	{
		match = strchr(str, sep[0]);
		if (match != NULL &&
			str[0] != sep[0] &&
			str[strlen(str) - 1] != sep[0])
			return match;
	}

	return NULL;
}

/*
 * Parse numbers
 */
static int
string_to_number_ll(struct dset_session *session,
					const char *str,
					unsigned long long min,
					unsigned long long max,
					unsigned long long *ret)
{
	unsigned long long number = 0;
	char *end;

	/* Handle hex, octal, etc. */
	errno = 0;
	number = strtoull(str, &end, 0);
	if (*end == '\0' && end != str && errno != ERANGE)
	{
		/* we parsed a number, let's see if we want this */
		if (min <= number && (!max || number <= max))
		{
			*ret = number;
			return 0;
		}
		else
			errno = ERANGE;
	}
	if (errno == ERANGE && max)
		return syntax_err("'%s' is out of range %llu-%llu",
						  str, min, max);
	else if (errno == ERANGE)
		return syntax_err("'%s' is out of range %llu-%llu",
						  str, min, ULLONG_MAX);
	else
		return syntax_err("'%s' is invalid as number", str);
}

static int
string_to_u8(struct dset_session *session,
			 const char *str, uint8_t *ret)
{
	int err;
	unsigned long long num = 0;

	err = string_to_number_ll(session, str, 0, 255, &num);
	*ret = num;

	return err;
}

static int
string_to_u16(struct dset_session *session,
			  const char *str, uint16_t *ret)
{
	int err;
	unsigned long long num = 0;

	err = string_to_number_ll(session, str, 0, USHRT_MAX, &num);
	*ret = num;

	return err;
}

static int
string_to_u32(struct dset_session *session,
			  const char *str, uint32_t *ret)
{
	int err;
	unsigned long long num = 0;

	err = string_to_number_ll(session, str, 0, UINT_MAX, &num);
	*ret = num;

	return err;
}

static char *
dset_strdup(struct dset_session *session, const char *str)
{
	char *tmp = strdup(str);

	if (tmp == NULL)
		dset_err(session,
				 "Cannot allocate memory to duplicate %s.",
				 str);
	return tmp;
}

static char *
strip_escape(struct dset_session *session, char *str)
{
	if (STRNEQ(str, DSET_ESCAPE_START, 1))
	{
		if (!STREQ(str + strlen(str) - 1, DSET_ESCAPE_END))
		{
			syntax_err("cannot find closing escape character "
					   "'%s' in %s",
					   DSET_ESCAPE_END, str);
			return NULL;
		}
		str++;
		str[strlen(str) - 1] = '\0';
	}
	return str;
}

static void
print_warn(struct dset_session *session)
{
	if (!dset_envopt_test(session, DSET_ENV_QUIET))
		fprintf(stderr, "Warning: %s",
				dset_session_report_msg(session));
	dset_session_report_reset(session);
}

/**
 * dset_parse_timeout - parse timeout parameter
 * @session: session structure
 * @opt: option kind of the data
 * @str: string to parse
 *
 * Parse string as a timeout parameter. We have to take into account
 * the jiffies storage in kernel.
 *
 * Returns 0 on success or a negative error code.
 */
int dset_parse_timeout(struct dset_session *session,
					   enum dset_opt opt, const char *str)
{
	int err;
	unsigned long long llnum = 0;
	uint32_t num = 0;

	assert(session);
	assert(opt == DSET_OPT_TIMEOUT);
	assert(str);

	err = string_to_number_ll(session, str, 0, (UINT_MAX >> 1) / 1000, &llnum);
	if (err == 0)
	{
		/* Timeout is expected to be 32bits wide, so we have
		   to convert it here */
		num = llnum;
		return dset_session_data_set(session, opt, &num);
	}

	return err;
}

#define check_setname(str, saved)                                           \
	do                                                                      \
	{                                                                       \
		if (strlen(str) > DSET_MAXNAMELEN - 1)                              \
		{                                                                   \
			int __err;                                                      \
			__err = syntax_err("setname '%s' is longer than %u characters", \
							   str, DSET_MAXNAMELEN - 1);                   \
			free(saved);                                                    \
			return __err;                                                   \
		}                                                                   \
	} while (0)

/**
 * dset_parse_name_compat - parse setname as element
 * @session: session structure
 * @opt: option kind of the data
 * @str: string to parse
 *
 * Parse string as a setname or a setname element to add to a set.
 * The pattern "setname,before|after,setname" is recognized and
 * parsed.
 * The value is stored in the data blob of the session.
 *
 * Returns 0 on success or a negative error code.
 */
int dset_parse_name_compat(struct dset_session *session,
						   enum dset_opt opt, const char *str)
{
	char *saved;
	char *a = NULL, *b = NULL, *tmp;
	int err, before = 0;
	const char *sep = DSET_ELEM_SEPARATOR;
	struct dset_data *data;

	assert(session);
	assert(opt == DSET_OPT_NAME);
	assert(str);

	data = dset_session_data(session);
	if (dset_data_flags_test(data, DSET_FLAG(DSET_OPT_NAMEREF)))
		syntax_err("mixed syntax, before|after option already used");

	tmp = saved = dset_strdup(session, str);
	if (saved == NULL)
		return -1;
	if ((a = elem_separator(tmp)) != NULL)
	{
		/* setname,[before|after,setname */
		*a++ = '\0';
		if ((b = elem_separator(a)) != NULL)
			*b++ = '\0';
		if (b == NULL ||
			!(STREQ(a, "before") || STREQ(a, "after")))
		{
			err = dset_err(session, "you must specify elements "
									"as setname%s[before|after]%ssetname",
						   sep, sep);
			goto out;
		}
		before = STREQ(a, "before");
	}
	check_setname(tmp, saved);
	if ((err = dset_data_set(data, opt, tmp)) != 0 || b == NULL)
		goto out;

	check_setname(b, saved);
	if ((err = dset_data_set(data,
							 DSET_OPT_NAMEREF, b)) != 0)
		goto out;

	if (before)
		err = dset_data_set(data, DSET_OPT_BEFORE, &before);

out:
	free(saved);
	return err;
}

/**
 * dset_parse_setname - parse string as a setname
 * @session: session structure
 * @opt: option kind of the data
 * @str: string to parse
 *
 * Parse string as a setname.
 * The value is stored in the data blob of the session.
 *
 * Returns 0 on success or a negative error code.
 */
int dset_parse_setname(struct dset_session *session,
					   enum dset_opt opt, const char *str)
{
	assert(session);
	assert(opt == DSET_SETNAME ||
		   opt == DSET_OPT_NAME ||
		   opt == DSET_OPT_SETNAME2);
	assert(str);

	check_setname(str, NULL);

	return dset_session_data_set(session, opt, str);
}

/**
 * dset_parse_before - parse string as "before" reference setname
 * @session: session structure
 * @opt: option kind of the data
 * @str: string to parse
 *
 * Parse string as a "before" reference setname for list:set
 * type of sets. The value is stored in the data blob of the session.
 *
 * Returns 0 on success or a negative error code.
 */
int dset_parse_before(struct dset_session *session,
					  enum dset_opt opt, const char *str)
{
	struct dset_data *data;

	assert(session);
	assert(opt == DSET_OPT_NAMEREF);
	assert(str);

	data = dset_session_data(session);
	if (dset_data_flags_test(data, DSET_FLAG(DSET_OPT_NAMEREF)))
		syntax_err("mixed syntax, before|after option already used");

	check_setname(str, NULL);
	dset_data_set(data, DSET_OPT_BEFORE, str);

	return dset_data_set(data, opt, str);
}

/**
 * dset_parse_after - parse string as "after" reference setname
 * @session: session structure
 * @opt: option kind of the data
 * @str: string to parse
 *
 * Parse string as a "after" reference setname for list:set
 * type of sets. The value is stored in the data blob of the session.
 *
 * Returns 0 on success or a negative error code.
 */
int dset_parse_after(struct dset_session *session,
					 enum dset_opt opt, const char *str)
{
	struct dset_data *data;

	assert(session);
	assert(opt == DSET_OPT_NAMEREF);
	assert(str);

	data = dset_session_data(session);
	if (dset_data_flags_test(data, DSET_FLAG(DSET_OPT_NAMEREF)))
		syntax_err("mixed syntax, before|after option already used");

	check_setname(str, NULL);

	return dset_data_set(data, opt, str);
}

/**
 * dset_parse_uint64 - parse string as an unsigned long integer
 * @session: session structure
 * @opt: option kind of the data
 * @str: string to parse
 *
 * Parse string as an unsigned long integer number.
 * The value is stored in the data blob of the session.
 *
 * Returns 0 on success or a negative error code.
 */
int dset_parse_uint64(struct dset_session *session,
					  enum dset_opt opt, const char *str)
{
	unsigned long long value = 0;
	int err;

	assert(session);
	assert(str);

	err = string_to_number_ll(session, str, 0, ULLONG_MAX - 1, &value);
	if (err)
		return err;

	return dset_session_data_set(session, opt, &value);
}

/**
 * dset_parse_uint32 - parse string as an unsigned integer
 * @session: session structure
 * @opt: option kind of the data
 * @str: string to parse
 *
 * Parse string as an unsigned integer number.
 * The value is stored in the data blob of the session.
 *
 * Returns 0 on success or a negative error code.
 */
int dset_parse_uint32(struct dset_session *session,
					  enum dset_opt opt, const char *str)
{
	uint32_t value;
	int err;

	assert(session);
	assert(str);

	if ((err = string_to_u32(session, str, &value)) == 0)
		return dset_session_data_set(session, opt, &value);

	return err;
}

int dset_parse_uint16(struct dset_session *session,
					  enum dset_opt opt, const char *str)
{
	uint16_t value;
	int err;

	assert(session);
	assert(str);

	err = string_to_u16(session, str, &value);
	if (err == 0)
		return dset_session_data_set(session, opt, &value);

	return err;
}

/**
 * dset_parse_uint8 - parse string as an unsigned short integer
 * @session: session structure
 * @opt: option kind of the data
 * @str: string to parse
 *
 * Parse string as an unsigned short integer number.
 * The value is stored in the data blob of the session.
 *
 * Returns 0 on success or a negative error code.
 */
int dset_parse_uint8(struct dset_session *session,
					 enum dset_opt opt, const char *str)
{
	uint8_t value;
	int err;

	assert(session);
	assert(str);

	if ((err = string_to_u8(session, str, &value)) == 0)
		return dset_session_data_set(session, opt, &value);

	return err;
}

/**
 * dset_parse_flag - "parse" option flags
 * @session: session structure
 * @opt: option kind of the data
 * @str: string to parse
 *
 * Parse option flags :-)
 * The value is stored in the data blob of the session.
 *
 * Returns 0 on success or a negative error code.
 */
int dset_parse_flag(struct dset_session *session,
					enum dset_opt opt, const char *str)
{
	assert(session);

	return dset_session_data_set(session, opt, str);
}

/**
 * dset_parse_type - parse dset type name
 * @session: session structure
 * @opt: option kind of the data
 * @str: string to parse
 *
 * Parse dset module type: supports both old and new formats.
 * The type name is looked up and the type found is stored
 * in the data blob of the session.
 *
 * Returns 0 on success or a negative error code.
 */
int dset_parse_typename(struct dset_session *session,
						enum dset_opt opt ASSERT_UNUSED, const char *str)
{
	const struct dset_type *type;
	const char *typename;

	assert(session);
	assert(opt == DSET_OPT_TYPENAME);
	assert(str);

	if (strlen(str) > DSET_MAXNAMELEN - 1)
		return syntax_err("typename '%s' is longer than %u characters",
						  str, DSET_MAXNAMELEN - 1);

	/* Find the corresponding type */
	typename = dset_typename_resolve(str);
	if (typename == NULL)
		return syntax_err("typename '%s' is unknown", str);
	dset_session_data_set(session, DSET_OPT_TYPENAME, typename);
	type = dset_type_get(session, DSET_CMD_CREATE);

	if (type == NULL)
		return -1;

	return dset_session_data_set(session, DSET_OPT_TYPE, type);
}

/**
 * dset_parse_comment - parse string as a comment
 * @session: session structure
 * @opt: option kind of the data
 * @str: string to parse
 *
 * Parse string for use as a comment on an dset entry.
 * Gets stored in the data blob as usual.
 *
 * Returns 0 on success or a negative error code.
 */
int dset_parse_comment(struct dset_session *session,
					   enum dset_opt opt, const char *str)
{
	struct dset_data *data;

	assert(session);
	assert(opt == DSET_OPT_ADT_COMMENT);
	assert(str);

	data = dset_session_data(session);
	if (strchr(str, '"'))
		return syntax_err("\" character is not permitted in comments");
	if (strlen(str) > DSET_MAX_COMMENT_SIZE)
		return syntax_err("Comment is longer than the maximum allowed "
						  "%d characters",
						  DSET_MAX_COMMENT_SIZE);
	return dset_data_set(data, opt, str);
}

int dset_parse_skbmark(struct dset_session *session,
					   enum dset_opt opt, const char *str)
{
	struct dset_data *data;
	uint64_t result = 0;
	unsigned long mark, mask;
	int ret = 0;

	assert(session);
	assert(opt == DSET_OPT_SKBMARK);
	assert(str);

	data = dset_session_data(session);
	ret = sscanf(str, "0x%lx/0x%lx", &mark, &mask);
	if (ret != 2)
	{
		mask = 0xffffffff;
		ret = sscanf(str, "0x%lx", &mark);
		if (ret != 1)
			return syntax_err("Invalid skbmark format, "
							  "it should be: "
							  " MARK/MASK or MARK (see manpage)");
	}
	result = ((uint64_t)(mark) << 32) | (mask & 0xffffffff);
	return dset_data_set(data, opt, &result);
}

int dset_parse_skbprio(struct dset_session *session,
					   enum dset_opt opt, const char *str)
{
	struct dset_data *data;
	unsigned maj, min;
	uint32_t major;
	int err;

	assert(session);
	assert(opt == DSET_OPT_SKBPRIO);
	assert(str);

	data = dset_session_data(session);
	err = sscanf(str, "%x:%x", &maj, &min);
	if (err != 2)
		return syntax_err("Invalid skbprio format, it should be:"
						  "MAJOR:MINOR (see manpage)");
	major = ((uint32_t)maj << 16) | (min & 0xffff);
	return dset_data_set(data, opt, &major);
}

/**
 * dset_parse_ignored - "parse" ignored option
 * @session: session structure
 * @opt: option kind of the data
 * @str: string to parse
 *
 * Ignore deprecated options. A single warning is generated
 * for every ignored opton.
 *
 * Returns 0.
 */
int dset_parse_ignored(struct dset_session *session,
					   enum dset_opt opt, const char *str)
{
	assert(session);
	assert(str);

	if (!dset_data_ignored(dset_session_data(session), opt))
		dset_warn(session,
				  "Option '--%s %s' is ignored. "
				  "Please upgrade your syntax.",
				  dset_ignored_optname(opt), str);

	return 0;
}

/**
 * dset_call_parser - call a parser function
 * @session: session structure
 * @parsefn: parser function
 * @optstr: option name
 * @opt: option kind of the data
 * @str: string to parse
 *
 * Wrapper to call the parser functions so that ignored options
 * are handled properly.
 *
 * Returns 0 on success or a negative error code.
 */
int dset_call_parser(struct dset_session *session,
					 const struct dset_arg *arg,
					 const char *str)
{
	struct dset_data *data = dset_session_data(session);

	if (dset_data_flags_test(data, DSET_FLAG(arg->opt)))
		return syntax_err("%s already specified", arg->name[0]);

	return arg->parse(session, arg->opt, str);
}

#define parse_elem(s, t, d, str)                                    \
	do                                                              \
	{                                                               \
		if (!(t)->elem[d - 1].parse)                                \
			goto internal;                                          \
		ret = (t)->elem[d - 1].parse(s, (t)->elem[d - 1].opt, str); \
		if (ret)                                                    \
			goto out;                                               \
	} while (0)

#define elem_syntax_err(fmt, args...)   \
	do                                  \
	{                                   \
		free(saved);                    \
		return syntax_err(fmt, ##args); \
	} while (0)

/**
 * dset_parse_elem - parse ADT elem, depending on settype
 * @session: session structure
 * @opt: option kind of the data
 * @str: string to parse
 *
 * Parse string as a (multipart) element according to the settype.
 * The value is stored in the data blob of the session.
 *
 * Returns 0 on success or a negative error code.
 */
int dset_parse_elem(struct dset_session *session,
					bool optional, const char *str)
{
	const struct dset_type *type;
	char *a = NULL, *b = NULL, *tmp, *saved;
	int ret;

	assert(session);
	assert(str);

	type = dset_session_data_get(session, DSET_OPT_TYPE);
	if (!type)
		return dset_err(session,
						"Internal error: set type is unknown!");

	saved = tmp = dset_strdup(session, str);
	if (tmp == NULL)
		return -1;

	a = elem_separator(tmp);
	if (type->dimension > DSET_DIM_ONE)
	{
		if (a != NULL)
		{
			/* elem,elem */
			*a++ = '\0';
		}
		else if (!optional)
			elem_syntax_err("Second element is missing from %s.",
							str);
	}
	else if (a != NULL)
	{
		if (type->compat_parse_elem)
		{
			ret = type->compat_parse_elem(session,
										  type->elem[DSET_DIM_ONE - 1].opt,
										  saved);
			goto out;
		}
		elem_syntax_err("Elem separator in %s, "
						"but settype %s supports none.",
						str, type->name);
	}

	if (a)
		b = elem_separator(a);
	if (type->dimension > DSET_DIM_TWO)
	{
		if (b != NULL)
		{
			/* elem,elem,elem */
			*b++ = '\0';
		}
		else if (!optional)
			elem_syntax_err("Third element is missing from %s.",
							str);
	}
	else if (b != NULL)
		elem_syntax_err("Two elem separators in %s, "
						"but settype %s supports one.",
						str, type->name);
	if (b != NULL && elem_separator(b))
		elem_syntax_err("Three elem separators in %s, "
						"but settype %s supports two.",
						str, type->name);

	D("parse elem part one: %s", tmp);
	parse_elem(session, type, DSET_DIM_ONE, tmp);

	if (type->dimension > DSET_DIM_ONE && a != NULL)
	{
		D("parse elem part two: %s", a);
		parse_elem(session, type, DSET_DIM_TWO, a);
	}
	if (type->dimension > DSET_DIM_TWO && b != NULL)
	{
		D("parse elem part three: %s", b);
		parse_elem(session, type, DSET_DIM_THREE, b);
	}

	goto out;

internal:
	ret = dset_err(session,
				   "Internal error: missing parser function for %s",
				   type->name);
out:
	free(saved);
	return ret;
}
