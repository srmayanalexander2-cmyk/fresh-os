ChaN's FatFs R0.16 from `zephyrproject-rtos/fatfs` at
`4f60a12c58c3de4246857ad87c40239ecd93349b`. The license is in `ff.c` and
`ff.h`. FreshOS uses a freestanding string header, code page 437, no RTC,
8.3 filenames and 16 open-object locks. No formatting API is built in. Only
the x64 kernel links FatFs. The sector adapter is `native/hal/fat.c`; bounds
and the explicit data-disk marker check live in `native/hal/home.c`.
