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

// It seems this scheme of creating a render thread works as expected at least
// for the OpenGL backend on ARM64/x86_64 Linux, x64 Windows, and Apple Silicon
// macOS. TODO: Test on more platforms, then add the tested platforms here.
//
// The initial setup does as much as possible to ensure an OpenGL render driver
// is used, of course. And the initial setup fails gracefully if no OpenGL
// driver is available.
//
// The setup is accomplished via:
//
// 1. Initialize the window etc. first in the main thread.
//
// 2. Make the OpenGL context for the renderer not current in the main
// thread.
//
// 3. Create the render thread, relying upon the creation being a full memory
// barrier between the main thread and render thread.
//
// 4. Make the context current in the render thread before doing any render API
// calls in the render thread.
//
// 5. Make the context not current in the render thread before the render
// thread closes.
//
// 7. Wait-thread on the render thread in the main thread.
//
// 8. Make the context current in the main thread upon closure of the render
// thread, relying upon the closure being a full memory barrier between the
// main thread and render thread.
//
// 9. Destroy everything as usual in the main thread.
//
// Between making the context not current in the main thread and later making
// it current in the main thread before shutdown, no render APIs are called in
// the main thread.
//
// Perhaps some dance of sync between the threads is required if the main
// thread does some changes on the window object via non-render APIs (a mutex
// over the window object might suffice). Or it's outright unsafe to operate
// upon the window object in the main thread, while the context is current in
// the render thread, in which case the main thread would have to signal to the
// render thread to do the window operations, and window state would have to be
// read in the render thread then communicated to the main thread.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <math.h>

#define NANOTIME_IMPLEMENTATION
#include "nanotime.h"
#include "SDL.h"

#define TICK_RATE 60.0
#define TWO_PI (2.0 * M_PI)

static SDL_atomic_t quit_now = { 0 };
static SDL_atomic_t ticks = { 0 };

// We rely on thread create and wait being full memory barriers between a
// spawning thread and spawned thread, so no sync is required for these. And,
// since all writes of them are only done in the spawning thread (the main
// thread), all reads in the spawned thread (the render thread) are guaranteed
// valid.
static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;
static SDL_GLContext context = NULL;
static SDL_sem* render_wake = NULL;

static int SDLCALL render(void* data) {
	SDL_assert(window != NULL);
	SDL_assert(renderer != NULL);
	SDL_assert(context != NULL);
	SDL_assert(render_wake != NULL);

	if (SDL_GL_MakeCurrent(window, context) < 0) {
		SDL_MemoryBarrierRelease();
		SDL_AtomicSet(&quit_now, 1);
		return -1;
	}

	while (true) {
		if (SDL_SemWait(render_wake) < 0) {
			SDL_GL_MakeCurrent(window, NULL);
			SDL_MemoryBarrierRelease();
			SDL_AtomicSet(&quit_now, 1);
			return -2;
		}

		// quit_now is set true before waking this thread by the main
		// thread when the main thread determines it's time to quit, so
		// we always have to acquire to be sure we get the correct
		// value each wakeup that the main thread expects this thread
		// to observe.
		const int current_quit_now = SDL_AtomicGet(&quit_now);
		SDL_MemoryBarrierAcquire();
		if (current_quit_now) {
			break;
		}

		// We want to be sure the ticks count monotonically proceeds
		// through its range, not observing old values before new
		// values, so we have to acquire here.
		const int current_ticks = SDL_AtomicGet(&ticks);
		SDL_MemoryBarrierAcquire();

		const Uint8 shade = (Uint8)(((sin(TWO_PI * (current_ticks / TICK_RATE)) + 1.0) / 2.0) * 255.0);
		if (
			SDL_SetRenderDrawColor(renderer, shade, shade, shade, SDL_ALPHA_OPAQUE) < 0 ||
			SDL_RenderClear(renderer) < 0
		) {
			SDL_GL_MakeCurrent(window, NULL);
			SDL_MemoryBarrierRelease();
			SDL_AtomicSet(&quit_now, 1);
			return -3;
		}
		SDL_RenderPresent(renderer);
	}

	SDL_GL_MakeCurrent(window, NULL);
	return 0;
}

