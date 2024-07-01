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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <math.h>

#define NANOTIME_IMPLEMENTATION
#include "nanotime.h"
#include "SDL.h"

#define LOGIC_RATE 60.0

#define FRAME_RATE 120.0

static SDL_atomic_t quit_now;
static SDL_atomic_t reset_average;

static SDL_mutex* logic_mutex;

/*
 * logic_data[0] is owned by the logic thread and doesn't need locking,
 * logic_data[1] is shared so must be locked, but try-lock is used for
 * logic_data[1] to avoid ever blocking the logic thread, since it's acceptable
 * for the logic data accessed in the main thread to be slightly outdated.
 */
struct {
	uint64_t update_measured;
	uint64_t update_sleep_total;
	uint64_t accumulator;
	uint64_t num_updates;
} logic_data[2] = {0};

static void update_logic(const uint64_t last_sleep_point, nanotime_step_data* const stepper) {
	logic_data[0].update_measured = nanotime_interval(last_sleep_point, stepper->sleep_point, nanotime_now_max());
	if (SDL_AtomicCAS(&reset_average, 1, 0)) {
		logic_data[0].update_sleep_total = 0;
		logic_data[0].num_updates = 0;
	}
	logic_data[0].update_sleep_total += logic_data[0].update_measured;
	logic_data[0].accumulator = stepper->accumulator;
	logic_data[0].num_updates++;

	if (SDL_TryLockMutex(logic_mutex) == 0) {
		logic_data[1] = logic_data[0];
		SDL_UnlockMutex(logic_mutex);
	}
}

#ifdef MULTITHREADED
static int SDLCALL update_logic_thread_function(void* data) {
#ifdef REALTIME
	SDL_SetHint(SDL_HINT_THREAD_FORCE_REALTIME_TIME_CRITICAL, "1");
	SDL_SetThreadPriority(SDL_THREAD_PRIORITY_TIME_CRITICAL);
#endif

	nanotime_step_data stepper;
	nanotime_step_init(&stepper, (uint64_t)(NANOTIME_NSEC_PER_SEC / LOGIC_RATE), nanotime_now_max(), nanotime_now, nanotime_sleep);
	while (!SDL_AtomicGet(&quit_now)) {
		const uint64_t last_sleep_point = stepper.sleep_point;
		nanotime_step(&stepper);
		update_logic(last_sleep_point, &stepper);
	}

	return 0;
}
#endif

int main(int argc, char** argv) {
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		return EXIT_FAILURE;
	}

	SDL_Window* const window = SDL_CreateWindow("test_nanotime_step", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_SHOWN);
	if (!window) {
		SDL_Quit();
		return EXIT_FAILURE;
	}
	SDL_Renderer* const renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	if (!renderer) {
		SDL_DestroyWindow(window);
		SDL_Quit();
		return EXIT_FAILURE;
	}

	const double two_pi = 8.0 * atan(1.0);

	SDL_AtomicSet(&quit_now, 0);
	SDL_AtomicSet(&reset_average, 0);

	logic_mutex = SDL_CreateMutex();
	if (!logic_mutex) {
		SDL_DestroyRenderer(renderer);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return EXIT_FAILURE;
	}

#ifdef MULTITHREADED
	SDL_Thread* const logic_thread = SDL_CreateThread(update_logic_thread_function, "logic_thread", NULL);
	if (!logic_thread) {
		SDL_DestroyMutex(logic_mutex);
		SDL_DestroyRenderer(renderer);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return EXIT_FAILURE;
	}
#endif

	nanotime_step_data stepper;

#ifdef REALTIME
	SDL_SetHint(SDL_HINT_THREAD_FORCE_REALTIME_TIME_CRITICAL, "1");
	SDL_SetThreadPriority(SDL_THREAD_PRIORITY_TIME_CRITICAL);
#endif

#ifdef MULTITHREADED
	nanotime_step_init(&stepper, (uint64_t)(NANOTIME_NSEC_PER_SEC / FRAME_RATE), nanotime_now_max(), nanotime_now, nanotime_sleep);
#else
	nanotime_step_init(&stepper, (uint64_t)(NANOTIME_NSEC_PER_SEC / LOGIC_RATE), nanotime_now_max(), nanotime_now, nanotime_sleep);
