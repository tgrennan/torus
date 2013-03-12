#!/bin/bash
#
# torus.sh - show or modify torus sysfs attributes
#
# This example script shows simple means to modify a node lookup
# table in lieu of a dynamic link state daemon or thread.
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

prog=${0##*/}
op=usage
while [ $# -gt 0 ] ; do
	case "$1" in
		-h | --help)
			op=usage
			;;
		-n | --dry-run)
			ip () {
				echo ip $*
			}
			;;
		te*)	declare -r dev=$1
			;;
		*)	break
			;;
	esac
	shift
done

if [ -n "$dev" ] ; then
	if [ $# -ge 1 ] ; then
		attr=${1%:*}
		idx=${1#*:}
		if [ $# -eq 2 ] ; then
			val=$2
			op=store
		else
			op=show
		fi
		if [ "$idx" = "$attr" ] ; then
			unset idx
		else
			let idx++
			op+=-idx
		fi
		[[ "$attr" == +(nodelu|portlu) ]] && attr+=0
		sysfile=/sys/devices/virtual/net/$dev/$attr
	fi
fi

show-idx () {
	sed -n "${idx}p" $sysfile
}

show () {
	cat $sysfile
}

store-idx () {
	tmp=/tmp/${prog}$$
	trap "rm $tmp" EXIT
	sed "${idx}c${val}" $sysfile > $tmp && cat $tmp > $sysfile
}

store () {
	if [ "$val" = "-" ] ; then
		cat >$sysfile
	else
		echo "$val" >$sysfile
	fi
}

usage () {
	cat <<-EOF
	Usage: $prog [-n|--dry-run] ATTRIBUTE[:ENTRY] [VALUE|-]
	EOF
}

eval $op

