## usage

This requires the patched
[iproute2](http://www.linuxfoundation.org/collaborate/workgroups/networking/iproute2)
package available [here](https://github.com/tgrennan/iproute2).

Create and destroy virtual toroids like this.

```console
ip link add type torus toroid 0
ip link del te0
```

The following example script that is a reference for what is done through the
virtual toroid device.

```bash
#!/bin/bash

# create virtual toroid

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
```

### FIXME
With the rest.
