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

ip () {
	PATH=.:$PATH command ip $@
}

prog=${0##*/}
op=usage
net="fd4d:ead4:3895:c141"

while [ $# -gt 0 ] ; do
	case "$1" in
		-h | --help)
			break
			;;
		-n | --dry-run)
			ip () {
				echo ip $*
			}
			;;
		--net )	net=$2
			shift 2
			;;
		show | start | stop | store )
			op=$1; shift
			break
			;;
		*)	break
			;;
	esac
	shift
done

show () {	# show [NETNS/]DEVICE ATTRIBUTE[:INDEX]
	[ $# -lt 1 ] && usage Error: missing [NETNS/]DEVICE
	[ $# -lt 2 ] && usage Error: missing ATTRIBUTE
	netns=$(netns $1)
	dev=$(dev $1)
	attr=${2%:*}
	idx=${2#*:}
	sysfile=/sys/devices/virtual/net/$dev/$attr
	if [ "$idx" = "$attr" ] ; then
		eval $netns cat $sysfile
	else
		let idx++
		eval $netns sed -n ${idx}p $sysfile
	fi
}

start () {	# start [name NAME ][ MASTER ][ ROWSxCOLS ]
	if [ "$1" = "name" ] ; then
		name="name $2"
		shift 2
	fi
	ip link add ${name} type torus $@
	shopt -s nullglob
	declare -a full_te=( /sys/devices/virtual/net/te* )
	declare -a lladdr
	for te in ${full_te[@]##*/} ;  do
		ip netns add $te
		ip link set dev $te netns $te
		ip netns exec $te \
			sysctl -q -w net.ipv6.conf.${te}.forwarding=1
		lladdr=( $(ip netns exec $te \
			   cat /sys/devices/virtual/net/${te}/address \
			   | tr : ' ') )
		prefix=${net}:
		prefix+=:${lladdr[0]}${lladdr[1]}
		prefix+=:${lladdr[2]}${lladdr[3]}
		prefix+=:${lladdr[4]}${lladdr[5]}
		prefix+=/64
		ip netns exec $te \
			ip -6 addr add $prefix dev $te
		ip netns exec $te \
			ip -6 route add ${net}::/64 dev $te
	done
	for te in ${full_te[@]##*/} ;  do
		ip netns exec $te \
			ip link set dev $te up
	done
}

stop () {	# stop [ PATTERN ]
	declare -a full_te=( /var/run/netns/${1:-te*} )
	for te in ${full_te[@]##*/} ;  do
		ip netns del $te
	done
}

store () {	# store [NETNS/]DEVICE ATTRIBUTE[:INDEX] VALUE
	[ $# -lt 1 ] && usage Error: missing [NETNS/]DEVICE
	[ $# -lt 2 ] && usage Error: missing ATTRIBUTE
	[ $# -lt 3 ] && usage Error: missing VALUE
	netns=$(netns $1)
	dev=$(dev $1)
	attr=${2%:*}
	idx=${2#*:}
	sysfile=/sys/devices/virtual/net/$dev/$attr
	val=$3
	if [ "$idx" = "$attr" ] ; then
		if [ "$val" = "-" ] ; then
			eval $netns tee $sysfile >/dev/null
		else
			echo "$val" | eval $netns tee $sysfile >/dev/null
		fi
	else
		tmp=/tmp/${prog}$$
		trap "rm $tmp" EXIT
		eval $netns sed "${idx}c${val}" $sysfile > $tmp &&
			cat $tmp | eval $netns tee $sysfile >/dev/null
	fi
}

usage () {
	if [ $# -gt 0 ] ; then
		exec >&2
		echo Error: $@
		trap 'exit 1' RETURN
	fi
	cat <<-EOF
	Usage:	$prog [ --dry-run ] show DEVICE ATTRIBUTE[:ENTRY]
	...	$prog [ --dry-run ] start [ name NAME ][ MASTER ][ ROWSxCOLS ]
	...	$prog [ --dry-run ] stop [ PATTERN ]
	...	$prog [ --dry-run ] store DEVICE ATTRIBUTE[:ENTRY] < VALUE | - >
	EOF
}

netns () {	# netns [NETNS/]DEVICE
	netns=${1%/*}
	if [  -d /var/run/netns/$netns ] ; then
		echo ip netns exec $netns
	fi
}

dev () {	# dev [NETNS/]DEVICE
	dev=${1#*/}
	if [ -z "$dev" ] ; then
		echo ${1%/*}
	else
		echo $dev
	fi
}

eval $op $@

