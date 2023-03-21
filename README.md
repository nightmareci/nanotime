# nanotime
A single-header library that provides nanosecond-resolution timestamps, sleeps, and accurate-sleep fixed timestepping for a variety of platforms.

Define `NANOTIME_IMPLEMENTATION` before one `#include` of `nanotime.h` for the implementation, then `#include` the `nanotime.h` header without that definition in other sources using `nanotime_` features.

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

    // The nanosecond count duration actually slept can be calculated here via
    // "end - start". And you can convert that nanosecond count to seconds via
    // "(end - start) / (double)NANOTIME_NSEC_PER_SEC".
    printf("Slept: %lf seconds\n", (end - start) / (double)NANOTIME_NSEC_PER_SEC);

    return 0;
}
```

C and C++ programs for testing the timestamp and sleep functions are provided; the C version requires C11, the C++ version requires C++11.

When using the included timestamp and sleep functions, the `nanotime.h` header has a somewhat complicated support matrix; `stdint.h` must be available:
* Using C11 or higher with the optional C threads library will always be supported.
* Using C++11 or higher will always be supported.
* Using C versions other than C11 or higher without threading support is somewhat supported, as they require platform-specific features to be supported. Some common platforms are currently supported (Windows, macOS, Linux, perhaps other POSIX/UNIX platforms). For Windows/MSVC support, Visual Studio 2010 or higher is required, with the other supported platforms requiring C99 or higher.

An implementation of the [fixed timestep](https://www.gafferongames.com/post/fix_your_timestep/) algorithm is provided, using a performance-optimized [accurate sleep](https://blog.bearcats.nl/accurate-sleep-function/) algorithm I've developed myself, to make it very easy to get precisely timed updates/frames in games:
```c
#define NANOTIME_IMPLEMENTATION
#include "nanotime.h"
#include <stdio.h>

int main() {
    nanotime_step_data stepper;
    nanotime_step_init(&stepper, NANOTIME_NSEC_PER_SEC / 1000, nanotime_now, nanotime_sleep);
    for (int i = 0; i < 1000; i++) {
        const uint64_t start = stepper.sleep_point;
        nanotime_step(&stepper);
        const uint64_t duration = stepper.sleep_point - start;
        printf("Slept %lf seconds\n", (double)duration / NANOTIME_NSEC_PER_SEC);
    }

    return 0;
}
```

An example C/SDL2 program is provided, demonstrating how the timestep feature can be integrated into games; the example C/SDL2 program requires C11.

If you want to use alternative timestamp and sleep functions, you can `#define NANOTIME_ONLY_STEP` before including `nanotime.h`; by omitting the timestamp and sleep functions, you can use the timestep feature when the timestamp and sleep functions aren't available on your target platform(s), or if you don't wish to use the included timestamp and sleep functions in lieu of others. The timestep feature doesn't use platform-specific features, so its support matrix is simpler, requiring C99 or higher, C++11 or higher, or Visual Studio 2010 or higher:
```c
// SDL3 example
#define NANOTIME_ONLY_STEP
#define NANOTIME_IMPLEMENTATION
#include "nanotime.h"
#include "SDL.h"
// ...
    nanotime_step_data stepper;
    nanotime_step_init(&stepper, NANOTIME_NSEC_PER_SEC / 60, SDL_GetTicksNS, SDL_DelayNS);
// ...
```
