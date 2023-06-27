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

#if defined(_MSC_VER)
	#if (_MSC_VER < 1600)
		#error "Current Visual Studio version is not at least Visual Studio 2010, the nanotime library requires at least 2010."
	#endif
#elif defined(__cplusplus)
	#if (__cplusplus < 201103L)
		#error "Current C++ standard is not at least C++11, the nanotime library requires at least C++11."
	#endif
#elif defined(__STDC_VERSION__)
	#if (__STDC_VERSION__ < 199901L)
		#error "Current C standard is not at least C99, the nanotime library requires at least C99."
	#endif
#else
	#error "Current C or C++ standard is unknown, the nanotime library requires stdint.h and stdbool.h to be available (C99 or higher, C++11 or higher, Visual Studio 2010 or higher)."
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#define NANOTIME_NSEC_PER_SEC 1000000000L

#ifndef NANOTIME_ONLY_STEP

/*
 * Returns the current time since some unspecified epoch. With the exception of
 * the standard C11 implementation and non-Apple/Mach kernel POSIX
 * implementation when neither CLOCK_MONOTONIC_RAW nor CLOCK_MONOTONIC are
 * available, the time values monotonically increase, so they're not equivalent
 * to calendar time (i.e., no leap seconds are accounted for, etc.). Calendar
 * time has to be used as a last resort sometimes, as monotonic time isn't
 * always available.
 */
uint64_t nanotime_now();

/*
 * Sleeps the current thread for the requested count of nanoseconds. The slept
 * duration may be less than, equal to, or greater than the time requested.
 */
void nanotime_sleep(uint64_t nsec_count);

#endif

#ifndef NANOTIME_ONLY_STEP

/*
 * The yield function is provided for some platforms, but in the case of
 * unknown platforms, the function is defined as a no-op.
 */

#ifdef _MSC_VER
#include <Windows.h>
#define nanotime_yield() YieldProcessor()
#define NANOTIME_YIELD_IMPLEMENTED
#elif (defined(__unix__) || defined(__APPLE__) || defined(__MINGW32__) || defined(__MINGW64__)) && (_POSIX_VERSION >= 200112L)
#include <sched.h>
#define nanotime_yield() (void)sched_yield()
#define NANOTIME_YIELD_IMPLEMENTED
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L) && !defined(__STDC_NO_THREADS__)
#include <threads.h>
#define nanotime_yield() thrd_yield()
#define NANOTIME_YIELD_IMPLEMENTED
#elif defined(__cplusplus)
extern void (* const nanotime_yield)();
#else
#define nanotime_yield()
#define NANOTIME_YIELD_NOP
#define NANOTIME_YIELD_IMPLEMENTED
#endif

#endif

typedef struct nanotime_step_data {
	uint64_t sleep_duration;
	uint64_t (* now)();
	void (* sleep)(uint64_t nsec_count);

	uint64_t overhead_duration;
	uint64_t zero_sleep_duration;
	uint64_t accumulator;
	uint64_t sleep_point;
} nanotime_step_data;

/*
 * Initializes the nanotime precise fixed timestep object. Call immediately
 * before entering the loop using the stepper object.
 */
void nanotime_step_init(nanotime_step_data* const stepper, const uint64_t sleep_duration, uint64_t (* const now)(), void (* const sleep)(uint64_t nsec_count));

/*
 * Does one step of sleeping for a fixed timestep logic update cycle. It makes
 * a best-attempt at a precise delay per iteration, but might skip a cycle of
 * sleeping if skipping sleeps is required to catch up to the correct
 * wall-clock time. Returns true if a sleep up to the latest target sleep end
 * time occurred, otherwise returns false in the case of a sleep step skip.
 */
bool nanotime_step(nanotime_step_data* const stepper);

#ifdef _MSC_VER
#define NANOTIME_RESOLUTION UINT64_C(100)

#else
#define NANOTIME_RESOLUTION UINT64_C(1)

