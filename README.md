# coop-sched

A minimalistic scheduler / context switcher to facilitate cooperative multitasking on ARM Cortex-M.

## Why?

Sometimes a bare-metal embedded system grows too complex to fit into a simple single-threaded `while (1)` loop pattern, but at the same time not complex enough to justify adding a full-blown RTOS. Sometimes it's also necessary to add a modicum of concurrency to are large legacy codebase, but without introducing the extra complexity of preemptive task switching and multithreading, and their associated pitfalls (like race conditions). Finally, for extremely space-constrained applications (e.g. bootloaders) the smallest and least complex solution is desirable.

## Size

The whole project is less than 250 lines of code, and compiled size is significantly less than 1K (current version takes up 611 bytes when compiled with `-Os`).

## Dependencies

`coop-sched` only needs standard, low-level CMSIS functions / definitions which are usually bundled with the MCU vendor's libraries. `platform.h` includes these MCU-specific headers - to support new MCUs, this file has to be extended, but `coop_sched.c` should not need to change.

## Supported MCUs

At the moment, MCUs based on Cortex-M4 (both with and without FPU) are supported. ATSAM4L and STM32L433 are supported out of the box; more MCUs will be added in the future.

## TODO

Add support for other cores such as Cortex-M0 and -M7.
