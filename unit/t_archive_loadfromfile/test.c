#include <sys/types.h>

#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "imaged.h"

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
	struct archive	*a, *wa;
	char		*fdata, *e;

	char		*nfdata;
	size_t		 ndatasize;

	a = archive_new(key);
	if (a == NULL) err(1, "archive_new");

	fdata = maketestdata('v');

	if (archive_addfile(a, "testfile", fdata, datasize) < 0) {
		e = archive_error(a);
		archive_teardown(a);
		errx(1, "archive_addfile: %s", e);
	}

	if (archive_addfile(a, "testdir/testfile", fdata, datasize) < 0) {
		e = archive_error(a);
		archive_teardown(a);
		errx(1, "archive_addfile: %s", e);
	}

	if (archive_addfile(a, "testdir/testfile2", fdata, datasize) < 0) {
		e = archive_error(a);
		archive_teardown(a);
		errx(1, "archive_addfile: %s", e);
	}

	if (!archive_isvalid(a)) {
		e = archive_error(a);
		archive_teardown(a);
		errx(1, "archive_isvalid: %s", e);
	}

	wa = archive_fromfile(key + 1, ARCHIVES "/65535.bundle");
	if (wa == NULL) {
		archive_teardown(a);
		err(1, "archive_loadfromfile");
	}

	nfdata = archive_loadfile(wa, "testdir/testfile", &ndatasize);
	if (nfdata == NULL) {
		e = archive_error(wa);
		archive_teardown(wa);
		archive_teardown(a);
		errx(1, "failed to load testdir/testfile: %s", archive_error(a));
	}

	if (ndatasize != datasize) {
		archive_teardown(wa);
		archive_teardown(a);
		errx(1, "got incorrect data size for file");
	}

	if (memcmp(fdata, nfdata, datasize) != 0) {
		archive_teardown(wa);
		archive_teardown(a);
		errx(1, "got incorrect data for file");
	}

	free(nfdata);
	free(fdata);

	archive_teardown(wa);
	archive_teardown(a);
}
