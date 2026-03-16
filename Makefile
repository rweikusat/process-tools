# process-tools Makefile
#

#*  definitions
#**  programs
#
GCC :=		gcc
CC :=		$(GCC) -Iinclude
LD :=		$(GCC)
INST_D :=	install -m 0644
INST_X :=	install -m 0755

#**  files
#
SRCS :=		$(shell ls src/*.c)
OBJS :=		$(addprefix tmp/, $(notdir $(SRCS:.c=.o)))
DEPS :=		$(OBJS:.o=.d)

PRGS :=		$(addprefix bin/, clfds launch)

#**  CFLAGS
#
CFLAGS :=	-g -W -Wall -Wno-pointer-sign -Wno-implicit-fallthrough

ifndef DEV
CFLAGS :=	-O2 $(CFLAGS)
endif

#**  installation
#
TARGET :=	$(DESTDIR)/usr
TARGET_BIN :=	$(TARGET)/bin

#**  lib version
#

#*  targets
#
.PHONY: all clean install deb

all: $(PRGS)

deb:
	fakeroot debian/rules binary

clean:
	-rm tmp/*.o tmp/*.d
	-rm bin/*

include $(DEPS)

#*  %-rules
#
tmp/%.d: src/%.c
	$(CC) -MM $< | perl -pe 's|$*\.o|$@|' >$@

tmp/%.o: src/%.c tmp/%.d
	$(CC) $(CFLAGS) -c -o $@ $<

bin/%: tmp/%.o
	$(LD) -o $@ $^
