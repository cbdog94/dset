/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <assert.h> /* assert */
#include <stdlib.h> /* malloc, free */
#include <string.h> /* memset */

#include <libdset/linux_domain_set.h> /* DSET_MAXNAMELEN */
#include <libdset/debug.h>			  /* D() */
#include <libdset/types.h>			  /* struct dset_type */
#include <libdset/utils.h>			  /* inXcpy */
#include <libdset/data.h>			  /* prototypes */

/* Internal data structure to hold
 * a) input data entered by the user or
 * b) data received from kernel
 *
 * We always store the data in host order.
 */
struct dset_data
{
	/* Option bits: which fields are set */
	uint64_t bits;
	/* Option bits: which options are ignored */
	uint64_t ignored;
	/* Setname  */
	char setname[DSET_MAXNAMELEN];
	/* Set type */
	const struct dset_type *type;
	/* Common CADT options */
	// uint8_t cidr;
	uint8_t family;
	uint32_t flags;		 /* command level flags */
	uint32_t cadt_flags; /* data level flags */
	uint32_t timeout;
	char domaindata[DSET_MAX_DOMAIN_LEN];

	uint16_t index;
	union {
		/* RENAME/SWAP */
		char setname2[DSET_MAXNAMELEN];
		/* CREATE/LIST/SAVE */
		struct
		{
			uint8_t probes;
			uint8_t resize;
			uint8_t netmask;
			uint32_t hashsize;
			uint32_t maxelem;
			uint32_t markmask;
			uint32_t gc;
			uint32_t size;
			/* Filled out by kernel */
			uint32_t references;
			uint32_t elements;
			uint32_t memsize;
			char typename[DSET_MAXNAMELEN];
			uint8_t revision_min;
			uint8_t revision;
		} create;
		/* ADT/LIST/SAVE */
		struct
		{
			char name[DSET_MAXNAMELEN];
			char nameref[DSET_MAXNAMELEN];
			uint64_t packets;
			uint64_t bytes;
			char comment[DSET_MAX_COMMENT_SIZE + 1];
			uint64_t skbmark;
			uint32_t skbprio;
			uint16_t skbqueue;
		} adt;
	};
};

/**
 * dset_strlcpy - copy the string from src to dst
 * @dst: the target string buffer
 * @src: the source string buffer
 * @len: the length of bytes to copy, including the terminating null byte.
 *
 * Copy the string from src to destination, but at most len bytes are
 * copied. The target is unconditionally terminated by the null byte.
 */
void dset_strlcpy(char *dst, const char *src, size_t len)
{
	assert(dst);
	assert(src);

	strncpy(dst, src, len);
	dst[len - 1] = '\0';
}

/**
 * dset_strlcat - concatenate the string from src to the end of dst
 * @dst: the target string buffer
 * @src: the source string buffer
 * @len: the length of bytes to concat, including the terminating null byte.
 *
 * Cooncatenate the string in src to destination, but at most len bytes are
 * copied. The target is unconditionally terminated by the null byte.
 */
void dset_strlcat(char *dst, const char *src, size_t len)
{
	assert(dst);
	assert(src);

	strncat(dst, src, len);
	dst[len - 1] = '\0';
}

/**
 * dset_data_flags_test - test option bits in the data blob
 * @data: data blob
 * @flags: the option flags to test
 *
 * Returns true if the options are already set in the data blob.
 */
bool dset_data_flags_test(const struct dset_data *data, uint64_t flags)
{
	assert(data);
	return !!(data->bits & flags);
}

/**
 * dset_data_flags_set - set option bits in the data blob
 * @data: data blob
 * @flags: the option flags to set
 *
 * The function sets the flags in the data blob so that
 * the corresponding fields are regarded as if filled with proper data.
 */
void dset_data_flags_set(struct dset_data *data, uint64_t flags)
{
	assert(data);
	data->bits |= flags;
}

/**
 * dset_data_flags_unset - unset option bits in the data blob
 * @data: data blob
 * @flags: the option flags to unset
 *
 * The function unsets the flags in the data blob.
 * This is the quick way to clear specific fields.
 */
void dset_data_flags_unset(struct dset_data *data, uint64_t flags)
{
	assert(data);
	data->bits &= ~flags;
}

#define flag_type_attr(data, opt, flag) \
	do                                  \
	{                                   \
		data->flags |= flag;            \
		opt = DSET_OPT_FLAGS;           \
	} while (0)

#define cadt_flag_type_attr(data, opt, flag) \
	do                                       \
	{                                        \
		data->cadt_flags |= flag;            \
		opt = DSET_OPT_CADT_FLAGS;           \
	} while (0)

