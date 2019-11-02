/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <assert.h>   /* assert */
#include <errno.h>	/* errno */
#include <stdio.h>	/* snprintf */
#include <inttypes.h> /* PRIx macro */

#include <libdset/debug.h>   /* D() */
#include <libdset/data.h>	/* dset_data_* */
#include <libdset/parse.h>   /* DSET_*_SEPARATOR */
#include <libdset/types.h>   /* dset set types */
#include <libdset/session.h> /* DSET_FLAG_ */
#include <libdset/utils.h>   /* UNUSED */
#include <libdset/dset.h>	/* DSET_ENV_* */
#include <libdset/print.h>   /* prototypes */

/* Print data (to output buffer). All function must follow snprintf. */

#define SNPRINTF_FAILURE(size, len, offset)        \
	do                                             \
	{                                              \
		if (size < 0 || (unsigned int)size >= len) \
			return offset + size;                  \
		offset += size;                            \
		len -= size;                               \
	} while (0)

/**
 * dset_print_type - print dset type string
 * @buf: printing buffer
 * @len: length of available buffer space
 * @data: data blob
 * @opt: the option kind
 * @env: environment flags
 *
 * Print dset module string identifier to output buffer.
 *
 * Return lenght of printed string or error size.
 */
int dset_print_type(char *buf, unsigned int len,
					const struct dset_data *data, enum dset_opt opt,
					uint8_t env UNUSED)
{
	const struct dset_type *type;

	assert(buf);
	assert(len > 0);
	assert(data);
	assert(opt == DSET_OPT_TYPE);

	type = dset_data_get(data, opt);
	assert(type);
	if (len < strlen(type->name) + 1)
		return -1;

	return snprintf(buf, len, "%s", type->name);
}

/**
 * dset_print_number - print number to string
 * @buf: printing buffer
 * @len: length of available buffer space
 * @data: data blob
 * @opt: the option kind
 * @env: environment flags
 *
 * Print number to output buffer.
 *
 * Return lenght of printed string or error size.
 */
int dset_print_number(char *buf, unsigned int len,
					  const struct dset_data *data, enum dset_opt opt,
					  uint8_t env UNUSED)
{
	size_t maxsize;
	const void *number;

	assert(buf);
	assert(len > 0);
	assert(data);

	number = dset_data_get(data, opt);
	maxsize = dset_data_sizeof(opt, AF_INET);
	D("opt: %u, maxsize %zu", opt, maxsize);
	if (maxsize == sizeof(uint8_t))
		return snprintf(buf, len, "%u", *(const uint8_t *)number);
	else if (maxsize == sizeof(uint16_t))
		return snprintf(buf, len, "%u", *(const uint16_t *)number);
	else if (maxsize == sizeof(uint32_t))
		return snprintf(buf, len, "%lu",
						(long unsigned)*(const uint32_t *)number);
	else if (maxsize == sizeof(uint64_t))
		return snprintf(buf, len, "%llu",
						(long long unsigned)*(const uint64_t *)number);
	else
		assert(0);
	return 0;
}

/**
 * dset_print_name - print setname element string
 * @buf: printing buffer
 * @len: length of available buffer space
 * @data: data blob
 * @opt: the option kind
 * @env: environment flags
 *
 * Print setname element string to output buffer.
 *
 * Return lenght of printed string or error size.
 */
int dset_print_name(char *buf, unsigned int len,
					const struct dset_data *data, enum dset_opt opt,
					uint8_t env UNUSED)
{
	const char *name;
	int size, offset = 0;

	assert(buf);
	assert(len > 0);
	assert(data);
	assert(opt == DSET_OPT_NAME);

	if (len < 2 * DSET_MAXNAMELEN + 2 + strlen("before"))
		return -1;

	name = dset_data_get(data, opt);
	assert(name);
	size = snprintf(buf, len, "%s", name);
	SNPRINTF_FAILURE(size, len, offset);

	if (dset_data_test(data, DSET_OPT_NAMEREF))
	{
		bool before = false;
		if (dset_data_flags_test(data, DSET_FLAG(DSET_OPT_FLAGS)))
		{
			const uint32_t *flags =
				dset_data_get(data, DSET_OPT_FLAGS);
			before = (*flags) & DSET_FLAG_BEFORE;
		}
		size = snprintf(buf + offset, len,
						" %s %s", before ? "before" : "after",
						(const char *)dset_data_get(data,
													DSET_OPT_NAMEREF));
		SNPRINTF_FAILURE(size, len, offset);
	}

	return offset;
}

