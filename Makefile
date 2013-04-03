#!/usr/bin/make -f
#
# To put more focus on warnings, this is less verbose by default.
# Use 'make V=1' to see the full commands
#
# Copyright (C) 2012, 2013 Tom Grennan and Eliot Dresselhaus
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License along
#   with this program; if not, write to the Free Software Foundation, Inc.,
#   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

ifneq ($(KERNELRELEASE),)
include Kbuild
else
-include config

ifeq (,$(IPROUTE2DIR))
ifneq (,$(wildcard ../iproute2/Config))
	IPROUTE2DIR=$(addsuffix iproute2,$(dir $(CURDIR)))
else
	IPROUTE2DIR=$(CURDIR)/iproute2
endif
endif

mod	:= $(notdir $(CURDIR))
ko	:= $(mod).ko
uname_r	:= $(shell uname -r)
uname_m	:= $(shell uname -m)
TOPDIR	?= /usr/src/linux-headers-$(uname_r)

ifeq (,$(wildcard $(TOPDIR)/include/generated/autoconf.h))
 $(error Inaccessible or improperly configured kernel tree.)
endif

# mod-defines:	list the module configuration options (i.e.: CONFIG_TORUS_...)
#		If any are set on the command line (i.e. `make CONFIG_TORUS_...)
#		Kbuild will append to cflags-y (-DCONFIG_TORUS_...)
#		Alternatively, one may set these in `config.h`
mod-defines = CONFIG_TORUS_MSG_LVL
mod-options = V TOPDIR CONFIG_DEBUG_INFO $(mod-defines)

command-has-opt	= $(findstring command line,$(origin $(opt)))
config-has-opt	= $(findstring file,$(origin $(opt)))
kbuild-opt	= $(eval KO_BUILD+= $(opt)="$(value $(opt))")

KO_BUILD = $(MAKE) --no-print-directory
KO_BUILD+= -C "$(TOPDIR)"
KO_BUILD+= M="$(CURDIR)"
KO_BUILD+= DEFINES="$(mod-defines)"
$(foreach opt,$(mod-options),$(if $(config-has-opt),$(kbuild-opt)))
$(foreach opt,$(mod-options),$(if $(command-has-opt),$(kbuild-opt)))

.PHONY: all
all:	$(ko) ip

modules $(ko):
	@$(RM) $(ko)
	@$(KO_BUILD) modules

modules_install: $(ko)
	@$(KO_BUILD) modules_install

IP_BUILD = $(MAKE)
IP_BUILD+= -C $(IPROUTE2DIR)
IP_BUILD+= TORUSDIR=$(CURDIR)

define IPROUTE2_CONFIG_PATCH
ifneq (,$$(TORUSDIR))
	IPOBJ+=$$(TORUSDIR)/iplink_torus.o
	CFLAGS+=-I $$(CURDIR) -I $$(CURDIR)/include -I $$(TORUSDIR)
endif
endef

ifeq (,$(shell grep TORUSDIR $(IPROUTE2DIR)/Config))
iproute2-config-patch:	export IPROUTE2_CONFIG_PATCH:=$(IPROUTE2_CONFIG_PATCH)
iproute2-config-patch:	$(IPROUTE2DIR)/Config
	@echo "$${IPROUTE2_CONFIG_PATCH}" >> $(IPROUTE2DIR)/Config
ip:	iproute2-config-patch
endif

ip:	$(IPROUTE2DIR)/ip/ip
	@ln -sf $< $@ && echo "  LN $< $(CURDIR)/$@"

$(IPROUTE2DIR)/ip/ip:	iplink_torus.o
	@echo "  LD $@"

iplink_torus.o:	iplink_torus.c linux/torus.h
	@$(IP_BUILD)

rel-h-to-test-c = $(subst .h,.c,$(addprefix test_header_,$(subst /,__,$(1))))
test-c-to-rel-h	= $(subst test_header_,,$(subst __,/,$(subst .c,.h,$(1))))

rel-h	:= $(wildcard *.h linux/*.h)
test-h-c = $(call rel-h-to-test-c,$(rel-h))
test-h-o = $(subst .c,.o,$(test-h-c))

check-headers: $(test-h-o)

test_header_%.o: test_header_%.c
	@$(KO_BUILD) $@

test_header_%.c:
	@echo "#include <$(call test-c-to-rel-h,$@)>" >$@

define test-h-c-dep
$(1) : $(call test-c-to-rel-h,$(1))
endef

$(foreach t,$(test-h-c),$(eval $(call test-h-c-dep,$t)))

clean:
	@$(KO_BUILD) clean
	@rm -f test_header_*

endif

# Use this to show a variable, make KERNELRELEASE=1 show-VAR
show-% :
	@$(eval $$(info $* is $$($*))) :