/**
 * dset_data_ignored - test and set ignored bits in the data blob
 * @data: data blob
 * @flags: the option flag to be ignored
 *
 * Returns true if the option was already ignored.
 */
bool dset_data_ignored(struct dset_data *data, enum dset_opt opt)
{
	bool ignored;
	assert(data);

	ignored = data->ignored & DSET_FLAG(opt);
	data->ignored |= DSET_FLAG(opt);

	return ignored;
}

/**
 * dset_data_test_ignored - test ignored bits in the data blob
 * @data: data blob
 * @flags: the option flag to be tested
 *
 * Returns true if the option is ignored.
 */
bool dset_data_test_ignored(struct dset_data *data, enum dset_opt opt)
{
	assert(data);

	return data->ignored & DSET_FLAG(opt);
}

/**
 * dset_data_set - put data into the data blob
 * @data: data blob
 * @opt: the option kind of the data
 * @value: the value of the data
 *
 * Put a given kind of data into the data blob and mark the
 * option kind as already set in the blob.
 *
 * Returns 0 on success or a negative error code.
 */
int dset_data_set(struct dset_data *data, enum dset_opt opt, const void *value)
{
	assert(data);
	assert(opt != DSET_OPT_NONE);
	assert(value);

	switch (opt)
	{
	/* Common ones */
	case DSET_SETNAME:
		dset_strlcpy(data->setname, value, DSET_MAXNAMELEN);
		break;
	case DSET_OPT_TYPE:
		data->type = value;
		break;
	case DSET_OPT_FAMILY:
		data->family = *(const uint8_t *)value;
		data->ignored &= ~DSET_FLAG(DSET_OPT_FAMILY);
		D("family set to %u", data->family);
		break;
	/* CADT options */
	case DSET_OPT_DOMAIN:
		dset_strlcpy(data->domaindata, value,
					 DSET_MAX_DOMAIN_LEN);
		D("domain set to %s", data->domaindata);
		break;
	case DSET_OPT_TIMEOUT:
		data->timeout = *(const uint32_t *)value;
		break;
	case DSET_OPT_INDEX:
		data->index = *(const uint16_t *)value;
		break;
	/* Create-specific options */
	case DSET_OPT_GC:
		data->create.gc = *(const uint32_t *)value;
		break;
	case DSET_OPT_HASHSIZE:
		data->create.hashsize = *(const uint32_t *)value;
		break;
	case DSET_OPT_MAXELEM:
		data->create.maxelem = *(const uint32_t *)value;
		break;
	case DSET_OPT_PROBES:
		data->create.probes = *(const uint8_t *)value;
		break;
	case DSET_OPT_RESIZE:
		data->create.resize = *(const uint8_t *)value;
		break;
	case DSET_OPT_SIZE:
		data->create.size = *(const uint32_t *)value;
		break;
	case DSET_OPT_COUNTERS:
		cadt_flag_type_attr(data, opt, DSET_FLAG_WITH_COUNTERS);
		break;
	case DSET_OPT_CREATE_COMMENT:
		cadt_flag_type_attr(data, opt, DSET_FLAG_WITH_COMMENT);
		break;
	case DSET_OPT_FORCEADD:
		cadt_flag_type_attr(data, opt, DSET_FLAG_WITH_FORCEADD);
		break;
	case DSET_OPT_SKBINFO:
		cadt_flag_type_attr(data, opt, DSET_FLAG_WITH_SKBINFO);
		break;
	/* Create-specific options, filled out by the kernel */
	case DSET_OPT_ELEMENTS:
		data->create.elements = *(const uint32_t *)value;
		break;
	case DSET_OPT_REFERENCES:
		data->create.references = *(const uint32_t *)value;
		break;
	case DSET_OPT_MEMSIZE:
		data->create.memsize = *(const uint32_t *)value;
		break;
	/* Create-specific options, type */
	case DSET_OPT_TYPENAME:
		dset_strlcpy(data->create.typename, value,
					 DSET_MAXNAMELEN);
		break;
	case DSET_OPT_REVISION:
		data->create.revision = *(const uint8_t *)value;
		break;
	case DSET_OPT_REVISION_MIN:
		data->create.revision_min = *(const uint8_t *)value;
		break;
	/* ADT-specific options */
	case DSET_OPT_NAME:
		dset_strlcpy(data->adt.name, value, DSET_MAXNAMELEN);
		break;
	case DSET_OPT_NAMEREF:
		dset_strlcpy(data->adt.nameref, value, DSET_MAXNAMELEN);
		break;
	case DSET_OPT_PACKETS:
		data->adt.packets = *(const uint64_t *)value;
		break;
	case DSET_OPT_BYTES:
		data->adt.bytes = *(const uint64_t *)value;
		break;
	case DSET_OPT_ADT_COMMENT:
		dset_strlcpy(data->adt.comment, value,
					 DSET_MAX_COMMENT_SIZE + 1);
		break;
	case DSET_OPT_SKBMARK:
		data->adt.skbmark = *(const uint64_t *)value;
		break;
	case DSET_OPT_SKBPRIO:
		data->adt.skbprio = *(const uint32_t *)value;
		break;
	case DSET_OPT_SKBQUEUE:
		data->adt.skbqueue = *(const uint16_t *)value;
		break;
	/* Swap/rename */
	case DSET_OPT_SETNAME2:
		dset_strlcpy(data->setname2, value, DSET_MAXNAMELEN);
		break;
	/* flags */
	case DSET_OPT_EXIST:
		flag_type_attr(data, opt, DSET_FLAG_EXIST);
		break;
	case DSET_OPT_BEFORE:
		cadt_flag_type_attr(data, opt, DSET_FLAG_BEFORE);
		break;
	case DSET_OPT_PHYSDEV:
		cadt_flag_type_attr(data, opt, DSET_FLAG_PHYSDEV);
		break;
	case DSET_OPT_NOMATCH:
		cadt_flag_type_attr(data, opt, DSET_FLAG_NOMATCH);
		break;
	case DSET_OPT_FLAGS:
		data->flags = *(const uint32_t *)value;
		break;
	case DSET_OPT_CADT_FLAGS:
		data->cadt_flags = *(const uint32_t *)value;
		if (data->cadt_flags & DSET_FLAG_BEFORE)
			dset_data_flags_set(data,
								DSET_FLAG(DSET_OPT_BEFORE));
		if (data->cadt_flags & DSET_FLAG_PHYSDEV)
			dset_data_flags_set(data,
								DSET_FLAG(DSET_OPT_PHYSDEV));
		if (data->cadt_flags & DSET_FLAG_NOMATCH)
			dset_data_flags_set(data,
								DSET_FLAG(DSET_OPT_NOMATCH));
		if (data->cadt_flags & DSET_FLAG_WITH_COUNTERS)
			dset_data_flags_set(data,
								DSET_FLAG(DSET_OPT_COUNTERS));
		if (data->cadt_flags & DSET_FLAG_WITH_COMMENT)
			dset_data_flags_set(data,
								DSET_FLAG(DSET_OPT_CREATE_COMMENT));
		if (data->cadt_flags & DSET_FLAG_WITH_SKBINFO)
			dset_data_flags_set(data,
								DSET_FLAG(DSET_OPT_SKBINFO));
		break;
	default:
		return -1;
	};

	dset_data_flags_set(data, DSET_FLAG(opt));
	return 0;
}

