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
	monitor \
)

PRGS :=			$(D_PRGS) $(addprefix bin/, \
	ch-dir \
	monitor-ctrl \
	sane-env \
)

PODS := $(shell ls doc/*.pod)
MANS := $(PODS:.pod=.1)

TARGET :=	$(DESTDIR)/usr
TARGET_BIN :=	$(TARGET)/bin

#**  CFLAGS
#
CFLAGS := \
	-W \
	-Wall \
	-Wno-implicit-fallthrough \
	-Wno-pointer-sign \
	-Wno-sign-compare \
	-Wno-unused-result \
	-fno-inline-functions \
	-fno-inline-functions-called-once \
	-fno-inline-small-functions \
	-g

ifndef DEV
CFLAGS :=	-O2 $(CFLAGS)
endif

#**  installation
#
TARGET :=	$(DESTDIR)/usr
TARGET_BIN :=	$(TARGET)/bin

#**  functions
#
man-ver = $(shell sed -n 's/^\([^ ]* [^ ]*\) .*/\1/g; s/[()]//g; p; q' debian/changelog)

#*  targets
#
.PHONY: all clean install deb
.PRECIOUS: tmp/diag.o

all: $(PRGS) $(MANS)

deb:
	fakeroot debian/rules binary

install:
	$(INST_X) -d $(TARGET_BIN)
	$(INST_X) $(PRGS) $(TARGET_BIN)

clean:
	-rm tmp/*.o tmp/*.d
	-rm bin/*
	-rm $(MANS)

bin/ch-dir: tmp/ch_dir.o
bin/monitor-ctrl: tmp/monitor_ctrl.o
bin/sane-env: tmp/sane_env.o

$(D_PRGS) : bin/% : tmp/%.o

include $(DEPS)

#*  %-rules
#
tmp/%.d: src/%.c
	$(CC) -MM $< | perl -pe 's|$*\.o|$@|' >$@

tmp/%.o: src/%.c tmp/%.d
	$(CC) $(CFLAGS) -c -o $@ $<

bin/%: tmp/diag.o
	$(LD) -o $@ $^

doc/%.1: doc/%.pod debian/changelog
	pod2man -c 'User Commands' -r "$(call man-ver)" $< >$@
