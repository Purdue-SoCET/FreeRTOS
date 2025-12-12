This repository is a fork of [FreeRTOS 202212.00](https://github.com/FreeRTOS/FreeRTOS/tree/202212.00), created for porting FreeRTOS to AFTx08, which is a microcontroller of Purdue-SoCET orgs.

The FreeRTOS kernel source files are included as a submodule from the official [FreeRTOS kernel](https://github.com/FreeRTOS/FreeRTOS-Kernel).

## Getting started

The main directory of interest is: FreeRTOS/Demo/AFTx08.

To understand the repo structure, please take a look at [FreeRTOS Source File Structure](https://www.freertos.org/a00017.html).

For details on porting FreeRTOS to RISC-V, please look at [RISV Port](https://www.freertos.org/a00090.html#RISC-V).

To modify config file, please look at [FreeRTOSConfig.h](https://www.freertos.org/a00110.html).

## How to build

First, initialize the FreeRTOS kernel submodule:

```
git submodule update --init
```

Build instructions details can be found in:

```
FreeRTOS/Demo/AFTx08
```
