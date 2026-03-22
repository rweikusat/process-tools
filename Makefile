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

D_PRGS :=		$(addprefix bin/, \
	chids \
	clfds \
	launch \
)

PRGS :=			$(D_PRGS) $(addprefix bin/, \
	ch-dir \
	sane-env \
)

#**  CFLAGS
#
CFLAGS := \
	-g \
	-W \
	-Wall \
	-Wno-pointer-sign \
	-Wno-implicit-fallthrough \
	-fno-inline-functions \
	-fno-inline-small-functions \
	-fno-inline-functions-called-once

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

bin/ch-dir: tmp/ch_dir.o
bin/sane-env: tmp/sane_env.o

$(D_PRGS) : bin/% : tmp/%.o

include $(DEPS)

#*  %-rules
#
tmp/%.d: src/%.c
	$(CC) -MM $< | perl -pe 's|$*\.o|$@|' >$@

tmp/%.o: src/%.c tmp/%.d
	$(CC) $(CFLAGS) -c -o $@ $<

bin/%:
	$(LD) -o $@ $^
