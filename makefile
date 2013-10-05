# Don't use make's built-in rules or variables, and don't print directory info
MAKEFLAGS += -rR --no-print-directory

topdir = $(CURDIR)
export topdir

# install directories
prefix  = /usr/local
bindir  = $(prefix)/bin
man1dir = $(prefix)/share/man/man1
man5dir = $(prefix)/share/man/man5
man7dir = $(prefix)/share/man/man7

man1ext = .1
man5ext = .5
man7ext = .7

# project directories
docdir = $(topdir)/doc
incdir = $(topdir)/include

export docdir incdir

AR        = ar
ARFLAGS   = rcs
CC        = gcc
CFLAGS    = -Wall -Wextra -Werror -Wno-unused-parameter -std=gnu99 -g
ALLCFLAGS = -I $(incdir) $(CFLAGS)
CPPFLAGS  = -DPSNETLOG -D_Noreturn=__attribute\(\(noreturn\)\)
LD        = gcc
LDFLAGS   = -pthread
INSTALL   = install -o 0 -g 0 -D

LIBS = $(topdir)/common/psnet-common.a

export AR ARFLAGS CC CFLAGS ALLCFLAGS CPPFLAGS LD LDFLAGS
export LIBS

# sub-makes
tracker = tracker
router  = router
common  = common
submakes = $(tracker) $(router) $(common)

all: $(submakes)

include rules.mk

# install(src,dst,opts)
cmd_install = @$(if $(quiet),echo '  INSTALL $(2)' &&) $(INSTALL) $(3) $(1) $(2)

do_submake = +@$(if $(quiet),echo '  MAKE    $@' &&) cd $@ && $(MAKE)

$(tracker): $(common)
	$(do_submake)

$(router): $(common)
	$(do_submake)

$(common):
	$(do_submake)

install: all
	$(call cmd_install, $(tracker)/pstrackd, $(bindir)/pstrackd)
	$(call cmd_install, $(docdir)/psnet_protocol, $(man7dir)/psnet_protocol$(man7ext), -m 0644)
	$(call cmd_install, $(docdir)/psnetrc, $(man5dir)/psnetrc$(man5ext), -m 0644)
	$(call cmd_install, $(docdir)/pstrackd, $(man1dir)/pstrackd$(man1ext), -m 0644)

clean: topclean
topclean:
	@cd $(common) && $(MAKE) clean
	@cd $(tracker) && $(MAKE) clean
	@cd $(router) && $(MAKE) clean

distclean: topdistclean
topdistclean:
	@cd $(common); $(MAKE) distclean
	@cd $(tracker); $(MAKE) distclean
	@cd $(router); $(MAKE) distclean

dirmem: $(tracker)/pstrackd
	valgrind --tool=memcheck --leak-check=yes --show-reachable=yes \
	    --num-callers=20 --track-fds=yes $(tracker)/pstrackd

.PHONY: $(submakes) topclean topdistclean
