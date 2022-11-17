# nanosleep
A single-header library that provides POSIX nanosleep for POSIX and non-POSIX platforms, such as Windows.

Just include the header, then use nanosleep per the POSIX documentation:

```c
#include "nanosleep.h"

int main() {
  // Sleep for one second.
  struct timespec req = { 1, 0 };
  nanosleep(&req, NULL);
  return 0;
}
```
