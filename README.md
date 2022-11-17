# nanosleep
A single-header library that provides POSIX nanosleep for POSIX and non-POSIX platforms, such as Windows.

Just include the header, then use nanosleep per the [POSIX documentation](https://pubs.opengroup.org/onlinepubs/9699919799/functions/nanosleep.html):

```c
#include "nanosleep.h"

int main() {
  // Sleep for one second.
  struct timespec req = { 1, 0 };
  nanosleep(&req, NULL);
  return 0;
}
```

A simple test program, `test_nanosleep`, is provided, that can be built using CMake. The test program requires C11, but `nanosleep.h` might only require C89.
