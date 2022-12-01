# nanotime
A single-header library that provides nanosecond-resolution timestamps and sleeps for a variety of platforms.

Define `NANOTIME_IMPLEMENTATION` before one `#include` of `nanotime.h` for the implementation, then `#include` the `nanotime.h` header without that definition in other sources using `nanotime_` features.

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

C and C++ test programs are provided; the C version requires C11, the C++ version requires C++11.

The `nanotime.h` header has a somewhat complicated support matrix; C versions below C99 are entirely unsupported, as are C++ versions below C++11:
* Using C11 or higher with the optional C threads library will always be supported.
* Using C++ always requires C++11; with C++11 in use, the library will always be supported.
* Using C99 is somewhat supported; using C99 requires platform-specific features to be supported. Some common platforms are currently supported (Windows, macOS, Linux, perhaps other POSIX/UNIX platforms).