/**
 * dset_print_domain - print domain data
 * @buf: printing buffer
 * @len: length of available buffer space
 * @data: data blob
 * @opt: the option kind
 * @env: environment flags
 *
 * Print domain data string to output buffer.
 *
 * Return lenght of printed string or error size.
 */
int dset_print_domain(char *buf, unsigned int len,
					  const struct dset_data *data, enum dset_opt opt,
					  uint8_t env UNUSED)
{
	const char *domain;
	int size, offset = 0;

	assert(buf);
	assert(len > 0);
	assert(data);
	assert(opt == DSET_OPT_DOMAIN);
	domain = dset_data_get(data, opt);
	assert(domain);
	size = snprintf(buf + offset, len, "%s", domain);
	SNPRINTF_FAILURE(size, len, offset);
	return offset;
}

/**
 * dset_print_comment - print arbitrary parameter string
 * @buf: printing buffer
 * @len: length of available buffer space
 * @data: data blob
 * @opt: the option kind
 * @env: environment flags
 *
 * Print arbitrary string to output buffer.
 *
 * Return length of printed string or error size.
 */
int dset_print_comment(char *buf, unsigned int len,
					   const struct dset_data *data, enum dset_opt opt,
					   uint8_t env UNUSED)
{
	const char *comment;
	int size, offset = 0;

	assert(buf);
	assert(len > 0);
	assert(data);
	assert(opt == DSET_OPT_ADT_COMMENT);

	comment = dset_data_get(data, opt);
	assert(comment);
	size = snprintf(buf + offset, len, "\"%s\"", comment);
	SNPRINTF_FAILURE(size, len, offset);
	return offset;
}

int dset_print_skbmark(char *buf, unsigned int len,
					   const struct dset_data *data, enum dset_opt opt,
					   uint8_t env UNUSED)
{
	int size, offset = 0;
	const uint64_t *skbmark;
	uint32_t mark, mask;

	assert(buf);
	assert(len > 0);
	assert(data);
	assert(opt == DSET_OPT_SKBMARK);

	skbmark = dset_data_get(data, DSET_OPT_SKBMARK);
	assert(skbmark);
	mark = *skbmark >> 32;
	mask = *skbmark & 0xffffffff;
	if (mask == 0xffffffff)
		size = snprintf(buf + offset, len, "0x%" PRIx32, mark);
	else
		size = snprintf(buf + offset, len,
						"0x%" PRIx32 "/0x%" PRIx32, mark, mask);
	SNPRINTF_FAILURE(size, len, offset);
	return offset;
}

int dset_print_skbprio(char *buf, unsigned int len,
					   const struct dset_data *data, enum dset_opt opt,
					   uint8_t env UNUSED)
{
	int size, offset = 0;
	const uint32_t *skbprio;

	assert(buf);
	assert(len > 0);
	assert(data);
	assert(opt == DSET_OPT_SKBPRIO);

	skbprio = dset_data_get(data, opt);
	assert(skbprio);
	size = snprintf(buf + offset, len, "%x:%x",
					*skbprio >> 16, *skbprio & 0xffff);
	SNPRINTF_FAILURE(size, len, offset);
	return offset;
}

/**
 * dset_print_elem - print ADT elem according to settype
 * @buf: printing buffer
 * @len: length of available buffer space
 * @data: data blob
 * @opt: the option kind
 * @env: environment flags
 *
 * Print (multipart) element according to settype
 *
 * Return lenght of printed string or error size.
 */
