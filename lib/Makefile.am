include $(top_srcdir)/Make_global.am

DSET_SETTYPE_LIST = \
	dset_hash_domain.c

AM_CFLAGS += ${libmnl_CFLAGS}

lib_LTLIBRARIES = libdset.la

include $(top_srcdir)/lib/Make_extra.am

libdset_la_LDFLAGS = -Wl,--version-script=$(top_srcdir)/lib/libdset.map -version-info $(LIBVERSION)
libdset_la_LIBADD  = ${libmnl_LIBS} $(DSET_SETTYPE_STATIC_OBJECTS) $(LIBADD_DLOPEN)
libdset_la_SOURCES = \
	args.c \
	data.c \
	errcode.c \
	list_sort.c \
	mnl.c \
	parse.c \
	print.c \
	session.c \
	types.c \
	dset.c \
	types_init.c

EXTRA_libdset_la_SOURCES = \
	debug.c

EXTRA_DIST = $(DSET_SETTYPE_LIST) libdset.map

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libdset.pc

dist_man_MANS = libdset.3

sparse-check: $(libdset_la_SOURCES:.c=.d)

%.d: %.c
	$(DSET_AM_V_CHECK)\
	$(SPARSE) $(SPARSE_FLAGS) $(AM_CFLAGS) $(AM_CPPFLAGS) $< || :
