# Emulating FreeRTOS on AFTx07 on Verilator

## Requirements
1. Set up Software in Socet Confluence.

https://wiki.itap.purdue.edu/pages/viewpage.action?pageId=247664452

2. GNU RISC-V toolchains.

First try if riscv-gcc toolchain is on your path.
```
$ riscv64-unknown-elf-gcc --version
riscv64-unknown-elf-gcc (SiFive GCC 10.1.0-2020.08.2) 10.1.0
Copyright (C) 2020 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
```

It should be added in the PATH in the setup step, if not download from here and add to PATH.
```
$ cd ~
$ wget https://static.dev.sifive.com/dev-tools/freedom-tools/v2020.08/riscv64-unknown-elf-gcc-10.1.0-2020.08.2-x86_64-linux-centos6.tar.gz
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

**NOTE:** run from main AFTx07 dir. Make sure the verilator is built with enough RAM. Refer to confluence page to how to build verilator.
```
$ ./aft_out/sim-verilator/Vaftx07
```

## Description

This demo just prints Tx/Rx message of queue to serial port, use no
other hardware and use only primary core (currently hart 0).
Other cores are simply going to wfi state and execute nothing else.


## For future revs of AFTx

Should the address of CLINT module change, it has to be updated in `riscv-virt.h` file.

For more information on how to port FreeRTOS to RISCV, check out this link [FreeRTOS on RISC-V Microcontrollers](https://www.freertos.org/Using-FreeRTOS-on-RISC-V.html#FREERTOS_CONFIG_SETTINGS)