int dset_print_elem(char *buf, unsigned int len,
					const struct dset_data *data, enum dset_opt opt UNUSED,
					uint8_t env)
{
	const struct dset_type *type;
	int size, offset = 0;

	assert(buf);
	assert(len > 0);
	assert(data);

	type = dset_data_get(data, DSET_OPT_TYPE);
	if (!type)
		return -1;

	size = type->elem[DSET_DIM_ONE - 1].print(buf, len, data,
											  type->elem[DSET_DIM_ONE - 1].opt, env);
	SNPRINTF_FAILURE(size, len, offset);
	IF_D(dset_data_test(data, type->elem[DSET_DIM_TWO - 1].opt),
		 "print second elem");
	if (type->dimension == DSET_DIM_ONE ||
		(type->last_elem_optional &&
		 !dset_data_test(data, type->elem[DSET_DIM_TWO - 1].opt)))
		return offset;

	size = snprintf(buf + offset, len, DSET_ELEM_SEPARATOR);
	SNPRINTF_FAILURE(size, len, offset);
	size = type->elem[DSET_DIM_TWO - 1].print(buf + offset, len, data,
											  type->elem[DSET_DIM_TWO - 1].opt, env);
	SNPRINTF_FAILURE(size, len, offset);
	if (type->dimension == DSET_DIM_TWO ||
		(type->last_elem_optional &&
		 !dset_data_test(data, type->elem[DSET_DIM_THREE - 1].opt)))
		return offset;

	size = snprintf(buf + offset, len, DSET_ELEM_SEPARATOR);
	SNPRINTF_FAILURE(size, len, offset);
	size = type->elem[DSET_DIM_THREE - 1].print(buf + offset, len, data,
												type->elem[DSET_DIM_THREE - 1].opt, env);
	SNPRINTF_FAILURE(size, len, offset);

	return offset;
}

/**
 * dset_print_flag - print a flag
 * @buf: printing buffer
 * @len: length of available buffer space
 * @data: data blob
 * @opt: the option kind
 * @env: environment flags
 *
 * Print a flag, i.e. option without value
 *
 * Return lenght of printed string or error size.
 */
int dset_print_flag(char *buf UNUSED, unsigned int len UNUSED,
					const struct dset_data *data UNUSED,
					enum dset_opt opt UNUSED, uint8_t env UNUSED)
{
	return 0;
}

/**
 * dset_print_data - print data, generic fuction
 * @buf: printing buffer
 * @len: length of available buffer space
 * @data: data blob
 * @opt: the option kind
 * @env: environment flags
 *
 * Generic wrapper of the printing functions.
 *
 * Return lenght of printed string or error size.
 */
int dset_print_data(char *buf, unsigned int len,
					const struct dset_data *data, enum dset_opt opt,
					uint8_t env)
{
	int size = 0, offset = 0;

	assert(buf);
	assert(len > 0);
	assert(data);

	switch (opt)
	{
	case DSET_OPT_DOMAIN:
		size = dset_print_domain(buf, len, data, opt, env);
		break;
	case DSET_OPT_TYPE:
		size = dset_print_type(buf, len, data, opt, env);
		break;
	case DSET_SETNAME:
		size = snprintf(buf, len, "%s", dset_data_setname(data));
		break;
	case DSET_OPT_ELEM:
		size = dset_print_elem(buf, len, data, opt, env);
		break;
	case DSET_OPT_GC:
	case DSET_OPT_HASHSIZE:
	case DSET_OPT_MAXELEM:
	case DSET_OPT_PROBES:
	case DSET_OPT_RESIZE:
	case DSET_OPT_TIMEOUT:
	case DSET_OPT_REFERENCES:
	case DSET_OPT_ELEMENTS:
	case DSET_OPT_SIZE:
		size = dset_print_number(buf, len, data, opt, env);
		break;
	default:
		return -1;
	}
	SNPRINTF_FAILURE(size, len, offset);

	return offset;
}
