#include <sys/types.h>

#include <err.h>
#include <string.h>

#include "bundled.h"

int
main()
{
	config_parse("parses.txt");

	if (MAXNAMESIZE != 1024)
		errx(1, "1K did not parse correctly");

	if (MAXFILESIZE != 1048576 * 50)
		errx(1, "50M did not parse correctly");

	if (strcmp(ARCHIVES, "/var/bundled/archives") != 0)
		errx(1, "borked path did not parse correctly");

	if (strcmp(USER, "jaytlang") != 0)
		errx(1, "user did not parse correctly");

	return 0;
}
