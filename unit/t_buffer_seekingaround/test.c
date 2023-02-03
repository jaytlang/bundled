#include <sys/types.h>

#include <err.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "imaged.h"

static char *teststr = "hello, world";

int
main()
{
	char		readbuf[500];
	ssize_t		testsize;
	int		desc, i;

	testsize = strlen(teststr) + 1;
	bzero(readbuf, 500 * sizeof(char));

	desc = buffer_open();
	if (desc < 0) err(1, "buffer_open");

	if (buffer_write(desc, teststr, testsize) != testsize)
		err(1, "failed to write teststr to buffer");

	if (buffer_seek(desc, 7, SEEK_SET) != 7)
		err(1, "buffer_seek with SEEK_SET returned incorrect value");

	for (i = 0; i < 2; i++) {
		if (buffer_read(desc, readbuf, 3) != 3)
			err(1, "buffer_read out of middle of buffer");

		if (strcmp(readbuf, "wor") != 0)
			err(1, "buffer_read returned wrong string %s", readbuf);

		if (buffer_seek(desc, -3, SEEK_CUR) != 7)
			err(1, "buffer_seek to already-read material failed");
	}

	if (buffer_seek(desc, 50, SEEK_END) != testsize + 50)
		err(1, "buffer_seek to the end + 50 failed");

	if (buffer_read(desc, readbuf, 5) != 0)
		err(1, "buffer_read past EOF didn't return 0");

	if (buffer_seek(desc, 0, SEEK_END) != testsize)
		err(1, "buffer_seek to the end failed");
}
