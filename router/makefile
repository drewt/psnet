objects = msgcache.o router.o service.o
targets = psrouted
clean = $(objects) $(targets)

all: $(targets)

include $(topdir)/rules.mk

psrouted: $(objects)
	$(call cmd,ld,$(LIBS))

README: $(docdir)/psrouted
	$(call cmd,groff)
