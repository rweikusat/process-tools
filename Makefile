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

define hy-prg-dep
bin/$(1): tmp/$(subst -,_,$(1)).o
endef

#**  files
#
SRCS :=		$(shell ls src/*.c)
OBJS :=		$(addprefix tmp/, $(notdir $(SRCS:.c=.o)))
DEPS :=		$(OBJS:.o=.d)

# 'plain' programs, ie, without a hyphen in their name,
# depend on .o file with same names
#
PLAIN_PRGS := \
	chids \
	clfds \
	launch \
	lock \
	monitor \
	pause \
	syslogging

# hyphen programs, depend on .o file with - translated to _
#
HY_PRGS := \
	ch-dir \
	have-locks \
	monitor-ctrl \
	sane-env \
	u-talk

PRGS := $(addprefix bin/, $(PLAIN_PRGS) $(HY_PRGS))

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

$(addprefix bin/, $(PLAIN_PRGS)) : bin/% : tmp/%.o
$(foreach prg,$(HY_PRGS),$(eval $(call hy-prg-dep,$(prg))))

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