/**
 * dset_data_get - get data from the data blob
 * @data: data blob
 * @opt: option kind of the requested data
 *
 * Returns the pointer to the requested kind of data from the data blob
 * if it is set. If the option kind is not set or is an unknown type,
 * NULL is returned.
 */
const void *
dset_data_get(const struct dset_data *data, enum dset_opt opt)
{
	assert(data);
	assert(opt != DSET_OPT_NONE);

	if (!(opt == DSET_OPT_TYPENAME || dset_data_test(data, opt)))
		return NULL;

	switch (opt)
	{
	/* Common ones */
	case DSET_SETNAME:
		return data->setname;
	case DSET_OPT_TYPE:
		return data->type;
	case DSET_OPT_TYPENAME:
		if (dset_data_test(data, DSET_OPT_TYPE))
			return data->type->name;
		else if (dset_data_test(data, DSET_OPT_TYPENAME))
			return data->create.typename;
		return NULL;
	case DSET_OPT_FAMILY:
		return &data->family;
	case DSET_OPT_DOMAIN:
		return data->domaindata;
	/* CADT options */
	case DSET_OPT_TIMEOUT:
		return &data->timeout;
	case DSET_OPT_INDEX:
		return &data->index;
	/* Create-specific options */
	case DSET_OPT_GC:
		return &data->create.gc;
	case DSET_OPT_HASHSIZE:
		return &data->create.hashsize;
	case DSET_OPT_MAXELEM:
		return &data->create.maxelem;
	case DSET_OPT_PROBES:
		return &data->create.probes;
	case DSET_OPT_RESIZE:
		return &data->create.resize;
	case DSET_OPT_SIZE:
		return &data->create.size;
	/* Create-specific options, filled out by the kernel */
	case DSET_OPT_ELEMENTS:
		return &data->create.elements;
	case DSET_OPT_REFERENCES:
		return &data->create.references;
	case DSET_OPT_MEMSIZE:
		return &data->create.memsize;
	/* Create-specific options, TYPE */
	case DSET_OPT_REVISION:
		return &data->create.revision;
	case DSET_OPT_REVISION_MIN:
		return &data->create.revision_min;
	/* ADT-specific options */
	case DSET_OPT_NAME:
		return data->adt.name;
	case DSET_OPT_NAMEREF:
		return data->adt.nameref;
	case DSET_OPT_PACKETS:
		return &data->adt.packets;
	case DSET_OPT_BYTES:
		return &data->adt.bytes;
	case DSET_OPT_ADT_COMMENT:
		return &data->adt.comment;
	case DSET_OPT_SKBMARK:
		return &data->adt.skbmark;
	case DSET_OPT_SKBPRIO:
		return &data->adt.skbprio;
	case DSET_OPT_SKBQUEUE:
		return &data->adt.skbqueue;
	/* Swap/rename */
	case DSET_OPT_SETNAME2:
		return data->setname2;
	/* flags */
	case DSET_OPT_FLAGS:
	case DSET_OPT_EXIST:
		return &data->flags;
	case DSET_OPT_CADT_FLAGS:
	case DSET_OPT_BEFORE:
	case DSET_OPT_PHYSDEV:
	case DSET_OPT_NOMATCH:
	case DSET_OPT_COUNTERS:
	case DSET_OPT_CREATE_COMMENT:
	case DSET_OPT_FORCEADD:
	case DSET_OPT_SKBINFO:
		return &data->cadt_flags;
	default:
		return NULL;
	}
}