#endif

	// The SDL2 documentation says that for maximally-portable code, video
	// and events should be handled only in the main thread. Additionally,
	// stdio should only be used in the main thread, as it's not guaranteed
	// that using stdio in a non-main thread is safe.
	while (true) {
#ifndef MULTITHREADED
		uint64_t last_sleep_point = stepper.sleep_point;
#endif
		int status;

		// This animation code is time-based, so it's independent of
		// the frame rate.
		const Uint8 shade = (Uint8)(((sin(two_pi * ((double)(nanotime_now() % NANOTIME_NSEC_PER_SEC) / NANOTIME_NSEC_PER_SEC)) + 1.0) / 2.0) * 255.0);
		if (
			SDL_SetRenderDrawColor(renderer, shade, shade, shade, SDL_ALPHA_OPAQUE) < 0 ||
			SDL_RenderClear(renderer) < 0
		) {
			SDL_AtomicSet(&quit_now, 1);
#ifdef MULTITHREADED
			SDL_WaitThread(logic_thread, NULL);
#endif
			SDL_DestroyMutex(logic_mutex);
			SDL_DestroyRenderer(renderer);
			SDL_DestroyWindow(window);
			SDL_Quit();
			return EXIT_FAILURE;
		}

		// Just pretend this is rendering to the screen; it's still in
		// the right place if it were render code.
		status = SDL_TryLockMutex(logic_mutex);
		if (status == 0) {
			if (logic_data[1].num_updates > UINT64_C(0)) {
				printf("%" PRIu64 " ns/frame current, %" PRIu64 " ns/frame average, %" PRId64 " ns off, accumulated %" PRIu64 " ns\n",
					logic_data[1].update_measured,
					logic_data[1].update_sleep_total / logic_data[1].num_updates,
					(int64_t)logic_data[1].update_measured - (int64_t)(NANOTIME_NSEC_PER_SEC / LOGIC_RATE),
					logic_data[1].accumulator
				);
				fflush(stdout);
			}
			if (SDL_UnlockMutex(logic_mutex) == -1) {
				SDL_AtomicSet(&quit_now, 1);
#ifdef MULTITHREADED
				SDL_WaitThread(logic_thread, NULL);
#endif
				SDL_DestroyMutex(logic_mutex);
				SDL_DestroyRenderer(renderer);
				SDL_DestroyWindow(window);
				SDL_Quit();
				return EXIT_FAILURE;
			}
		}
		else if (status == -1) {
			SDL_AtomicSet(&quit_now, 1);
#ifdef MULTITHREADED
			SDL_WaitThread(logic_thread, NULL);
#endif
			SDL_DestroyMutex(logic_mutex);
			SDL_DestroyRenderer(renderer);
			SDL_DestroyWindow(window);
			SDL_Quit();
			return EXIT_FAILURE;
		}

		SDL_RenderPresent(renderer);

		// The timestep should be here, followed by input, as the
		// player should be given as much time as possible to react to
		// screen updates.
		nanotime_step(&stepper);
#ifndef MULTITHREADED
		update_logic(last_sleep_point, &stepper);
#endif
		SDL_PumpEvents();
		SDL_Event event;
		bool quit_loop = false;
		while ((status = SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT)) > 0) {
			if (event.type == SDL_QUIT) {
				quit_loop = true;
			}
			else if (event.type == SDL_KEYDOWN) {
				SDL_AtomicSet(&reset_average, 1);
			}
		}
		if (quit_loop) {
			SDL_AtomicSet(&quit_now, 1);
			break;
		}
		else if (status < 0) {
			SDL_AtomicSet(&quit_now, 1);
#ifdef MULTITHREADED
			SDL_WaitThread(logic_thread, NULL);
			SDL_DestroyMutex(logic_mutex);
#endif
			SDL_DestroyRenderer(renderer);
			SDL_DestroyWindow(window);
			SDL_Quit();
			return EXIT_FAILURE;
		}
	}

#ifdef MULTITHREDED
	SDL_WaitThread(logic_thread, NULL);
	SDL_DestroyMutex(logic_mutex);
#endif
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return EXIT_SUCCESS;
}
