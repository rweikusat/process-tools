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

#**  functions
#
man-ver = $(shell sed -n 's/^\([^ ]* [^ ]*\) .*/\1/g; s/[()]//g; p; q' debian/changelog)
pods = $(shell ls doc/man$(1)/*.pod)
mans = $(subst pod,$(1),$(call pods,$(1)))

#**  files
#
SRCS :=		$(shell ls src/*.c)
OBJS :=		$(addprefix tmp/, $(notdir $(SRCS:.c=.o)))
DEPS :=		$(OBJS:.o=.d)

D_PRGS :=		$(addprefix bin/, \
	chids \
	clfds \
	launch \
	lock \
	monitor \
	pause \
)

PRGS :=			$(D_PRGS) $(addprefix bin/, \
	ch-dir \
	monitor-ctrl \
	sane-env \
)

MANS1 :=	$(call mans,1)
MANS :=		$(MANS1)

TARGET :=	$(DESTDIR)/usr
TARGET_BIN :=	$(TARGET)/bin
TARGET_MAN :=	$(TARGET)/share/man
TARGET_MAN1 :=	$(TARGET)/share/man/man1

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


#*  targets
#
.PHONY: all clean install deb
.PRECIOUS: tmp/diag.o

all: $(PRGS) $(MANS)

deb:
	fakeroot debian/rules binary

install:
	$(INST_X) -d $(TARGET_BIN) $(TARGET_MAN1)
	$(INST_X) $(PRGS) $(TARGET_BIN)
	$(INST_D) $(MANS1) $(TARGET_MAN1)

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

doc/man1/%.1: doc/man1/%.pod debian/changelog
	pod2man -c 'User Commands' -r "$(call man-ver)" $< >$@