int main(int argc, char** argv) {
	if (SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO) < 0) {
		printf("SDL_Init failed\n");
		return EXIT_FAILURE;
	}

	const int num_render_drivers = SDL_GetNumRenderDrivers();
	if (num_render_drivers < 0) {
		printf("Failed to get number of render drivers\n");
		SDL_Quit();
		return EXIT_FAILURE;
	}

	int render_driver;
	for (render_driver = 0; render_driver < num_render_drivers; render_driver++) {
		SDL_RendererInfo info;
		if (SDL_GetRenderDriverInfo(render_driver, &info) < 0) {
			printf("Failed to get render driver info for index %d\n", render_driver);
			SDL_Quit();
			return EXIT_FAILURE;
		}
		if (!strncmp(info.name, "opengl", 6)) {
			break;
		}
	}
	if (render_driver == num_render_drivers) {
		printf("Failed to find some OpenGL render driver, which is required\n");
		SDL_Quit();
		return EXIT_FAILURE;
	}

	SDL_MemoryBarrierRelease();
	SDL_AtomicSet(&quit_now, 0);
	SDL_MemoryBarrierRelease();
	SDL_AtomicSet(&ticks, 0);

	window = SDL_CreateWindow("Fixed timestep test", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
	if (!window) {
		printf("SDL_CreateWindow failed\n");
		SDL_Quit();
		return EXIT_FAILURE;
	}

	renderer = SDL_CreateRenderer(window, render_driver, SDL_RENDERER_ACCELERATED);
	if (!renderer) {
		printf("SDL_CreateRenderer failed\n");
		SDL_DestroyWindow(window);
		SDL_Quit();
		return EXIT_FAILURE;
	}

	context = SDL_GL_GetCurrentContext();
	if (context == NULL || SDL_GL_MakeCurrent(window, NULL) < 0) {
		printf("Context release failed, context is %s\n", context == NULL ? "NULL" : "not NULL");
		SDL_DestroyRenderer(renderer);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return EXIT_FAILURE;
	}

	render_wake = SDL_CreateSemaphore(0);
	if (!render_wake) {
		printf("Failed to create render_wake semaphore\n");
		SDL_DestroyRenderer(renderer);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return EXIT_FAILURE;
	}

	SDL_Thread* const render_thread = SDL_CreateThread(render, "render_thread", NULL);
	if (!render_thread) {
		printf("Failed to create the render thread\n");
		SDL_DestroySemaphore(render_wake);
		if (SDL_GL_MakeCurrent(window, context) >= 0) {
			SDL_DestroyRenderer(renderer);
			SDL_DestroyWindow(window);
		}
		else {
			printf("Failed to make context current in main thread\n");
		}
		SDL_Quit();
		return EXIT_FAILURE;
	}

	nanotime_step_data stepper;

#ifdef REALTIME
	SDL_SetHint(SDL_HINT_THREAD_FORCE_REALTIME_TIME_CRITICAL, "1");
	SDL_SetThreadPriority(SDL_THREAD_PRIORITY_TIME_CRITICAL);
#endif

	nanotime_step_init(&stepper, (uint64_t)(NANOTIME_NSEC_PER_SEC / TICK_RATE), nanotime_now_max(), nanotime_now, nanotime_sleep);
	uint64_t last_point = stepper.sleep_point;
	uint64_t sleep_total = 0;
	uint64_t num_ticks = 0;

	int status = 0;
	while (true) {
		const int current_quit_now = SDL_AtomicGet(&quit_now);
		SDL_MemoryBarrierAcquire();
		if (current_quit_now) {
			goto end;
		}

		SDL_PumpEvents();
		SDL_Event event;
		while ((status = SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT)) > 0) {
			if (event.type == SDL_QUIT) {
				goto end;
			}
			if (event.type == SDL_KEYDOWN) {
				sleep_total = 0;
				num_ticks = 0;
			}
		}
		if (status < 0) {
			printf("Error while dequeueing events\n");
			SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
			goto end;
		}

		if (SDL_AtomicGet(&ticks) == (int)TICK_RATE - 1) {
			SDL_MemoryBarrierRelease();
			SDL_AtomicSet(&ticks, 0);
		}
		else {
			SDL_MemoryBarrierRelease();
			SDL_AtomicIncRef(&ticks);
		}

		if (SDL_SemPost(render_wake) < 0) {
			printf("Error waking render thread during main thread ticks\n");
			abort();
		}

		nanotime_step(&stepper);

		const uint64_t current_sleep = nanotime_interval(last_point, stepper.sleep_point, stepper.now_max);
		sleep_total += current_sleep;
		num_ticks++;
		printf("%" PRIu64 " ns/tick current, %" PRIu64 " ns/tick average, %" PRId64 " ns off, accumulated %" PRIu64 " ns\n",
			current_sleep,
			sleep_total / num_ticks,
			(int64_t)(current_sleep - stepper.sleep_duration),
			stepper.accumulator
		);
		fflush(stdout);
		last_point = stepper.sleep_point;
	}

end:
	SDL_MemoryBarrierRelease();
	SDL_AtomicSet(&quit_now, 1);

	if (SDL_SemPost(render_wake) < 0) {
		printf("Error waking render thread at quit\n");
		abort();
	}
	SDL_WaitThread(render_thread, &status);
	switch (status) {
	default:
		if (status != 0) {
			printf("Invalid status value returned from render thread of %d\n", status);
			abort();
		}
		break;

	case -1:
		printf("Error making context current in render thread\n");
		break;

	case -2:
		printf("Error waiting on semaphore in render thread\n");
		abort();

	case -3:
		printf("Error rendering in render thread\n");
		break;
	}

	if (SDL_GL_MakeCurrent(window, context) < 0) {
		printf("Error making context current at quit\n");
		abort();
	}
	SDL_DestroySemaphore(render_wake);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return EXIT_SUCCESS;
}
