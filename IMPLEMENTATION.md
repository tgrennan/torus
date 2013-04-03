## Implementation musings.

This is a virtual ethernet interface that implements a torus node from two or
more user assigned physical interfaces that have point-to-point connections to
adjacent nodes.  It now makes forwarding decisions from an encoded destination
MAC address; someday this may be extended to MPLS, LISP or other encapsulation
headers too.  To, facilitate testing, you may have this device recursively
clone itself to create a two dimensional toroidal network.  Similarly, you may
make a torus node become a port of another torus and use it for virtual
hosting, inter-toroid routing; or simulate multi-dimensional networks.  You may
assign up to 255 devices to a torus node.

We linked a dynamic library with the `ip` command of the
[iproute2](http://www.linuxfoundation.org/collaborate/workgroups/networking/iproute2)
package to add and delete torus interfaces to like this:

```console
./ip link add type torus [ MASTER ][ ROWSxCOLS ]
./ip link del DEV
```

We also include [torus.sh](examples/torus.sh) to show how the `ip` command may
be used to create and manage torus nodes within network name-spaces.  Look
[here](USAGE.md) for examples.

Someday we will rewrite this script as another C program leveraging
`libnetlink` from `iproute2` to replace an expand the current `sysfs` attribute
interface.  We'll also add companion daemons for torus address distribution and
integration or reproduction of IGP routers.

#### TODO
- [ ] IP[v6] lookup of host sourced packets
- [ ] netlink interface to lookup tables
- [ ] node announcement and discovery protocol (kernel thread?)
- [ ] respond to host/router discovery
- [ ] torus address/label distribution
- [ ] some SPF IGP
