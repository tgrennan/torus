## build and install

This describes how to build and install the Linux kernel torus module along
with its associated dynamic library extension to the `ip` command; both
contained in this directory. Someday, this or something like it may be
integrated with the respective Linux kernel and `iproute2` project, but for
now, they're standalone.

This currently only supports a manual build but will eventually include debian
packaging so that this may be built and installed by the kernel update trigger.

### Requirements

This requires a properly configured kernel source tree.
Many distributions provide this in the `linux-source` package.
This also requires the `iproute2` package source which may be obtained with,

````console
$ apt-get source iproute
````

or,

````console
$ git clone git://git.kernel.org/pub/scm/linux/kernel/git/shemminger/iproute2.git
````

### Build

This will build the `torus.ko` module for the host Linux kernel along with the
`ip` command with the torus dynamic link library.

````console
$ make
````

#### Optional Parameters

These may be set on the command line or defined in the file named `config`.

* `TOPDIR` is an alternate, configured kernel source directory.
  The default is the host `linux-source` package.
* Use `V=1` to see full commands.
* Use `CONFIG_DEBUG_INFO=y` to include GDB info.
* `IPROUTE2DIR`, the default search path is `../iproute2:./iproute2`

#### Configuration Parameters

These/This may be configured through `make` command arguments
(i.e. CONFIG_TORUS_...) or defined within `config.h`.

* `CONFIG_TORUS_MSG_LVL`
  The default is silence; use 7 for full debug

### Check Headers

Use this to check that all header files self compile.

````console
$ make check-headers
````

### Install

````console
$ make modules_install
````
### Load installed module

````console
$ sudo modprobe torus
````

### Load an uninstalled module.

````console
$ sudo insmod torus.ko
````

### Unload module

````console
$ sudo rmmod torus
````
