# Portable nanosleep
A single-header library that provides POSIX nanosleep for POSIX and non-POSIX platforms, such as Windows.

Define `PORTABLE_NANOSLEEP_IMPLEMENTATION` before one `#include` of `portable_nanosleep.h` for the implementation, then `#include` the `portable_nanosleep.h` header without that definition in other sources using `nanosleep()`. Refer to the [POSIX documentation](https://pubs.opengroup.org/onlinepubs/9699919799/functions/nanosleep.html) for all the details about `nanosleep()`:

```c
#define PORTABLE_NANOSLEEP_IMPLEMENTATION
#include "portable_nanosleep.h"

int main() {
    // Sleep for one second.
    struct timespec req = { 1, 0 };
    nanosleep(&req, NULL);
    return 0;
}
```

C and C++ test programs are provided; the C version requires C11, the C++ version requires C++17. `portable_nanosleep.h` might only require C89. The test programs can be built with the provided `CMakeLists.txt`, but are entirely portable code that should be easy to drop into some other build system or IDE project.
