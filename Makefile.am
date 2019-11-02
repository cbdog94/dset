## Process this file with automake to produce Makefile.in

ACLOCAL_AMFLAGS = -I m4

include $(top_srcdir)/Make_global.am

if ! WITH_KBUILDDIR
KBUILD_OUTPUT=/lib/modules/`uname -r`/build
else
KBUILD_OUTPUT=$(KBUILDDIR)
endif
if ! WITH_MAXSETS
DOMAIN_SET_MAX=256
else
DOMAIN_SET_MAX=$(MAXSETS)
endif

SUBDIRS		= include/libdset lib src
INSTALL_MOD_PATH = /

sparse:
	$(MAKE) -C lib sparse-check
	$(MAKE) -C src sparse-check

modules_sparse:
if WITH_KMOD
	${MAKE} -C $(KBUILD_OUTPUT) M=$$PWD/kernel/net \
			V=$V W=1 C=2 CF="-D__CHECK_ENDIAN__ -Wsparse-all" \
			KCFLAGS="-DPACKAGE_VERSION=$(PACKAGE_VERSION) -DCHECK_KCONFIG -Wextra" \
			DOMAIN_SET_MAX=$(DOMAIN_SET_MAX) KDIR=$$PWD/kernel modules
else
	@echo Skipping kernel modules due to --with-kmod=no
endif

modules:
if WITH_KMOD
	${MAKE} -C $(KBUILD_OUTPUT) M=$$PWD/kernel/net V=$V \
			KCFLAGS="-DPACKAGE_VERSION=$(PACKAGE_VERSION)" \
			DOMAIN_SET_MAX=$(DOMAIN_SET_MAX) KDIR=$$PWD/kernel modules
else
	@echo Skipping kernel modules due to --with-kmod=no
endif

define DEPMOD_WARNING

!!! WARNING !!! WARNING !!! WARNING !!!

Your distribution seems to ignore the /lib/modules/<kernelrelease>/extra/
subdirectory, where the dset kernel modules are installed.

Add the 'extra' directory to the search definition of your depmod
configuration (/etc/depmod.conf or /etc/depmod.d/) and re-run

	depmod <kernelrelease>

otherwise the dset kernel modules in the extra subdir will be ignored.

endef
export DEPMOD_WARNING

define MODULE_WARNING

!!! WARNING !!! WARNING !!! WARNING !!!

Your running system has got dset related kernel modules loaded in.
You MUST rmmod the domain_set* kernel modules in order to make possible
for the system to load in the newly installed ones.
endef
export MODULE_WARNING

modules_install:
if WITH_KMOD
	${MAKE} -C $(KBUILD_OUTPUT) M=$$PWD/kernel/net \
			KDIR=$$PWD/kernel modules_install
	@modinfo -b ${INSTALL_MOD_PATH} domain_set_hash_domain | ${GREP} /extra/ >/dev/null || echo "$$DEPMOD_WARNING"
	@lsmod | ${GREP} '^domain_set' >/dev/null && echo "$$MODULE_WARNING"
else
	@echo Skipping kernel modules due to --with-kmod=no
endif

modules_clean:
if WITH_KMOD
	${MAKE} -C $(KBUILD_OUTPUT) M=$$PWD/kernel/net \
			KDIR=$$PWD/kernel clean
else
	@echo Skipping kernel modules due to --with-kmod=no
endif

update_includes:
	for x in domain_set.h domain_set_hash.h; do \
	    sed -r -e 's@#(ifndef|define|endif[ \t]*/[*])[ \t]*_UAPI@#\1 @' \
		   -e 's@^#include <linux/netfilter/dset/domain_set.h>@@' \
		kernel/include/uapi/linux/netfilter/dset/$$x \
		> include/libdset/linux_$$x; \
	done

check_libmap:
	@./check_libmap.sh

tests:
	cd tests; ./runtest.sh

cleanup_dirs := . include/libdset lib src tests

tidy: distclean modules_clean
	rm -rf .deps $(foreach dir,$(cleanup_dirs),$(wildcard $(dir)/*~))
	rm -rf aclocal.m4 autom4te.cache 
	rm -rf config.* configure compile depcomp install-sh libtool ltmain.sh
	rm -rf Makefile Makefile.in lib/Makefile lib/Makefile.in src/Makefile src/Makefile.in
	rm -rf missing stamp-h1 m4/libtool.m4 m4/lt*.m4
	rm -rf lib/dset_settype_check lib/types_init.c
	find . -name '*~' -delete

tarball:
	rm -Rf /tmp/dset-${PACKAGE_VERSION};
	pushd ${top_srcdir} && git archive --prefix=dset-${PACKAGE_VERSION}/ HEAD | tar -C /tmp -x && popd;
	pushd /tmp/dset-${PACKAGE_VERSION} && ./autogen.sh && popd;
	tar -C /tmp -cjf dset-${PACKAGE_VERSION}.tar.bz2 --owner=root --group=root dset-${PACKAGE_VERSION}/;
	rm -Rf /tmp/dset-${PACKAGE_VERSION};

help:
	@echo '  <nothing>              - Compile userspace'
	@echo '  modules                - Compile kernel modules'
	@echo '  install                - Install userspace'
	@echo '  modules_install        - Install kernel modules'
	@echo '  clean                  - Remove generated userspace files'
	@echo '  modules_clean          - Remove generated kernelspace files'
	@echo '  tidy                   - Tidy up the whole source tree'
	@echo '  tests                  - Run testsuite'
	@echo '  sparse                 - Check userspace with sparse'
	@echo '  modules_sparse         - Check kernelspace with sparse'
	@echo '  update_includes        - Update userspace include files'
	@echo '  check_libmap           - Check libdset.map for missing symbols'
	@echo '  tarball                - Create a tarball for a new release'

.PHONY: modules modules_instal modules_clean update_includes tests tarball

DISTCHECK_CONFIGURE_FLAGS = --with-kmod=no
