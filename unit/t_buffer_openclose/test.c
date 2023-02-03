#include <sys/types.h>

#include <err.h>

#include "imaged.h"

int
main()
{
	int	first, second;

	first = buffer_open();
	if (first < 0) err(1, "failed to open first buffer");
		

	second = buffer_open();
	if (second < 0) err(1, "failed to open second buffer");

	if (first == second)
		errx(1, "first buffer descriptor == second buffer descriptor");

	if (buffer_close(first) < 0) err(1, "failed to close first buffer");
	if (buffer_close(second) < 0) err(1, "failed to close second buffer");

	return 0;
}
