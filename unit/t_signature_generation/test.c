#include <sys/types.h>

#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "imaged.h"

static char	*testcontent = "hithere";
static size_t	 testcontentsize = 7;
static int	 testkey = 5;

int		 debug = 1, verbose = 1;

int
main()
{
	struct archive	*a;
	char		*signature, *signaturecopy;

	a = archive_new(testkey);
	if (a == NULL) err(1, "archive_new");

	if (archive_addfile(a, "testestesfds", testcontent, testcontentsize) < 0)
		errx(1, "archive_addfile: %s", archive_error(a));

	signature = crypto_takesignature(a);
	archive_writesignature(a, signature);

	signaturecopy = archive_getsignature(a);

	if (signaturecopy == NULL || strcmp(signature, signaturecopy) != 0)
		errx(1, "crypto_verifysignature: archive_getsignature failed");

	free(signature);
	free(signaturecopy);

	archive_teardown(a);
	return 0;
}
