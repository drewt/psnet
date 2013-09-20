objects = service.o
targets = pstrackd
clean = $(objects) $(targets)

all: $(targets)

include $(topdir)/rules.mk

pstrackd: $(objects)
	$(call cmd,ld,$(LIBS))

README: $(docdir)/pstrackd
	$(call cmd,groff)
