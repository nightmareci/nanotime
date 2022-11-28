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

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <cerrno>

#define NANOTIME_IMPLEMENTATION
#include "nanotime.h"

int main(int argc, char** argv) {
	double req_seconds = -1.0;
	if (argc <= 1 || sscanf(argv[1], "%lf", &req_seconds) != 1 || req_seconds < 0.0) {
		fprintf(stderr, "Usage: test_nanosleep [seconds]\n");
		fprintf(stderr, "[seconds] must be greater than or equal to 0.0.\n");
		fprintf(stderr, "Example, testing 1 millisecond suspension: test_nanosleep 0.001\n");
		return EXIT_FAILURE;
	}

	printf("Requested time to suspend (seconds): %.9lf\n", req_seconds);

	const timespec req = {
		(time_t)req_seconds, (long)(fmod(req_seconds, 1.0) * 1000000000.0)
	};
	timespec rem = { 0 };
	timespec start;
	timespec end;

	(void)timespec_get(&start, TIME_UTC);
	const int status = nanosleep(&req, &rem);
	(void)timespec_get(&end, TIME_UTC);

	const double elapsed_seconds =
		(double)(end.tv_sec  - start.tv_sec ) +
		(double)(end.tv_nsec - start.tv_nsec) / 1000000000.0;

	printf("Suspended time (seconds): %.9lf\n", elapsed_seconds);
	if (status == -1 && errno == EINTR) {
		const double rem_seconds =
			(double)rem.tv_sec +
			(double)rem.tv_nsec / 1000000000.0;
		printf("Remaining suspension time (seconds): %.9lf\n", rem_seconds);
	}
	else {
		printf("No remaining suspension time.\n");
	}

	return EXIT_SUCCESS;
}
