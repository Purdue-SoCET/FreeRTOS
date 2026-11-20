# FreeRTOS on AFTx08

FreeRTOS ported to the dual-hart AFTx08 SoC used by Purdue SoCET. The supported way to run it is the Verilator model in AFT-dev.

Build commands below are from this repository. The simulator command is from the AFT-dev root.

## Layout

```text
FreeRTOS/
  FreeRTOS-Kernel/          Kernel, including the GCC RISC-V port
  Demo/AFTx08/              Port and demos for this chip
    FreeRTOSConfig.h        Kernel configuration
    riscv-virt.h            CLINT addresses and which demo starts
    main.c                  Chooses the demo
    arch/                   IPI, interrupt entry, and the scheduler lock
    compare_tests/          Bare metal, single-core, and SMP measurements
    compare_mandelbrot/     Single-core and SMP Mandelbrot demo
    build/gcc/              Makefile, linker script, and startup code
```

`Demo/AFTx08/build/gcc/output/` is a local build directory.

## Requirements

Be sure to run (Prereq): 
```sh
cd ~/AFT-dev
git submodule update --init --recursive
./setup.sh
./build.sh
```
The demo is built with `riscv64-unknown-elf-gcc` for `rv32imac`. On the SoCET machines that compiler is here:

```sh
export PATH="/package/asicfab/riscv-gcc/13.2.0/bin:${PATH}"
riscv64-unknown-elf-gcc --version
```

AFT-dev must already have been configured and built, so this simulator exists:

```text
aft_out/socet_aft_aftx07_2.0.0/sim-verilator/Vaftx07
```

## Build

```sh
make -C FreeRTOS/Demo/AFTx08/build/gcc
```

That produces:

```text
FreeRTOS/Demo/AFTx08/build/gcc/output/RTOSDemo.elf
FreeRTOS/Demo/AFTx08/build/gcc/output/RTOSDemo.bin
```

With no extra flags, the image runs the migration demo. Hart 0 and hart 1 both run. See `Demo/AFTx08/Readme.md` for the other demos.

Clean the build with:

```sh
make -C FreeRTOS/Demo/AFTx08/build/gcc clean
```

## Run on Verilator

From the AFT-dev root:

```sh
cp FreeRTOS/FreeRTOS/Demo/AFTx08/build/gcc/output/RTOSDemo.bin meminit.bin
./aft_out/socet_aft_aftx07_2.0.0/sim-verilator/Vaftx07 --meminit-file meminit.bin --cycle-limit 20000000
```

## Other builds

Each of these writes to its own directory under `build/gcc/output/`, so the images do not overwrite each other.

```sh
make -C FreeRTOS/Demo/AFTx08/build/gcc COMPARE=bare
make -C FreeRTOS/Demo/AFTx08/build/gcc COMPARE=single
make -C FreeRTOS/Demo/AFTx08/build/gcc COMPARE=smp
make -C FreeRTOS/Demo/AFTx08/build/gcc MANDEL=single
make -C FreeRTOS/Demo/AFTx08/build/gcc MANDEL=smp
```

Copy the matching `RTOSDemo.bin` to `meminit.bin`. For `COMPARE=smp` that path is `build/gcc/output/smp/RTOSDemo.bin`. For `MANDEL=smp` it is `build/gcc/output/mandel-smp/RTOSDemo.bin`.

## More detail

`Demo/AFTx08/Readme.md` describes the boot path, the demos, and the files to change when the CLINT address moves.
