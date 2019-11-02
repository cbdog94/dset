include $(top_srcdir)/Make_global.am

sbin_PROGRAMS	=dset
dset_SOURCES	= dset.c
dset_LDADD	= ../lib/libdset.la

if ENABLE_SETTYPE_MODULES
AM_LDFLAGS  = -shared
else
if ENABLE_STATIC
AM_LDFLAGS	= -static
endif
endif

dist_man_MANS = dset.8

sparse-check: $(dset_SOURCES:.c=.d)

%.d: %.c
	$(DSET_AM_V_CHECK)\
	$(SPARSE) -I.. $(SPARSE_FLAGS) $(AM_CFLAGS) $(AM_CPPFLAGS) $< || :
