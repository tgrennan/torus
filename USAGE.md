## usage

This requires the patched
[iproute2](http://www.linuxfoundation.org/collaborate/workgroups/networking/iproute2)
package available [here](https://github.com/tgrennan/iproute2).

Create and destroy virtual toroids like this.

```console
ip link add type torus toroid 0
ip link del te0
```

[Here](examples/toroid.sh) is an example script that reproduces the above commands.

### FIXME
With the rest.
