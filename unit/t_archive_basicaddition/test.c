#include <sys/types.h>

#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "bundled.h"

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
	char		*fdata, *e;

	char		*nfdata;
	size_t		 ndatasize;

	config_parse("bundled.conf");

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

	if (archive_addfile(a, "testfile", fdata, datasize) == 0) {
		archive_teardown(a);
		errx(1, "archive_addfile worked on existing file");
	}

	if (!archive_isvalid(a)) {
		e = archive_error(a);
		archive_teardown(a);
		errx(1, "archive_isvalid: %s", e);
	}

	if (!archive_hasfile(a, "testfile")) {
		archive_teardown(a);
		errx(1, "archive does not have added file");

	} else if (!archive_hasfile(a, "testdir/testfile")) {
		archive_teardown(a);
		errx(1, "archive does not have added file");

	} else if (!archive_hasfile(a, "testdir/testfile2")) {
		archive_teardown(a);
		errx(1, "archive does not have added file");
	}

	nfdata = archive_loadfile(a, "testdir/testfile", &ndatasize);
	if (nfdata == NULL) {
		e = archive_error(a);
		archive_teardown(a);
		errx(1, "failed to load testdir/testfile: %s", archive_error(a));
	}

	if (ndatasize != datasize) {
		archive_teardown(a);
		errx(1, "got incorrect data size for file");
	}

	if (memcmp(fdata, nfdata, datasize) != 0) {
		archive_teardown(a);
		errx(1, "got incorrect data for file");
	}

	free(nfdata);
	free(fdata);
	archive_teardown(a);
}
