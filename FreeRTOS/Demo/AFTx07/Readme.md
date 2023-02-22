# Emulating generic RISC-V 32bit machine on QEMU

## Requirements
1. Set up Software in Socet Confluence.

https://wiki.itap.purdue.edu/pages/viewpage.action?pageId=247664452

2. GNU RISC-V toolchains.

It should be done in the previous step, if not download from here and add to PATH.
```
cd ~
wget https://static.dev.sifive.com/dev-tools/freedom-tools/v2020.08/riscv64-unknown-elf-gcc-10.1.0-2020.08.2-x86_64-linux-centos6.tar.gz
```

## How to build

Add path of toolchain that is described above section, such as:

```
$ export PATH="/YOUR_PATH/riscv64-unknown-elf/bin:${PATH}"
```

For release build:

```
$ make -C build/gcc/
```

For debug build:

**NOTE: not yet tested**

```
$ make -C build/gcc/ DEBUG=1
```

To clean build artifacts:

```
$ make -C build/gcc/ clean
```

If the build was successful, the RTOSDemo.bin will be located in the build/gcc/output directory and can be copy to main AFTx07 and rename to meminit.bin.



## How to run

**NOTE:** run from main AFTx07 dir.
```
$ ./aft_out/sim-verilator/Vaftx07
```

## Description

This demo just prints Tx/Rx message of queue to serial port, use no
other hardware and use only primary core (currently hart 0).
Other cores are simply going to wfi state and execute nothing else.
