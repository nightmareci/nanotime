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

#ifndef NANOTIME_ONLY_STEP

/*
 * Returns the current time since some unspecified epoch.
 */
uint64_t nanotime_now();

/*
 * Sleeps the current thread for the requested count of nanoseconds. The slept
 * duration may be less than, equal to, or greater than the time requested.
 */
void nanotime_sleep(uint64_t nsec_count);

#endif

typedef struct nanotime_step_data {
	uint64_t sleep_duration;
	uint64_t (* now)();
	void (* sleep)(uint64_t nsec_count);

	uint64_t nonzero_sleep_overhead;
	uint64_t zero_sleep_duration;

	uint64_t accumulator;
	uint64_t sleep_point;
} nanotime_step_data;

/*
 * Initializes the nanotime precise fixed timestep object.
 */
void nanotime_step_init(nanotime_step_data* const stepper, const uint64_t sleep_duration, uint64_t (* const now)(), void (* const sleep)(uint64_t nsec_count));

/*
 * Starts the nanotime precise fixed timestep object's timekeeping. Must be
 * called only once at some point after init, but before the first step
 * iteration. Generally, should be called immediately before entering the loop
 * implementing a fixed timestep logic update cycle.
 */
void nanotime_step_start(nanotime_step_data* const stepper);

/*
 * Does one step of sleeping for a fixed timestep logic update cycle. It makes
 * a best-attempt at a precise delay per iteration, but might skip a cycle of
 * sleeping if skipping sleeps is required to catch up to the correct
 * wall-clock time.
 */
void nanotime_step(nanotime_step_data* const stepper);

#if !defined(NANOTIME_ONLY_STEP) && defined(NANOTIME_IMPLEMENTATION)

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
	return performanceCount.QuadPart * UINT64_C(100);
}
#define NANOTIME_NOW_IMPLEMENTED
#endif

#ifndef NANOTIME_SLEEP_IMPLEMENTED
void nanotime_sleep(uint64_t nsec_count) {
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
void nanotime_sleep(uint64_t nsec_count) {
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
void nanotime_sleep(uint64_t nsec_count) {
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

#if !defined(NANOTIME_ONLY_STEP) && defined(__cplusplus) && (__cplusplus >= 201103L) && defined(NANOTIME_IMPLEMENTATION)

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
extern "C" void nanotime_sleep(uint64_t nsec_count) {
	try {
		std::this_thread::sleep_for(std::chrono::nanoseconds(nsec_count));
	}
	catch (std::exception e) {
	}
}
#define NANOTIME_SLEEP_IMPLEMENTED
#endif

#endif

#ifdef NANOTIME_IMPLEMENTATION

#ifndef NANOTIME_NOW_IMPLEMENTED
#error "Failed to implement nanotime_now (try using C11 with C11 threads support or C++11)."
#endif

#ifndef NANOTIME_SLEEP_IMPLEMENTED
#error "Failed to implement nanotime_sleep (try using C11 with C11 threads support or C++11)."
#endif

#endif

#ifdef NANOTIME_IMPLEMENTATION
#ifdef __cplusplus
extern "C" {
#endif

// This divisor might have to be adjusted up or down, based on your target platform(s).
#define NANOTIME_SLEEP_DIVISOR  UINT64_C(10000)

#define NANOTIME_NONZERO_SLEEP (NSEC_PER_SEC / NANOTIME_SLEEP_DIVISOR)

void nanotime_step_init(nanotime_step_data* const stepper, const uint64_t sleep_duration, uint64_t (* const now)(), void (* const sleep)(uint64_t nsec_count)) {
	stepper->sleep_duration = sleep_duration;
	stepper->now = now;
	stepper->sleep = sleep;

	{
		/*
		 * Sleeping can have some nonzero roughly constant amount of
		 * overhead above a nonzero delay request, so the overhead is
		 * approximated here. In the case of roughly zero overhead,
		 * this will produce a correct overhead approximately equal to
		 * zero seconds, or exactly zero seconds if the test delay
		 * ended up being shorter than the requested duration.
		 */
		const uint64_t start = nanotime_now();
		nanotime_sleep(NANOTIME_NONZERO_SLEEP);
		stepper->nonzero_sleep_overhead = nanotime_now() - start;
		if (stepper->nonzero_sleep_overhead < NANOTIME_NONZERO_SLEEP) {
			/*
			 * If the delay time was actually less than the
			 * requested delay time, the overhead is assumed to be
			 * zero.
			 */
			stepper->nonzero_sleep_overhead = UINT64_C(0);
		}
		else {
			stepper->nonzero_sleep_overhead -= NANOTIME_NONZERO_SLEEP;
		}
	}

	{
		/*
		 * Zero second delays with nanosleep yield the calling process
		 * to allow the OS to schedule other processes on the hardware
		 * thread for some time, passing control back to the calling
		 * process after the yield has completed; that behavior can be
		 * taken advantage of, to do minimum-time delays that don't
		 * incur high CPU usage and prevent the hardware thread from
		 * being used for other processes.
		 */
		const uint64_t start = nanotime_now();
		nanotime_sleep(UINT64_C(0));
		stepper->zero_sleep_duration = nanotime_now() - start;
	}

	stepper->accumulator = UINT64_C(0);
}

void nanotime_step_start(nanotime_step_data* const stepper) {
	stepper->sleep_point = stepper->now();
}

void nanotime_step(nanotime_step_data* const stepper) {
	if (stepper->accumulator < stepper->sleep_duration) {
		const uint64_t end = stepper->sleep_point + stepper->sleep_duration - stepper->accumulator;

		if (stepper->sleep_duration > stepper->zero_sleep_duration * UINT64_C(2)) {
			if (stepper->sleep_duration > stepper->nonzero_sleep_overhead) {
				uint64_t iter_sleep = stepper->sleep_duration >> 4;

				for (
					uint64_t max = UINT64_C(0);
					stepper->now() < end - max && iter_sleep > stepper->nonzero_sleep_overhead && iter_sleep >= NANOTIME_NONZERO_SLEEP;
					iter_sleep >>= 4
				) {
					uint64_t adjusted_sleep = iter_sleep - stepper->nonzero_sleep_overhead;
					max = UINT64_C(0);
					uint64_t start;
					while (max < stepper->sleep_duration && (start = stepper->now()) < end - max) {
						stepper->sleep(adjusted_sleep);
						uint64_t slept;
						if ((slept = stepper->now() - start) > max) {
							max = slept;
						}

						if (slept <= adjusted_sleep) {
							stepper->nonzero_sleep_overhead = UINT64_C(0);
							adjusted_sleep = iter_sleep;
						}
						else {
							stepper->nonzero_sleep_overhead = slept - adjusted_sleep;
							if (iter_sleep <= stepper->nonzero_sleep_overhead) {
								adjusted_sleep = UINT64_C(0);
							}
							else {
								adjusted_sleep = iter_sleep - stepper->nonzero_sleep_overhead;
							}
						}
					}
				}
			}

			uint64_t max = UINT64_C(0);
			uint64_t start;
			while ((start = stepper->now()) < end - max) {
				stepper->sleep(UINT64_C(0));
				if ((stepper->zero_sleep_duration = stepper->now() - start) > max) {
					max = stepper->zero_sleep_duration;
				}
			}
		}

		uint64_t current_time;
		while ((current_time = stepper->now()) < end);
		stepper->accumulator += current_time - stepper->sleep_point;
		stepper->sleep_point = current_time;
	}
	stepper->accumulator -= stepper->sleep_duration;
}

#ifdef __cplusplus
}
#endif
#endif

#endif /* _include_guard_nanotime_ */
