#define NANOSLEEP_IMPLEMENTATION
#include "nanosleep.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

int main(int argc, char** argv) {
	double req_seconds = -1.0;
	if (argc <= 1 || sscanf_s(argv[1], "%lf", &req_seconds) != 1 || req_seconds < 0.0) {
		goto usage;
	}

	printf("Requested time to suspend (seconds): %.9lf\n", req_seconds);

	struct timespec req = {
		(time_t)req_seconds, (long)(fmod(req_seconds, 1.0) * 1000000000.0)
	};
	struct timespec rem = { 0 };
	struct timespec start;
	struct timespec end;

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

	usage:
	fprintf(stderr, "Usage: test_nanosleep [seconds]\n");
	fprintf(stderr, "[seconds] must be greater than or equal to 0.0.\n");
	fprintf(stderr, "Example, testing 1 millisecond suspension: test_nanosleep 0.001\n");

	return EXIT_FAILURE;
}
