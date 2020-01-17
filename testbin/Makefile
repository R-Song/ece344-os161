#
# Makefile for src/testbin (sources for programs installed in /testbin)
#

include ../defs.mk

# specify exclusion here
EXCLUDES:=

PROGS:=$(filter-out $(EXCLUDES), $(subst /,,$(wildcard */)))
PROGS_ALL:=$(addsuffix .all,$(PROGS))
PROGS_DEPEND:=$(addsuffix .depend,$(PROGS))
PROGS_TAGS:=$(addsuffix .tags,$(PROGS))
PROGS_CLEAN:=$(addsuffix .clean,$(PROGS))
PROGS_INSTALL:=$(addsuffix .install,$(PROGS))

all: $(PROGS_ALL)
tags: $(PROGS_TAGS)
depend: $(PROGS_DEPEND)
clean: $(PROGS_CLEAN)
install: $(PROGS_INSTALL)

$(PROGS_ALL) $(PROGS_TAGS) $(PROGS_DEPEND) $(PROGS_CLEAN) $(PROGS_INSTALL):
	cd $(basename $@) && $(MAKE) $(subst .,,$(suffix $@))
	
.PHONY: all depend tags clean install $(PROGS_ALL) $(PROGS_DEPEND) \
	$(PROGS_TAGS) $(PROGS_CLEAN) $(PROGS_INSTALL)

