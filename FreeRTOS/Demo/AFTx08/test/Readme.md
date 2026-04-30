# AFTx08 Test Programs

This folder contains small benchmark programs used to evaluate FreeRTOS on the AFTx08. The tests compare FreeRTOS task-based execution with a bare-metal. Most programs use the cycle counter to report timing results through UART output.

## How to use these tests

 To run one test, build only one test file at a time because each test provides its own `main_blinky()` function.

A simple way to run a test is:

1. Choose one `.c` file from this folder.
2. Replace or temporarily copy it into `FreeRTOS/Demo/AFTx08/main_blinky.c`.
3. Build the demo from `FreeRTOS/Demo/AFTx08`:

```sh
make -C build/gcc/ clean
make -C build/gcc/

4. Copy the generated binary to the AFT simulation memory image location as `meminit.bin`.
5. Run the AFTx08 Verilator simulation and check the UART output.

Some tests contain this setting near the top of the file:

```c
#define USE_FREERTOS_MODE 1
```

Set it to `1` to run the FreeRTOS version, or `0` to run the bare-metal version when the test supports both modes.

## Test file summary

| File | Purpose | 
|---|---|---|
| `throughput.c` | Measures how many Fibonacci workload jobs can finish within a fixed time. This is used to compare raw work completion between FreeRTOS and bare-metal. 
| `jitter.c` | Measures timing variation of a periodic FreeRTOS task while a background workload is running. 
| `interrupt_latency.c` | Measures the delay from a simulated timer interrupt/event to the software response. In FreeRTOS mode, the ISR wakes a task. In bare-metal mode, the main loop handles a flag. 
| `interrupt_latency_legacy.c` | Older version of the interrupt latency test kept for reference. 
| `car_brake_response.c` | Simple autonomous braking scenario. The car starts at a fixed speed and distance, detects a brake threshold, and measures when braking begins. 
| `car_pid_throughput.c` | Autonomous braking test using PID-style distance control. It focuses on how many control updates are completed in each reporting window. 
| `car_pid_jitter.c` | Similar PID braking test, but focused on periodic control timing stability. 
| `car_emergency_control.c` | More complete autonomous braking benchmark with normal PID control, emergency braking, background workload, and handoff timing. It can compare FreeRTOS and bare-metal behavior.


## Metrics used

- Throughput: how many workload complete during a fixed window.
- Latency how long it takes for the system to respond after an event or threshold is detected.
- Handoff latency**: in FreeRTOS mode, the time from ISR notification to the task actually running.
- Jitter how much a periodic task or control loop deviates from its expected period.

## Notes

- The timing conversion assumes `configCPU_CLOCK_HZ` in `FreeRTOSConfig.h` matches the AFTx08 simulation clock.
- UART printing can slow down simulation, so printed results should be used mainly for comparison between matching test setups.

