## Implementation musings.

This module provides these four types of network interfaces:

*   The `node` is a pseudo-ethernet device that is given mastership of two of
    more network interfaces to run the torus protocol; much like the `bridge`,
    `team`, and `bonding` devices, 

*   The `clone` interface is a client of the given `node` used in virtual
    hosting.

*   A `toroid` is a non-forwarding device used to easily manage test networks.

*   The `port` interface mimics the virtual ethernet device (`veth`).
    These are indirectly managed by `toroid` interfaces.

We modified the `ip` (`iproute2`) command to add and delete torus interfaces
like this:

```console
ip link add type torus node TORUS
ip link add type clone DEVICE [CLONE]
ip link add type toroid TOROID [ROWSxCOLS[xCLONES]]
ip link del DEV
```

You also use `ip` to [de-]assign interfaces to nodes like this:

```console
ip link set dev DEV master NODE_DEV
```

#### TODO
- [ ] forwarding
- [ ] `netlink` interface to lookup tables
- [ ] label routes with torus addresses
- [ ] node announcement and discovery protocol (kernel thread?)