/**
 * dset_data_sizeof - calculates the size of the data type
 * @opt: option kind of the data
 * @family: INET family
 *
 * Returns the size required to store the given data type.
 */
size_t
dset_data_sizeof(enum dset_opt opt, uint8_t family)
{
	assert(opt != DSET_OPT_NONE);

	switch (opt)
	{
	case DSET_OPT_DOMAIN:
		return DSET_MAX_DOMAIN_LEN;
	case DSET_OPT_SKBQUEUE:
	case DSET_OPT_INDEX:
		return sizeof(uint16_t);
	case DSET_SETNAME:
	case DSET_OPT_NAME:
	case DSET_OPT_NAMEREF:
		return DSET_MAXNAMELEN;
	case DSET_OPT_TIMEOUT:
	case DSET_OPT_GC:
	case DSET_OPT_HASHSIZE:
	case DSET_OPT_MAXELEM:
	case DSET_OPT_SIZE:
	case DSET_OPT_ELEMENTS:
	case DSET_OPT_REFERENCES:
	case DSET_OPT_MEMSIZE:
	case DSET_OPT_SKBPRIO:
		return sizeof(uint32_t);
	case DSET_OPT_PACKETS:
	case DSET_OPT_BYTES:
	case DSET_OPT_SKBMARK:
		return sizeof(uint64_t);
	case DSET_OPT_PROBES:
	case DSET_OPT_RESIZE:
		return sizeof(uint8_t);
	/* Flags doesn't counted once :-( */
	case DSET_OPT_BEFORE:
	case DSET_OPT_PHYSDEV:
	case DSET_OPT_NOMATCH:
	case DSET_OPT_COUNTERS:
	case DSET_OPT_FORCEADD:
		return sizeof(uint32_t);
	case DSET_OPT_ADT_COMMENT:
		return DSET_MAX_COMMENT_SIZE + 1;
	default:
		return 0;
	};
}

/**
 * dset_setname - return the name of the set from the data blob
 * @data: data blob
 *
 * Return the name of the set from the data blob or NULL if the
 * name not set yet.
 */
const char *
dset_data_setname(const struct dset_data *data)
{
	assert(data);
	return dset_data_test(data, DSET_SETNAME) ? data->setname : NULL;
}

/**
 * dset_flags - return which fields are set in the data blob
 * @data: data blob
 *
 * Returns the value of the bit field which elements are set.
 */
uint64_t
dset_data_flags(const struct dset_data *data)
{
	assert(data);
	return data->bits;
}

/**
 * dset_data_reset - reset the data blob to unset
 * @data: data blob
 *
 * Resets the data blob to the unset state for every field.
 */
void dset_data_reset(struct dset_data *data)
{
	assert(data);
	memset(data, 0, sizeof(*data));
}

/**
 * dpset_data_init - create a new data blob
 *
 * Return the new data blob initialized to empty. In case of
 * an error, NULL is retured.
 */
struct dset_data *
dset_data_init(void)
{
	return calloc(1, sizeof(struct dset_data));
}

/**
 * dset_data_fini - release a data blob created by dset_data_init
 *
 * Release the data blob created by dset_data_init previously.
 */
void dset_data_fini(struct dset_data *data)
{
	assert(data);
	free(data);
}
