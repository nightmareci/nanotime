# nanotime
A single-header library that provides nanosecond-resolution timestamps and sleeps for a variety of platforms.

Define `NANOTIME_IMPLEMENTATION` before one `#include` of `nanotime.h` for the implementation, then `#include` the `nanotime.h` header without that definition in other sources using `nanotime_` features.

Basic usage of `nanotime_now` and `nanotime_sleep`:
```c
#define NANOTIME_IMPLEMENTATION
#include "nanotime.h"
#include <stdio.h>

int main() {
    // Get the time before sleeping.
    const uint64_t start = nanotime_now();

    // Sleep for one second. NSEC_PER_SEC is a convenience constant defined to
    // make it easy to convert to/from seconds and nanoseconds.
    nanotime_sleep(1 * NSEC_PER_SEC);

    // Get the time after sleeping.
    const uint64_t end = nanotime_now();

    // The nanosecond count duration actually slept can be calculated here via
    // "end - start". And you can convert that nanosecond count to seconds via
    // "(end - start) / (double)NSEC_PER_SEC".
    printf("Slept: %lf seconds\n", (end - start) / (double)NSEC_PER_SEC);

    return 0;
}
```

C and C++ programs for testing the timestamp and sleep functions are provided; the C version requires C11, the C++ version requires C++11.

When using the included timestamp and sleep functions, the `nanotime.h` header has a somewhat complicated support matrix; C versions below C99 are entirely unsupported, as are C++ versions below C++11:
* Using C11 or higher with the optional C threads library will always be supported.
* Using C++ always requires C++11; with C++11 in use, the library will always be supported.
* Using C99 is somewhat supported; using C99 requires platform-specific features to be supported. Some common platforms are currently supported (Windows, macOS, Linux, perhaps other POSIX/UNIX platforms).

An implementation of the [fixed timestep](https://www.gafferongames.com/post/fix_your_timestep/) algorithm is provided, using a performance-optimized [accurate sleep](https://blog.bearcats.nl/accurate-sleep-function/) algorithm I've developed myself, to make it very easy to get precisely timed updates/frames in games:
```c
#define NANOTIME_IMPLEMENTATION
#include "nanotime.h"
#include <stdio.h>

int main() {
    nanotime_step_data stepper;
    nanotime_step_init(&stepper, NSEC_PER_SEC / 1000, nanotime_now, nanotime_sleep);

    nanotime_step_start(&stepper);
    for (int i = 0; i < 1000; i++) {
        const uint64_t start = stepper.sleep_point;
        nanotime_step(&stepper);
        const uint64_t duration = stepper.sleep_point - start;
        printf("Slept %lf seconds\n", (double)duration / NSEC_PER_SEC);
    }

    return 0;
}
```

An example C/SDL2 program is provided, demonstrating how the timestep feature can be integrated into games; the example C/SDL2 program requires C11.

If you want to use alternative timestamp and sleep functions, you can `#define NANOTIME_ONLY_STEP` before including `nanotime.h`; by omitting the timestamp and sleep functions, you can use the timestep feature when the timestamp and sleep functions aren't available on your target platform(s). The timestep feature is entirely platform-independent C code, so it only requires C99 or C++11:
```c
// SDL3 example
#define NANOTIME_ONLY_STEP
#define NANOTIME_IMPLEMENTATION
#include "nanotime.h"
#include "SDL.h"
// ...
    nanotime_step_data stepper;
    nanotime_step_init(&stepper, NSEC_PER_SEC / 60, SDL_GetTicksNS, SDL_DelayNS);
// ...
```
