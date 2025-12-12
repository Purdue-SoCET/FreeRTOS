# Emulating FreeRTOS on AFTx08 on Verilator

## Requirements

1. Set up the software environment in Socet AFT-dev.

https://purdue0.sharepoint.com/sites/ENGR-ECE-O-SOCET/SitePages/Initial-Setup-Instructions.aspx

2. GNU RISC-V toolchains.

First, check whether the RISC-V GCC toolchain is already in your PATH:

```
$ riscv64-unknown-elf-gcc --version
riscv64-unknown-elf-gcc (SiFive GCC 10.1.0-2020.08.2) 10.1.0
Copyright (C) 2020 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
```

If not, download and install the toolchain, then add it to your PATH:
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

If the build is successful, RTOSDemo.bin will be generated in:

```
build/gcc/output/RTOSDemo.bin
```

3. Prepare meminit.bin for AFTx08

Copy RTOSDemo.bin into the main AFT-dev repository and rename it to meminit.bin, placing it in the directory used by Verilator to initialize memory.

## How to run

**NOTE:** Run the simulator from the AFTx08 top-level directory, not from the FreeRTOS demo directory.
Ensure Verilator was built with sufficient RAM (refer to the AFT-dev).
```
$ ./aft_out/sim-verilator/Vaftx07
```
(If the executable name or path differs in your repository, use the actual generated Verilator binary.)

## Description

This demo prints transmit and receive messages from a FreeRTOS queue to the UART (serial output).
Only the primary core (typically hart 0) is used. All other cores enter the wfi state and do not execute additional tasks.

## For future revs of AFTx

Notes for Future AFTx Revisions

If the base address of the CLINT (timer module) changes in future AFTx revisions, the corresponding address definitions must be updated in the platform header files (for example, riscv-virt.h).
Failure to update these addresses will cause the FreeRTOS tick interrupt to malfunction.

For more details on porting FreeRTOS to RISC-V, see:

For more information on how to port FreeRTOS to RISCV, check out this link [FreeRTOS on RISC-V Microcontrollers](https://www.freertos.org/Using-FreeRTOS-on-RISC-V.html#FREERTOS_CONFIG_SETTINGS)
