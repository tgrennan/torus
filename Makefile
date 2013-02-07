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

mod	:= $(notdir $(CURDIR))
ko	:= $(mod).ko
uname_r	:= $(shell uname -r)
uname_m	:= $(shell uname -m)

TOPDIR	?= /usr/src/linux-headers-$(uname_r)
ifeq (,$(wildcard $(TOPDIR)/include/generated/autoconf.h))
 $(error Inaccessible or improperly configured kernel tree.)
endif

# mod-defines:	list the module configuration options (i.e.: ROWS COLMUNS)
#		If any are set on the command line (i.e. `make ROWS=7`),
#		Kbuild will append to cflags-y (-DROWS="7")
mod-defines =
mod-options = V TOPDIR CONFIG_DEBUG_INFO $(mod-defines)

command-has-opt	= $(findstring command line,$(origin $(opt)))
config-has-opt	= $(findstring file,$(origin $(opt)))
kbuild-opt	= $(eval KBUILD += $(opt)="$(value $(opt))")

KBUILD	 = $(MAKE) --no-print-directory -C "$(TOPDIR)" M="$(CURDIR)"
KBUILD	+= DEFINES="$(mod-defines)"
$(foreach opt,$(mod-options),$(if $(config-has-opt),$(kbuild-opt)))
$(foreach opt,$(mod-options),$(if $(command-has-opt),$(kbuild-opt)))

modules $(ko):
	@$(RM) -f $(ko)
	@$(KBUILD) modules

modules_install: $(ko)
	@$(KBUILD) modules_install

rel-h-to-test-c = $(subst .h,.c,$(addprefix test_header_,$(subst /,__,$(1))))
test-c-to-rel-h	= $(subst test_header_,,$(subst __,/,$(subst .c,.h,$(1))))

rel-h	:= $(wildcard *.h */*.h */*/*.h)
test-h-c = $(call rel-h-to-test-c,$(rel-h))
test-h-o = $(subst .c,.o,$(test-h-c))

check_headers: $(test-h-o)

test_header_%.o: test_header_%.c
	@$(KBUILD) $@

test_header_%.c:
	@echo "#include <$(call test-c-to-rel-h,$@)>" >$@

define test-h-c-dep
$(1) : $(call test-c-to-rel-h,$(1))
endef

$(foreach t,$(test-h-c),$(eval $(call test-h-c-dep,$t)))

clean:
	@$(KBUILD) clean
	@rm -f test_header_*

endif

# Use this to show a variable, make KERNELRELEASE=1 show-VAR
show-% :
	@$(eval $$(info $* is $$($*))) :
