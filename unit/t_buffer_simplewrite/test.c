#include <sys/types.h>

#include <err.h>
#include <string.h>
#include <unistd.h>

#include "bundled.h"

static char	*teststr = "Hello, world";

int
main()
{
	char	 readbuffer[500];
	ssize_t	 testsize;
	int	 desc;

	testsize = strlen(teststr) + 1;

	desc = buffer_open();
	if (desc < 0) err(1, "buffer_open");

	if (buffer_write(desc, teststr, testsize) != testsize)
		err(1, "buffer_write teststr");

	if (buffer_seek(desc, 0, SEEK_SET) != 0)
		err(1, "buffer_seek with SEEK_SET to 0");

	if (buffer_read(desc, readbuffer, 500) != testsize)
		err(1, "buffer_read failed to read full written string");

	if (strncmp(teststr, readbuffer, testsize) != 0)
		err(1, "buffer_read got %s vs. expected %s", readbuffer, teststr);

	buffer_close(desc);
	return 0;
}
