#!/usr/bin/make -f
#
# KBUILD for the Linux torus module.
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

command-has-def	= $(findstring command line,$(origin $(def)))
ccflags-def	= $(eval ccflags-y += -D$(def)="$(value $(def))")

obj-m	:= torus.o
torus-y	:= mod.o rtnl.o netdev.o ethtool.o sysfs.o

ccflags-y := -I$(src) -Werror
ifneq (,$(wildcard $(src)/config.h))
	ccflags-y += -include config.h
$(foreach test_header_c,$(wildcard test_header_*.c),\
	$(subst .c,,$(test_header_c))-y: -include config.h)
endif
$(foreach def,$(DEFINES),$(if $(command-has-def),$(ccflags-def)))

extra-y += iplink_torus.o
extra-y += ip
extra-y	+= $(subst .c,.o,$(wildcard test_header_*.c))
