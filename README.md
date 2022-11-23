# Portable nanosleep
A single-header library that provides POSIX nanosleep for POSIX and non-POSIX platforms, such as Windows.

Define `NANOSLEEP_IMPLEMENTATION` before one `#include` of `nanosleep.h` for the implementation, then `#include` the `nanosleep.h` header without that definition in other sources using `nanosleep()`. Refer to the [POSIX documentation](https://pubs.opengroup.org/onlinepubs/9699919799/functions/nanosleep.html) for all the details about `nanosleep()`:

```c
#define NANOSLEEP_IMPLEMENTATION
#include "nanosleep.h"

int main() {
  // Sleep for one second.
  struct timespec req = { 1, 0 };
  nanosleep(&req, NULL);
  return 0;
}
```

A simple test program, `test_nanosleep`, is provided, that can be built using CMake. The test program requires C11, but `nanosleep.h` might only require C89.
