## usage

This requires the rebuilt `ip` command of the
[iproute2](http://www.linuxfoundation.org/collaborate/workgroups/networking/iproute2)
package with our included dynamic library.

Create and destroy torus nodes like this.

```console
./ip link add type torus
./ip link del te0
```

>> **Note** you only need to use the modified `ip` to add torus interfaces.

Assign ports to nodes like this.

```console
ip link set dev eth0 master te0
ip link set dev eth1 master te0
ip link set dev eth2 master te0
ip link set dev eth3 master te0
```

Disassociate ports from torus nodes like this.

```console
ip link set dev eth0 nomaster
ip link set dev eth1 nomaster
ip link set dev eth2 nomaster
ip link set dev eth3 nomaster
```

You may use the `veth` driver to peer between nodes.

```console
ip link add type torus
ip link add type torus
ip link add type veth
ip link set dev veth0 master te0
ip link set dev veth1 master te1
```

Here is an example of starting, then cloning another node for a virtual
hosting; each running in it's own name-space.

```console
examples/torus.sh start
examples/torus.sh start te0
examples/torus.sh show te0.0 ports
```

This example makes nine, 3x3 networks interconnected with a tenth 3x3 network
with each torus node device moved to its own container.

```console
examples/torus.sh start 3x3
for full_te in /var/run/netns/te? ; do
	examples/torus.sh start ${full_te##*/} 3x3
done
```

Use something like this to assign IP addresses.

```console
for full_te in /var/run/netns/te* ; do
	te=${full_te##*/}
	ip netns exec $te ip addr add 10.0.0.$(( ${te#te} + 100)) peer \
		10.0.0.1/32 dev $te
done
```

And this to bring-up the devices.

```console
for te in $(./ip netns); do
	ip netns exec $te ip link set dev $te up
done
```

Or this to delete all torus devices and associated name spaces.

```console
examples/torus.sh stop
```

### FIXME
With the rest.
