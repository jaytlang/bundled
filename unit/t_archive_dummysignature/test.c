#include <sys/types.h>

#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "imaged.h"

#define TEST_SIGNATURE	"hello, world, i am jay"

static size_t		datasize = 10000;
static uint32_t		key = 65535;
int			debug = 1, verbose = 1;

static char	*maketestdata(char);

static char *
maketestdata(char a)
{
	char	*buf;

	buf = reallocarray(NULL, datasize, sizeof(char));
	if (buf == NULL) err(1, "reallocarray");

	memset(buf, a, datasize);
	return buf;
}

int
main()
{
	struct archive	*a;
	char		*afdata, *bfdata, *e;

	char		*newsignature;

	a = archive_new(key);
	if (a == NULL) err(1, "archive_new");

	afdata = maketestdata('a');
	bfdata = maketestdata('b');

	if (archive_addfile(a, "filea", afdata, datasize) < 0) {
		e = archive_error(a);
		archive_teardown(a);
		errx(1, "archive_addfile: %s", e);
	}

	if (archive_addfile(a, "fileb", bfdata, datasize) < 0) {
		e = archive_error(a);
		archive_teardown(a);
		errx(1, "archive_addfile: %s", e);
	}

	newsignature = archive_getsignature(a);
	if (*newsignature != '\0') {
		archive_teardown(a);
		errx(1, "archive_getsignature returned non-empty signature before signing");
	}

	free(newsignature);
	archive_writesignature(a, TEST_SIGNATURE);
	newsignature = archive_getsignature(a);

	if (strcmp(newsignature, TEST_SIGNATURE) != 0) {
		archive_teardown(a);
		errx(1, "signature does not match originally written signature");
	}

	free(newsignature);

	if (!archive_isvalid(a)) {
		e = archive_error(a);
		archive_teardown(a);
		errx(1, "archive_isvalid: %s", e);
	}

	free(afdata);
	free(bfdata);
	archive_teardown(a);
}
