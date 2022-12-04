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
#include <math.h>

#define NANOTIME_IMPLEMENTATION
#include "nanotime.h"
#include "SDL.h"

#define LOGIC_RATE 60.0

#define FRAME_RATE 120.0

SDL_atomic_t quit_now;

int update_logic(void* data) {
	uint64_t num_updates = UINT64_C(0);
	uint64_t update_sleep_total = UINT64_C(0);

	nanotime_step_data stepper;
	nanotime_step_init(&stepper, (uint64_t)(NSEC_PER_SEC / LOGIC_RATE), nanotime_now, nanotime_sleep);
	while (!SDL_AtomicGet(&quit_now)) {
		const uint64_t last_sleep_point = stepper.sleep_point;
		nanotime_step(&stepper);

		const uint64_t update_measured = stepper.sleep_point - last_sleep_point;
		num_updates++;
		update_sleep_total += update_measured;
		#if 1
		printf("%.9lfs FPS current, %.9lf FPS average, %.9lf seconds off, accumulated %.9lf seconds\n",
			(double)NSEC_PER_SEC / update_measured,
			(double)NSEC_PER_SEC / (update_sleep_total / num_updates),
			update_measured / (double)NSEC_PER_SEC - 1.0 / LOGIC_RATE,
			stepper.accumulator / (double)NSEC_PER_SEC
		);
		fflush(stdout);
		#endif
	}

	return 0;
}

int main(int argc, char** argv) {
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		return EXIT_FAILURE;
	}

	SDL_Window* const window = SDL_CreateWindow("Fixed timestep test", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_SHOWN);
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

	SDL_Thread* const logic_thread = SDL_CreateThread(update_logic, "logic_thread", NULL);
	if (!logic_thread) {
		SDL_DestroyRenderer(renderer);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return EXIT_FAILURE;
	}

	nanotime_step_data stepper;
	nanotime_step_init(&stepper, (uint64_t)(NSEC_PER_SEC / FRAME_RATE), nanotime_now, nanotime_sleep);

	// The SDL2 documentation says that for maximally-portable code, video
	// and events should be handled only in the main thread.
	while (!SDL_AtomicGet(&quit_now)) {
		// This animation code is time-based, so it's independent of
		// the frame rate.
		const Uint8 shade = (Uint8)(((sin(two_pi * ((double)(nanotime_now() % NSEC_PER_SEC) / NSEC_PER_SEC)) + 1.0) / 2.0) * 255.0);
		if (
			SDL_SetRenderDrawColor(renderer, shade, shade, shade, SDL_ALPHA_OPAQUE) < 0 ||
			SDL_RenderClear(renderer) < 0
		) {
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
		SDL_PumpEvents();
		if (SDL_PeepEvents(NULL, 0, SDL_PEEKEVENT, SDL_QUIT, SDL_QUIT) > 0) {
			SDL_AtomicSet(&quit_now, 1);
		}
	}

	SDL_WaitThread(logic_thread, NULL);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return EXIT_SUCCESS;
}
