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
#if (__cplusplus < 201103L)
#error "Current C++ standard is not at least C++11, the nanotime library requires at least C++11."
#endif
#elif !defined(__STDC_VERSION__) || (__STDC_VERSION__ < 199901L)
#error "Current C standard is not at least C99, the nanotime library requires at least C99."
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <assert.h>

#define NSEC_PER_SEC 1000000000L

/*
 * Returns the current time since some unspecified epoch.
 */
uint64_t nanotime_now();

/*
 * Sleeps the current thread for the requested count of nanoseconds. The slept
 * duration may be less than, equal to, or greater than the time requested.
 */
void nanotime_sleep(const uint64_t nsec_count);

#ifdef NANOTIME_IMPLEMENTATION

/*
 * Non-portable, platform-specific implementations are first. If none of them
 * match the current platform, the standard C/C++ versions are used as a last
 * resort.
 */

#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#ifndef NANOTIME_NOW_IMPLEMENTED
uint64_t nanotime_now() {
	LARGE_INTEGER performanceCount;
	QueryPerformanceCounter(&performanceCount);
	return performanceCount.QuadPart * (uint64_t)100;
}
#define NANOTIME_NOW_IMPLEMENTED
#endif

#ifndef NANOTIME_SLEEP_IMPLEMENTED
void nanotime_sleep(const uint64_t nsec_count) {
	static HANDLE timer = NULL;
	static LARGE_INTEGER freq = { 0 };
	LARGE_INTEGER dueTime;

	/*
	 * This short-delay portion is placed here, so repeated calls might
	 * have low overhead, allowing the "precise sleep" algorithm to have
	 * higher precision.
	 */
	if (nsec_count <= UINT64_C(100)) {
		/*
		 * Allows the OS to schedule another process for a single time
		 * slice. Better than a delay of 0, which immediately returns
		 * with no actual non-CPU-hogging delay. The time-slice-yield
		 * behavior is specified in Microsoft's Windows documentation.
		 */
		SleepEx(0UL, FALSE);
		return 0;
	}
	else {
		if (
			timer == NULL &&
#ifdef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
			/*
			 * Requesting a high resolution timer can make quite
			 * the difference, so always request high resolution if
			 * available.  It's available in Windows 10 1803 and
			 * above. This arrangement of building it if the build
			 * system supports it will allow the executable to use
			 * high resolution if available on a user's system, but
			 * revert to low resolution if the user's system
			 * doesn't support high resolution.
			 */
			(timer = CreateWaitableTimerEx(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS)) == NULL &&
#endif
			(timer = CreateWaitableTimer(NULL, TRUE, NULL)) == NULL
		) {
			return;
		}

		dueTime.QuadPart = -(LONGLONG)(nsec_count / UINT64_C(100));

		SetWaitableTimer(timer, &dueTime, 0L, NULL, NULL, FALSE);
		WaitForSingleObject(timer, INFINITE);
	}
}
#define NANOTIME_SLEEP_IMPLEMENTED
#endif

#endif

#if defined(__unix__) && !defined(NANOTIME_NOW_IMPLEMENTED)
// Current platform is some version of POSIX, that might have clock_gettime.
#include <unistd.h>
#if _POSIX_VERSION >= 199309L
#include <time.h>
uint64_t nanotime_now() {
	struct timespec now;
	const int status = clock_gettime(CLOCK_REALTIME, &now);
	assert(status == 0 || (status == -1 && errno != EOVERFLOW));
	return (uint64_t)now.tv_sec * (uint64_t)NSEC_PER_SEC + (uint64_t)now.tv_nsec;
}
#define NANOTIME_NOW_IMPLEMENTED

#else
#error "Current platform is UNIX/POSIX, but doesn't support required functionality (IEEE Std 1003.1b-1993 is required)."
#endif
#endif

#if (defined(__APPLE__) || defined(__MACH__)) && !defined(NANOTIME_NOW_IMPLEMENTED)
// Current platform is some Apple operating system, or at least uses some Mach kernel.
#include <sys/time.h>
#include <mach/clock.h>
#include <mach/mach.h>
uint64_t nanotime_now() {
	clock_serv_t clock_serv;
	mach_timespec_t now;
	host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &clock_serv);
	clock_get_time(clock_serv, &now);
	mach_port_deallocate(mach_task_self(), clock_serv);
	return (uint64_t)now.tv_sec * (uint64_t)NSEC_PER_SEC + (uint64_t)now.tv_nsec;
}
#define NANOTIME_NOW_IMPLEMENTED
#endif

#if (defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))) && !defined(NANOTIME_SLEEP_IMPLEMENTED)
#include <time.h>
void nanotime_sleep(const uint64_t nsec_count) {
	const struct timespec req = {
		.tv_sec = (time_t)(nsec_count / NSEC_PER_SEC),
		.tv_nsec = (long)(nsec_count % NSEC_PER_SEC)
	};
	const int status = nanosleep(&req, NULL);
	assert(status == 0 || (status == -1 && errno != EINVAL));
}
#define NANOTIME_SLEEP_IMPLEMENTED
#endif

#ifndef NANOTIME_NOW_IMPLEMENTED
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#include <time.h>
uint64_t nanotime_now() {
	struct timespec now;
	const int status = timespec_get(&now, TIME_UTC);
	assert(status == TIME_UTC);
	return (uint64_t)now.tv_sec * (uint64_t)NSEC_PER_SEC + (uint64_t)now.tv_nsec;
}
#define NANOTIME_NOW_IMPLEMENTED
#endif
#endif

#ifndef NANOTIME_SLEEP_IMPLEMENTED
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L) && !defined(__STDC_NO_THREADS__)
#include <threads.h>
void nanotime_sleep(const uint64_t nsec_count) {
	const struct timespec req = {
		.tv_sec = (time_t)(nsec_count / NSEC_PER_SEC),
		.tv_nsec = (long)(nsec_count % NSEC_PER_SEC)
	};
	const int status = thrd_sleep(&req, NULL);
	assert(status == 0 || status == -1);
}
#define NANOTIME_SLEEP_IMPLEMENTED
#endif
#endif

#endif

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#ifdef NANOTIME_IMPLEMENTATION
#if defined(__cplusplus) && (__cplusplus >= 201103L)

#ifndef NANOTIME_NOW_IMPLEMENTED
#include <cstdint>
#include <chrono>
extern "C" uint64_t nanotime_now() {
	return static_cast<uint64_t>(
		std::chrono::time_point_cast<std::chrono::nanoseconds>(
			std::chrono::high_resolution_clock::now()
		).time_since_epoch().count()
	);
}
#define NANOTIME_NOW_IMPLEMENTED
#endif

#ifndef NANOTIME_SLEEP_IMPLEMENTED
#include <cstdint>
#include <thread>
#include <exception>
extern "C" void nanotime_sleep(const uint64_t nsec_count) {
	try {
		std::this_thread::sleep_for(std::chrono::nanoseconds(nsec_count));
	}
	catch (std::exception e) {
	}
}
#define NANOTIME_SLEEP_IMPLEMENTED
#endif

#endif
#endif
#endif

#ifndef NANOTIME_NOW_IMPLEMENTED
#error "Failed to implement nanotime_now (try using C11 with C11 threads support or C++11)."
#endif

#ifndef NANOTIME_SLEEP_IMPLEMENTED
#error "Failed to implement nanotime_sleep (try using C11 with C11 threads support or C++11)."
#endif

#endif /* _include_guard_nanotime_ */
