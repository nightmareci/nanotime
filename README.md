# nanotime
A single-header library that provides nanosecond-resolution timestamps, sleeps, and accurate-sleep fixed timestepping for a variety of platforms.

Define `NANOTIME_IMPLEMENTATION` before one `#include` of `nanotime.h` for the implementation, then `#include` the `nanotime.h` header without that definition in other sources using `nanotime_` features. Alternatively, just add `nanotime.c` to your project, and use `nanotime.h` normally, with no macros defined before `#include`s of `nanotime.h`.

Basic usage of `nanotime_now` and `nanotime_sleep`:
```c
#define NANOTIME_IMPLEMENTATION
#include "nanotime.h"
#include <stdio.h>

int main() {
    // Get the time before sleeping.
    const uint64_t start = nanotime_now();

    // Sleep for one second. NANOTIME_NSEC_PER_SEC is a convenience constant
    // defined to make it easy to convert to/from seconds and nanoseconds.
    nanotime_sleep(1 * NANOTIME_NSEC_PER_SEC);

    // Get the time after sleeping.
    const uint64_t end = nanotime_now();

    // The nanosecond count duration actually slept can be calculated here with
    // nanotime_interval; the interval function is recommended, as it properly
    // handles overflow of time values. And you can convert that nanosecond
    // count to seconds by dividing by NANOTIME_NSEC_PER_SEC.
    printf("Slept: %f seconds\n", nanotime_interval(start, end, nanotime_now_max()) / (double)NANOTIME_NSEC_PER_SEC);

    return 0;
}
```

`nanotime_yield` is also provided, and causes the thread within which it was called to yield the processor to another process for a small time slice.

C and C++ programs for testing the timestamp and sleep functions are provided; the C version requires C11, the C++ version requires C++11.

When using the included processor yield (`nanotime_yield`), timestamp (`nanotime_now`), and sleep (`nanotime_sleep`) functions, the `nanotime.h` header has a somewhat complicated support matrix; the C headers `stdint.h` and `stdbool.h` are required:
* Using a C11 or higher implementation that includes the standard C threads library will always be supported.
* Using C++11 or higher will always be supported.
* Using C versions other than C11 or higher without threading support is somewhat supported, as they require platform-specific features to be supported. Some common platforms are currently supported (Windows, macOS, Linux, perhaps other POSIX/UNIX platforms). For Windows/MSVC support, Visual Studio 2010 or higher is required, with the other supported platforms requiring C99 or higher.

An implementation of the [fixed timestep](https://www.gafferongames.com/post/fix_your_timestep/) algorithm is provided, using a performance and power usage optimized [accurate sleep](https://blog.bearcats.nl/accurate-sleep-function/) algorithm I've developed myself, to make it very easy to get precisely timed updates/frames in games:
```c
#define NANOTIME_IMPLEMENTATION
#include "nanotime.h"
#include <stdio.h>

int main() {
    nanotime_step_data stepper;
    nanotime_step_init(&stepper, NANOTIME_NSEC_PER_SEC / 1000, nanotime_now_max(), nanotime_now, nanotime_sleep);
    for (int i = 0; i < 1000; i++) {
        const uint64_t start = stepper.sleep_point;
        nanotime_step(&stepper);
        const uint64_t duration = nanotime_interval(start, stepper.sleep_point, nanotime_now_max());
        printf("Slept %f seconds\n", (double)duration / NANOTIME_NSEC_PER_SEC);
    }

    return 0;
}
```

Example C/SDL2 programs are provided, `test_nanotime_step` and `render_thread_test_nanotime_step`, demonstrating how the timestep feature can be integrated into games; the example C/SDL2 programs require C11. The example programs have some CMake options:
* Boolean `MULTITHREADED`, that makes `test_nanotime_step` have separate logic and render threads; it's disabled by default.
* Boolean `REALTIME`, that makes both programs' thread priority realtime for their thread(s), which will only work on Linux; it's disabled by default.

If you want to omit the timestamp, sleep, and yield functions, you can `#define NANOTIME_ONLY_STEP` before including `nanotime.h`; by omitting the timestamp, sleep, and yield functions, you can use the timestep feature when the timestamp, sleep, and yield functions aren't available on your target platform(s), or if you don't wish to use the included timestamp and sleep functions in lieu of others. The timestep feature doesn't use platform-specific features, so its support matrix is simpler, requiring C99 or higher, C++11 or higher, or Visual Studio 2010 or higher:
```c
// SDL3 example
#define NANOTIME_ONLY_STEP
#define NANOTIME_IMPLEMENTATION
#include "nanotime.h"
#include <SDL3/SDL.h>
// ...
    nanotime_step_data stepper;
    // SDL3 errors out if the time values overflow, so using UINT64_MAX for the
    // maximum timestamp value is appropriate.
    nanotime_step_init(&stepper, NANOTIME_NSEC_PER_SEC / 60, UINT64_MAX, SDL_GetTicksNS, SDL_DelayNS);
// ...
```
