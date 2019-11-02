Domain set
============

[![Latest release][release_badge]][release_url]

Introduction
----
This is the domain set source tree. It mantained a hash stucture to store domain name for efficiently matching the domain. Now it was integrated to [ebtables](https://github.com/cbdog94/ebtables).

Prerequisite
----
The original Linux kernel does not support domain set due to a new socket identifier needed in `nfnetlink.h`. Therefore, we have modified the [Linux kernel](https://github.com/cbdog94/linux).

Besides, the dpset source code depends on the libmnl library so the library must be installed. You can download the libmnl library from
```
git://git.netfilter.org/libmnl.git
```
For Ubuntu, you can use
```shell
sudo apt install libmnl-dev
```
Install
----
1. Initialize the compiling environment for dset. The packages automake, autoconf, pkg-config and libtool are required.
```shell
sudo apt install automake autoconf pkg-config libtool-bin
./autogen.sh
```

2. Run `./configure` and then compile the dset binary and the kernel modules. Configure parameters can be used to to override the default path to the kernel source tree (/lib/modules/`uname -r`/build), the maximum number of sets (256), the default hash sizes (1024). See `./configure --help`.
```shell
./configure
make
make modules
```

3. Install the binary and the kernel modules
```shell
sudo make install
sudo make modules_install
```

4. Cleanup the source tree
```shell
make clean
make modules_clean
```
That's it! 

Usage
----
Read the dset(8) and ebtables(8) manpages on how to use dset and its match and target from ebtables.

 [release_badge]: https://img.shields.io/github/release/cbdog94/dset.svg
 [release_url]: https://github.com/cbdog94/dset/releases/latest