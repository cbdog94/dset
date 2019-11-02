/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <assert.h> /* assert */
#include <errno.h>  /* errno */
#include <string.h> /* strerror */

#include <libdset/debug.h>   /* D() */
#include <libdset/data.h>	/* dset_data_get */
#include <libdset/session.h> /* dset_err */
#include <libdset/types.h>   /* struct dset_type */
#include <libdset/utils.h>   /* STRNEQ */
#include <libdset/errcode.h> /* prototypes */
#include <libdset/linux_domain_set_hash.h> /* hash specific errcodes */

/* Core kernel error codes */
static const struct dset_errcode_table core_errcode_table[] = {
	/* Generic error codes */
	{ENOENT, 0,
	 "The set with the given name does not exist"},
	{EMSGSIZE, 0,
	 "Kernel error received: message could not be created"},
	{DSET_ERR_PROTOCOL, 0,
	 "Kernel error received: dset protocol error"},

	/* CREATE specific error codes */
	{EEXIST, DSET_CMD_CREATE,
	 "Set cannot be created: set with the same name already exists"},
	{DSET_ERR_FIND_TYPE, 0,
	 "Kernel error received: set type not supported"},
	{DSET_ERR_MAX_SETS, 0,
	 "Kernel error received: maximal number of sets reached, "
	 "cannot create more."},
	{DSET_ERR_INVALID_FAMILY, 0,
	 "Protocol family not supported by the set type"},

	/* DESTROY specific error codes */
	{DSET_ERR_BUSY, DSET_CMD_DESTROY,
	 "Set cannot be destroyed: it is in use by a kernel component"},

	/* FLUSH specific error codes */

	/* RENAME specific error codes */
	{DSET_ERR_EXIST_SETNAME2, DSET_CMD_RENAME,
	 "Set cannot be renamed: a set with the new name already exists"},
	{DSET_ERR_REFERENCED, DSET_CMD_RENAME,
	 "Set cannot be renamed: it is in use by another system"},

	/* SWAP specific error codes */
	{DSET_ERR_EXIST_SETNAME2, DSET_CMD_SWAP,
	 "Sets cannot be swapped: the second set does not exist"},
	{DSET_ERR_TYPE_MISMATCH, DSET_CMD_SWAP,
	 "The sets cannot be swapped: their type does not match"},

	/* LIST/SAVE specific error codes */

	/* Generic (CADT) error codes */
	{DSET_ERR_TIMEOUT, 0,
	 "Timeout cannot be used: set was created without timeout support"},
	{DSET_ERR_COUNTER, 0,
	 "Packet/byte counters cannot be used: set was created without counter support"},
	{DSET_ERR_COMMENT, 0,
	 "Comment cannot be used: set was created without comment support"},
	{DSET_ERR_SKBINFO, 0,
	 "Skbinfo mapping cannot be used: set was created without skbinfo support"},

	/* ADD specific error codes */
	{DSET_ERR_EXIST, DSET_CMD_ADD,
	 "Element cannot be added to the set: it's already added"},

	/* DEL specific error codes */
	{DSET_ERR_EXIST, DSET_CMD_DEL,
	 "Element cannot be deleted from the set: it's not added"},

	/* TEST specific error codes */

	/* HEADER specific error codes */

	/* TYPE specific error codes */
	{EEXIST, DSET_CMD_TYPE,
	 "Kernel error received: set type does not supported"},

	/* PROTOCOL specific error codes */

	{},
};

/* Hash type-specific error codes */
static const struct dset_errcode_table hash_errcode_table[] = {
	/* Generic (CADT) error codes */
	{DSET_ERR_HASH_FULL, 0,
	 "Hash is full, cannot add more elements"},
	{DSET_ERR_HASH_ELEM, 0,
	 "Null-valued element, cannot be stored in a hash type of set"},
	{DSET_ERR_INVALID_PROTO, 0,
	 "Invalid protocol specified"},
	{DSET_ERR_MISSING_PROTO, 0,
	 "Protocol missing, but must be specified"},
	{DSET_ERR_HASH_RANGE_UNSUPPORTED, 0,
	 "Range is not supported in the \"net\" component of the element"},
	{DSET_ERR_HASH_RANGE, 0,
	 "Invalid range, covers the whole address space"},
	{},
};

/* Match set type names */
#define MATCH_TYPENAME(a, b) STRNEQ(a, b, strlen(b))

/**
 * dset_errcode - interpret a kernel error code
 * @session: session structure
 * @errcode: errcode
 *
 * Find the error code and print the appropriate
 * error message into the error buffer.
 *
 * Returns -1.
 */
int dset_errcode(struct dset_session *session, enum dset_cmd cmd, int errcode)
{
	const struct dset_errcode_table *table = core_errcode_table;
	int i, generic;

	if (errcode >= DSET_ERR_TYPE_SPECIFIC)
	{
		const struct dset_type *type;

		type = dset_saved_type(session);
		if (type)
		{
			if (MATCH_TYPENAME(type->name, "hash:"))
				table = hash_errcode_table;
		}
	}

retry:
	for (i = 0, generic = -1; table[i].errcode; i++)
	{
		if (table[i].errcode == errcode &&
			(table[i].cmd == cmd || table[i].cmd == 0))
		{
			if (table[i].cmd == 0)
			{
				generic = i;
				continue;
			}
			return dset_err(session, table[i].message);
		}
	}
	if (generic != -1)
		return dset_err(session, table[generic].message);
	/* Fall back to the core table */
	if (table != core_errcode_table)
	{
		table = core_errcode_table;
		goto retry;
	}
	if (errcode < DSET_ERR_PRIVATE)
		return dset_err(session, "Kernel error received: %s",
						strerror(errcode));
	else
		return dset_err(session,
						"Undecoded error %u received from kernel",
						errcode);
}
