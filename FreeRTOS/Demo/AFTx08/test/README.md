# AFTx08 test programs

These files are not built by `Demo/AFTx08/build/gcc/Makefile`. Copying one over `main_blinky.c` is not the current flow.

The wired comparison is `../compare_tests/`. Build it with `COMPARE=bare`, `COMPARE=single`, or `COMPARE=smp`, as described in `../Readme.md`.

The programs here are earlier experiments. Several can run as FreeRTOS tasks or as a bare-metal loop, selected near the top of the file:

```c
#define USE_FREERTOS_MODE 1
```

Timing uses the CPU cycle counter. `configCPU_CLOCK_HZ` in `FreeRTOSConfig.h` is 25 MHz, which matches the Verilator model.

| File | What it measures |
| --- | --- |
| `throughput.c` | Fibonacci jobs finished in a fixed window. |
| `jitter.c` | Variation of a periodic task while a background load runs. |
| `interrupt_latency.c` | Delay from a timer event to the response. |
| `interrupt_latency_legacy.c` | Older copy of the latency test. |
| `car_brake_response.c` | Time from a brake threshold to the start of braking. |
| `car_pid_throughput.c` | PID control updates finished in a window. |
| `car_pid_jitter.c` | Period variation of the PID loop. |
| `car_emergency_control.c` | PID control plus an emergency brake path. |
