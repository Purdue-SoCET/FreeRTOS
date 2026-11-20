# AFTx08 FreeRTOS demo

This directory is the FreeRTOS port for the dual-hart AFTx08 chip. Commands here assume the current directory is `Demo/AFTx08`. The simulator is run from the AFT-dev root. The repository readme covers the same steps from the FreeRTOS root.

## Build and run

```sh
make -C build/gcc
```

From the AFT-dev root:

```sh
cp FreeRTOS/FreeRTOS/Demo/AFTx08/build/gcc/output/RTOSDemo.bin meminit.bin
./aft_out/socet_aft_aftx07_2.0.0/sim-verilator/Vaftx07 --meminit-file meminit.bin --cycle-limit 20000000
```

Clean with `make -C build/gcc clean`.

## How the port runs

The image is linked into on-chip RAM at `0x8400` by `build/gcc/fake_rom.ld`.

Both harts share one kernel and one ready list. Hart 0 owns the tick. It reads the CLINT `mtime` register at `0x9001BFF8` and writes `mtimecmp` for hart 0. Hart 1 waits in `wfi` until hart 0 sets its `msip` bit, then runs tasks. The software interrupt path is `arch/ipi.c`.

The scheduler lock is a two-hart Peterson lock in `arch/spinlock.c`. The earlier `lr.w` / `sc.w` lock did not hold on this cache, because one hart's store does not clear the other hart's reservation.

`main.c` selects the program via environment variables set in `riscv-virt.h`. However, the declartion of `MANDEL=single|smp` and `COMPARE=bare|single|smp` will override that selection from the Makefile.

## Demos

`riscv-virt.h` chooses the program for a plain build:

| Flag | File | What it shows |
| --- | --- | --- |
| `mainCREATE_MIGRATION_DEMO` | `main_migrate.c` | One task moves from hart 1 to hart 0. This is the default. |
| `mainCREATE_AFFINITY_DEMO` | `main_affinity.c` | One task stays on hart 0 and one stays on hart 1. |
| `mainCREATE_SMP_DEMO` | `main_smp.c` | Two tasks with no affinity. |
| `mainCREATE_SIMPLE_BLINKY_DEMO_ONLY` | `main_blinky.c` | Queue demo on hart 0. Hart 1 stays in `wfi`. |

Turn on one flag and leave the others at 0.

These Makefile builds do not use those flags. Each one has its own output directory.

```sh
make -C build/gcc COMPARE=bare
make -C build/gcc COMPARE=single
make -C build/gcc COMPARE=smp
make -C build/gcc MANDEL=single
make -C build/gcc MANDEL=smp
```

`compare_tests/` runs the same control and load work three ways: bare metal, FreeRTOS on hart 0, and FreeRTOS on both harts. The console summary is the measurement. 

`compare_mandelbrot/` computes one ASCII frame on hart 0, or split across both harts (based on the setup), and prints `compute_cycles`.


## Configuration

`FreeRTOSConfig.h` holds the tick rate, heap size, and core count. The compare and Mandelbrot builds pass `-D` flags for the core count, affinity, and timers, so those values come from the Makefile for those builds.

`riscv-virt.h` holds the CLINT base, `0x90010000`. If a later chip moves the CLINT, update that header. The tick will not run until `mtime` and `mtimecmp` match the new map.

FPGA startup is not part of this port. Use the Verilator model above.