#endif

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
	static uint64_t scale = UINT64_C(0);
	static bool multiply;
	if (scale == 0u) {
		LARGE_INTEGER frequency;
		QueryPerformanceFrequency(&frequency);
		if (frequency.QuadPart < NANOTIME_NSEC_PER_SEC) {
			scale = NANOTIME_NSEC_PER_SEC / frequency.QuadPart;
			multiply = true;
		}
		else {
			scale = frequency.QuadPart / NANOTIME_NSEC_PER_SEC;
			multiply = false;
		}
	}
	LARGE_INTEGER performanceCount;
	QueryPerformanceCounter(&performanceCount);
	if (multiply) {
		return performanceCount.QuadPart * scale;
	}
	else {
		return performanceCount.QuadPart / scale;
	}
}
#define NANOTIME_NOW_IMPLEMENTED
#endif

#ifndef NANOTIME_SLEEP_IMPLEMENTED
void nanotime_sleep(uint64_t nsec_count) {
	LARGE_INTEGER dueTime;

	if (nsec_count < NANOTIME_RESOLUTION) {
		/*
		 * Allows the OS to schedule another process for a single time
		 * slice. Better than a delay of 0, which immediately returns
		 * with no actual non-CPU-hogging delay. The time-slice-yield
		 * behavior is specified in Microsoft's Windows documentation.
		 */
		SleepEx(0UL, FALSE);
	}
	else {
		HANDLE timer = NULL;
		if (
#ifdef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
			/*
			 * Requesting a high resolution timer can make quite the
			 * difference, so always request high resolution if available. It's
			 * available in Windows 10 1803 and above. This arrangement of
			 * building it if the build system supports it will allow the
			 * executable to use high resolution if available on a user's
			 * system, but revert to low resolution if the user's system
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

		CloseHandle(timer);
	}
}
#define NANOTIME_SLEEP_IMPLEMENTED
#endif

#endif

#if (defined(__APPLE__) || defined(__MACH__)) && !defined(NANOTIME_NOW_IMPLEMENTED)
// The current platform is some Apple operating system, or at least uses some
// Mach kernel. The POSIX implementation below using clock_gettime works on at
// least Apple platforms, though this version using Mach functions has lower
// overhead.
#include <mach/mach_time.h>
uint64_t nanotime_now() {
	static double scale = 0.0;
	if (scale == 0.0) {
		mach_timebase_info_data_t info;
		const kern_return_t status = mach_timebase_info(&info);
		assert(status == KERN_SUCCESS);
		if (status != KERN_SUCCESS) {
			return UINT64_C(0);
		}
		scale = (double)info.numer / info.denom;
	}
	return (uint64_t)(mach_absolute_time() * scale);
}
#define NANOTIME_NOW_IMPLEMENTED
#endif

#if (defined(__unix__) || defined(__MINGW32__) || defined(__MINGW64__)) && !defined(NANOTIME_NOW_IMPLEMENTED)
// Current platform is some version of POSIX, that might have clock_gettime.
#include <unistd.h>
#if _POSIX_VERSION >= 199309L
#include <time.h>
#include <errno.h>
uint64_t nanotime_now() {
	struct timespec now;
	const int status = clock_gettime(
		#if defined(CLOCK_MONOTONIC_RAW)
		// Monotonic raw is more precise, but not always available. For the
		// sorts of applications this code is intended for, mainly soft real
		// time applications such as game programming, the subtle
		// inconsistencies of it vs. monotonic aren't an issue.
		CLOCK_MONOTONIC_RAW
		#elif defined(CLOCK_MONOTONIC)
		// Monotonic is quite good, and widely available, but not as precise as
		// monotonic raw, so it's only used if required.
		CLOCK_MONOTONIC
		#else
		// Realtime isn't fully correct, as it's calendar time, but is even more
		// widely available than monotonic. Monotonic is only unavailable on
		// very old platforms though, so old they're likely unused now (as of
		// last editing this, 2023).
		CLOCK_REALTIME
		#endif
	, &now);
	assert(status == 0 || (status == -1 && errno != EOVERFLOW));
	if (status == 0 || (status == -1 && errno != EOVERFLOW)) {
		return (uint64_t)now.tv_sec * (uint64_t)NANOTIME_NSEC_PER_SEC + (uint64_t)now.tv_nsec;
	}
	else {
		return UINT64_C(0);
	}
}
#define NANOTIME_NOW_IMPLEMENTED

#else
#error "Current platform is UNIX/POSIX, but doesn't support required functionality (IEEE Std 1003.1b-1993 is required)."
#endif
#endif

#if (defined(__unix__) || (defined(__APPLE__) && defined(__MACH__)) || defined(__MINGW32__) || defined(__MINGW64__)) && !defined(NANOTIME_SLEEP_IMPLEMENTED)
#include <unistd.h>
#include <time.h>
#include <errno.h>
void nanotime_sleep(uint64_t nsec_count) {
	const struct timespec req = {
		.tv_sec = (time_t)(nsec_count / NANOTIME_NSEC_PER_SEC),
		.tv_nsec = (long)(nsec_count % NANOTIME_NSEC_PER_SEC)
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
	if (status == TIME_UTC) {
		return (uint64_t)now.tv_sec * (uint64_t)NANOTIME_NSEC_PER_SEC + (uint64_t)now.tv_nsec;
	}
	else {
		return UINT64_C(0);
	}
}
#define NANOTIME_NOW_IMPLEMENTED
#endif
#endif

#ifndef NANOTIME_SLEEP_IMPLEMENTED
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L) && !defined(__STDC_NO_THREADS__)
#include <threads.h>
void nanotime_sleep(uint64_t nsec_count) {
	const struct timespec req = {
		.tv_sec = (time_t)(nsec_count / NANOTIME_NSEC_PER_SEC),
		.tv_nsec = (long)(nsec_count % NANOTIME_NSEC_PER_SEC)
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

#if !defined(NANOTIME_ONLY_STEP) && defined(NANOTIME_IMPLEMENTATION) && defined(__cplusplus)

#if !defined(NANOTIME_YIELD_IMPLEMENTED)
#include <thread>
extern "C" void (* const nanotime_yield)() = std::this_thread::yield;
#define NANOTIME_YIELD_IMPLEMENTED
#endif

#ifndef NANOTIME_NOW_IMPLEMENTED
#include <cstdint>
#include <chrono>
extern "C" uint64_t nanotime_now() {
	return static_cast<uint64_t>(
		std::chrono::time_point_cast<std::chrono::nanoseconds>(
			std::chrono::steady_clock::now()
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

#if !defined(NANOTIME_ONLY_STEP) && defined(NANOTIME_IMPLEMENTATION) 

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

void nanotime_step_init(nanotime_step_data* const stepper, const uint64_t sleep_duration, uint64_t (* const now)(), void (* const sleep)(uint64_t nsec_count)) {
	stepper->sleep_duration = sleep_duration;
	stepper->now = now;
	stepper->sleep = sleep;

	const uint64_t start = now();
	nanotime_sleep(UINT64_C(0));
	stepper->zero_sleep_duration = now() - start;
	stepper->overhead_duration = UINT64_C(0);
	stepper->accumulator = UINT64_C(0);

	// This should be last here, so the sleep point is close to what it
	// should be.
	stepper->sleep_point = now();
}

bool nanotime_step(nanotime_step_data* const stepper) {
	bool slept;
	if (stepper->accumulator < stepper->sleep_duration) {
		uint64_t current_sleep_duration = stepper->sleep_duration - stepper->accumulator;
		const uint64_t end = stepper->sleep_point + current_sleep_duration;
		const uint64_t shift = UINT64_C(4);

		#ifdef __APPLE__
		// Start with a big sleep. This helps reduce CPU/power use on
		// macOS vs. many shorter sleeps. Shorter sleeps are still done
		// below, but this reduces the number of shorter sleeps. The
		// overhead multiplier helps reduce the frequency of
		// overshooting the target end time.
		const uint64_t overhead_start = stepper->now();
		// TODO: This was carefully tuned to be well-behaved on Apple
		// Silicon M1, but hasn't been tested on any Intel Mac; test on
		// an Intel Mac to see if this multiplier is appropriate there,
		// if not, special-case for each CPU type.
		const uint64_t overhead_multiplier = UINT64_C(100);
		if (current_sleep_duration > stepper->overhead_duration * overhead_multiplier) {
			const uint64_t overhead_sleep_duration = current_sleep_duration - stepper->overhead_duration * overhead_multiplier;
			stepper->sleep(overhead_sleep_duration);
			const uint64_t overhead_end = stepper->now();
			if (overhead_end - overhead_start > overhead_sleep_duration) {
				stepper->overhead_duration = (overhead_end - overhead_start) - overhead_sleep_duration;
			}
			else {
				stepper->overhead_duration = UINT64_C(0);
			}
		}
		else {
			stepper->overhead_duration = UINT64_C(0);
		}

		if (stepper->now() - overhead_start < current_sleep_duration && stepper->now() < end) {
			current_sleep_duration -= stepper->now() - overhead_start;
		}
		else {
			goto step_end;
		}
		#endif

		// This has the flavor of Zeno's dichotomous paradox of motion,
		// as it successively divides the time remaining to sleep, but
		// attempts to stop short of the deadline to hopefully be able
		// to precisely sleep up to the deadline below this loop. The
		// divisor is larger than two though, as it produces better
		// behavior, and seems to work fine in testing on real
		// hardware.
		current_sleep_duration >>= shift;
		for (
			uint64_t max = stepper->zero_sleep_duration;
			stepper->now() + max < end && current_sleep_duration > UINT64_C(0);
			current_sleep_duration >>= shift
		) {
			max = stepper->zero_sleep_duration;
			uint64_t start;
			while (max < stepper->sleep_duration && (start = stepper->now()) + max < end) {
				stepper->sleep(current_sleep_duration);
				const uint64_t end_sleep = stepper->now();
				if (end_sleep - start > current_sleep_duration) {
					stepper->overhead_duration = (end_sleep - start) - current_sleep_duration;
				}
				else {
					stepper->overhead_duration = UINT64_C(0);
				}
				uint64_t slept_duration;
				if ((slept_duration = stepper->now() - start) > max) {
					max = slept_duration;
				}
			}
		}

		{
			// After (hopefully) stopping short of the deadline by a small
			// amount, do small sleeps here to get closer to the deadline,
			// but again attempting to stop short by an even smaller
			// amount. It's best to do larger sleeps as done in the above
			// loop, to reduce CPU/power usage, as each sleep call has a
			// CPU/power usage cost.
			uint64_t max = stepper->zero_sleep_duration;
			uint64_t start;
			while ((start = stepper->now()) + max < end) {
				stepper->sleep(UINT64_C(0));
				if ((stepper->zero_sleep_duration = stepper->now() - start) > max) {
					max = stepper->zero_sleep_duration;
				}
			}
		}

		step_end: {
			// Finally, do a busyloop to precisely sleep up to the
			// deadline. The code above this loop attempts to reduce the
			// remaining time to sleep to a minimum via process-yielding
			// sleeps, so the amount of time spent spinning here is
			// hopefully quite low.
			uint64_t current_time;
			while ((current_time = stepper->now()) < end);

			stepper->accumulator += current_time - stepper->sleep_point;
			stepper->sleep_point = current_time;
			slept = true;
		}
	}
	else {
		slept = false;
	}
	stepper->accumulator -= stepper->sleep_duration;
	return slept;
}

#ifdef __cplusplus
}
#endif
#endif

#endif /* _include_guard_nanotime_ */