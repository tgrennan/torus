## usage

This requires the patched
[iproute2](http://www.linuxfoundation.org/collaborate/workgroups/networking/iproute2)
package available [here](https://github.com/tgrennan/iproute).

### 2x2

Here is an example of a single toroid with 2 rows of 2 nodes interconnected
with vitual ehternet pairs.

First create the paired virtual ethernet devices.

````console
$ sudo ip link add type veth
$ sudo ip link add type veth
$ sudo ip link add type veth
$ sudo ip link add type veth
$ sudo ip link add type veth
$ sudo ip link add type veth
$ sudo ip link add type veth
$ sudo ip link add type veth
````
Now create the nodes with the `veth` ports,

````console
$ sudo ip link add type torus node 0.0%2 veth0 veth2 veth4 veth6
$ sudo ip link add type torus node 0.1%2 veth8 veth7 veth10 veth3
$ sudo ip link add type torus node 0.2%2 veth5 veth12 veth1 veth14
$ sudo ip link add type torus node 0.3%2 veth11 veth15 veth9 veth13
````

FIXME with the rest.
