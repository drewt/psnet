objects = client.o deltalist.o ini.o jsmn.o misc.o network.o parse.o \
	  protocol.o server.o
targets = psnet-common.a
clean = $(objects) $(targets)

all: $(targets)

include $(topdir)/rules.mk

psnet-common.a: $(objects)
	$(call cmd,ar)
