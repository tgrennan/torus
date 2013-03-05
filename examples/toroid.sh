#!/bin/bash

# toroid.sh - an example script
#
# This is a reference for what's done by `ip link add type torus toroid...`
# Unlike the virtual-toroid link device, this script doesn't track veths to
# torus nodes so it can't support multiple toroids or ignore other unrealted
# veths. So, don't use this for anything other than a reference.
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

declare -i rows=3
declare -i cols=3
op=create

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
		--delete)
			op=delete
			;;
		*x*)	rows=${1%x*}
			cols=${1#*x}
			;;
		*)	break;;
	esac
	shift
done

if [ $# -ne 1 ] ; then
	op=usage
else
	declare -r toroid=$1; shift
fi

declare -r -i nodes=$(( rows * cols))
declare -i node node neighbor veth

usage () {
	cat <<-EOF
	Usage: $prog [-n|--dry-run] [--delete] [ROWSxCOLS] TOROID
	EOF
}

create () {
	# create rows*cols nodes plus 2 pairs of veth devices
	for ((node=0; node<nodes; node++)) ; do
		ip link add torus node $toroid.$node
		ip link add veth
		ip link add veth
	done
	# assign veth pairs to north-south, east-west nodes
	for ((node=0; node<nodes; node++)) ; do
		veth=$(( node * 2 ))
		ip link set dev veth$veth master te$toroid.$node
		neighbor=$(( (node + cols) % nodes ))
		ip link set dev veth$(( veth + 1 )) master te$toroid.$neighbor
		veth=$(( veth + (nodes * 2) ))
		ip link set dev veth$veth master te$toroid.$node
		neighbor=$(( node + 1 ))
		if [[ $(( neighbor % cols )) -eq 0 ]] ; then
			neighbor=$(( neighbor - cols ))
		fi
		ip link set dev veth$(( veth + 1 )) master te$toroid.$neighbor
	done
}

destroy () {
	for ((node=0; node<nodes; node++)) ; do
		ip link del te${toroid}.${node}
	done
	for ((veth=0; veth < nodes * 2; veth++)) ; do
		ip link del veth$veth
	done
}

eval $op
