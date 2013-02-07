## build and install

This describes how to build and install the Linux kernel torus module contained
in this directory. Someday, this or something like it may be integrated with
the Linux kernel project, but for now, it's standalone.

This currently only supports a manual build but will eventually include debian
packaging so that this may be built and installed by the kernel update trigger.

### Build

This will build the `torus.ko` module for the host Linux kernel.

````console
$ make
````

#### Optional Parameters

These may be set on the command line or defined in the file named `config`.

* `TOPDIR` is an alternate, configured kernel source directory.
  The default is the host `linux-source` package.
* Use `V=1` to see full commands.
* Use `CONFIG_DEBUG_INFO=y` to include GDB info.

> **Note**, this requires a properly configured kernel source tree.
> Many distributions provide this in the `linux-source` package.

### Check Headers

Use this to check that all header files self compile.

````console
$ make check_headers
````

### Install

````console
$ make modules_install
````
### Load installed module

````console
$ sudo modprobe torus
````

### Load uninstalled module

````console
$ sudo insmod torus.ko
````

### Unload module

````console
$ sudo rmmod torus
````
