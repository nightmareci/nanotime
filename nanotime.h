#ifndef _include_guard_nanotime_
#define _include_guard_nanotime_

/*
 * You can choose this license, if possible in your jurisdiction:
 *
 * Unlicense
 *
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or distribute
 * this software, either in source code form or as a compiled binary, for any
 * purpose, commercial or non-commercial, and by any means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors of
 * this software dedicate any and all copyright interest in the software to the
 * public domain. We make this dedication for the benefit of the public at
 * large and to the detriment of our heirs and successors. We intend this
 * dedication to be an overt act of relinquishment in perpetuity of all present
 * and future rights to this software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * For more information, please refer to <http://unlicense.org/>
 *
 *
 * Alternative license choice, if works can't be directly submitted to the
 * public domain in your jurisdiction:
 *
 * The MIT License (MIT)
 *
 * Copyright © 2022 Brandon McGriff <nightmareci@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the “Software”), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>

#ifdef _MSC_VER

#ifdef NANOTIME_IMPLEMENTATION

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

/* Attempt to suspend the current thread for the amount of time requested in "req". Refer to POSIX nanosleep documentation for more details. */
int nanosleep(const struct timespec* req, struct timespec* rem) {
	static HANDLE timer = NULL;
	static LARGE_INTEGER freq = { 0 };
	LARGE_INTEGER
		start, end, elapsed,
		req_li, rem_li, rem_subsec;

	/*
	 * Retrieval of the start time is placed here, so the elapsed time is
	 * closer to correct. Remember, all code here takes time to complete, so as
	 * much of that time as possible needs to be accounted for. Careful choices
	 * of where calculations are done have been made as well, to help reduce
	 * CPU overhead whenever possible.
	 */
	QueryPerformanceCounter(&start);

	if (req->tv_nsec >= 0L) {
		/*
		 * This short-delay portion is placed here, so repeated calls
		 * might have low overhead, allowing a "precise sleep"
		 * algorithm to have higher precision.
		 */
		if (req->tv_sec == 0 && req->tv_nsec <= 100L) {
			/*
			 * Allows the OS to schedule another process for a
			 * single time slice. Better than a delay of 0, which
			 * immediately returns with no actual non-CPU-hogging
			 * delay. The time-slice-yield behavior is specified in
			 * Microsoft's Windows documentation. Since a single
			 * time slice is probably at least 100 nanoseconds,
			 * this probably never requires rem to be set.
			 */
			SleepEx(0UL, FALSE);
			return 0;
		}
		else if (req->tv_sec >= 0 && req->tv_nsec <= 999999999L) {
			if (
				timer == NULL &&
#ifdef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
				/*
				 * Requesting a high resolution timer can make quite the
				 * difference, so always request high resolution if available.
				 * It's available in Windows 10 1803 and above. This
				 * arrangement of building it if the build system supports it
				 * will allow the executable to use high resolution if
				 * available on a user's system, but revert to low resolution
				 * if the user's system doesn't support high resolution.
				 */
				(timer = CreateWaitableTimerEx(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS)) == NULL &&
#endif
				(timer = CreateWaitableTimer(NULL, TRUE, NULL)) == NULL
			) {
				errno = EFAULT;
				return -1;
			}

			req_li.QuadPart = -(req->tv_sec * (1000000000LL / 100LL) + req->tv_nsec / 100LL);

			SetWaitableTimer(timer, &req_li, 0L, NULL, NULL, FALSE);
			WaitForSingleObject(timer, INFINITE);

			QueryPerformanceCounter(&end);

			elapsed.QuadPart = end.QuadPart - start.QuadPart;

			/*
			 * Windows really can suspend a process for less than the requested
			 * time, though it's quite uncommon.
			 */
			if (elapsed.QuadPart < -req_li.QuadPart) {
				errno = EINTR;
				if (rem != NULL) {
					rem_li.QuadPart = -req_li.QuadPart - elapsed.QuadPart;

					if (freq.QuadPart == 0) {
						/*
						 * Microsoft's Windows documentation says the frequency
						 * value is fixed at system startup, so it can be
						 * cached after getting it once.
						 */
						QueryPerformanceFrequency(&freq);
					}
					rem->tv_sec = (time_t)(rem_li.QuadPart / freq.QuadPart);
					rem_subsec.QuadPart = rem_li.QuadPart % freq.QuadPart;
					if (freq.QuadPart < 1000000000LL) {
						rem->tv_nsec = (long)(rem_subsec.QuadPart * (1000000000LL / freq.QuadPart));
					}
					else {
						rem->tv_nsec = (long)(rem_subsec.QuadPart / (freq.QuadPart / 1000000000LL));
					}
				}
				return -1;
			}
			else {
				return 0;
			}
		}
	}

	errno = EINVAL;
	return -1;
}

#else

/* Attempt to suspend the current thread for the amount of time requested in "req". Refer to POSIX nanosleep documentation for more details. */
int nanosleep(const struct timespec* req, struct timespec* rem);

#endif

#endif

#ifdef __cplusplus
}
#endif

#endif /* _include_guard_nanotime_ */
