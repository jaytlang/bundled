#include <sys/types.h>

#include <err.h>
#include <string.h>
#include <unistd.h>

#include "imaged.h"

static char	*teststr = "Hello ";
static char	*teststr2 = "there";

int
main()
{
	char	 readbuffer[500];
	ssize_t	 testsize;
	int	 desc;

	testsize = strlen(teststr) + strlen(teststr2) + 1;

	desc = buffer_open();
	if (desc < 0) err(1, "buffer_open");

	if (buffer_write(desc, teststr, strlen(teststr)) != (ssize_t)strlen(teststr))
		err(1, "buffer_write teststr");

	if (buffer_write(desc, teststr2, strlen(teststr2) + 1) != (ssize_t)strlen(teststr2) + 1)
		err(1, "buffer_write teststr2");

	if (buffer_seek(desc, 0, SEEK_SET) != 0)
		err(1, "buffer_seek with SEEK_SET to 0");

	if (buffer_read(desc, readbuffer, 500) != testsize)
		err(1, "buffer_read failed to read double-written string");

	if (strncmp("Hello there", readbuffer, testsize) != 0)
		errx(1, "buffer_read got %s vs. expected %s%s", readbuffer, teststr, teststr2);

	buffer_close(desc);
	return 0;
}
