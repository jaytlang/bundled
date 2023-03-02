#include <sys/types.h>

#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bundled.h"

static char	*testcontent = "hello";
static size_t	 testcontentsize = 5;
static int	 testkey = 5;

int		 debug = 1, verbose = 1;

int
main()
{
	struct archive	*a;
	char		*signature;

	config_parse("bundled.conf");

	a = archive_new(testkey);
	if (a == NULL) err(1, "archive_new");

	if (archive_addfile(a, "tester5555", testcontent, testcontentsize) < 0)
		errx(1, "archive_addfile: %s", archive_error(a));

	signature = crypto_takesignature(a);
	archive_writesignature(a, signature);

	if (crypto_verifysignature(a) < 0)
		errx(1, "crypto_verifysignature: incorrect signature");

	signature = crypto_takesignature(a);
	signature[strlen(signature) / 2] = 'A';
	archive_writesignature(a, signature);

	if (crypto_verifysignature(a) == 0)
		errx(1, "crypto_verifysignature: incorrect signature was verified");

	free(signature);
	archive_teardown(a);
	return 0;
}
