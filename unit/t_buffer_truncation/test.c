#include <sys/types.h>

#include <err.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>

#include "imaged.h"

static char	*teststr = "1234567890";

int
main()
{
	char	readbuffer[500];
	ssize_t	testsize;
	int	desc;

	testsize = (ssize_t)strlen(teststr);
	bzero(readbuffer, 500 * sizeof(char));

	desc = buffer_open();
	if (desc < 0) err(1, "buffer_open");

	if (buffer_write(desc, teststr, testsize) != testsize)
		err(1, "buffer_write couldn't write entire test string");

	if (buffer_truncate(desc, 5) < 0)
		err(1, "buffer_truncate did not truncate buffer");

	if (buffer_read(desc, readbuffer, 5) != 0)
		err(1, "buffer_read past newly truncated eof didn't return 0");

	if (buffer_seek(desc, 0, SEEK_END) != 5)
		err(1, "buffer_seek to new end of buffer did not return new size");

	if (buffer_seek(desc, 0, SEEK_SET) != 0)
		err(1, "buffer_seek to beginning of buffer failed");

	if (buffer_read(desc, readbuffer, 500) != 5)
		err(1, "buffer_read truncated buffer didn't give correct length");

	if (strcmp(readbuffer, "12345") != 0)
		errx(1, "truncated buffer returned %s, expected '12345'", readbuffer);

	buffer_close(desc);
	return 0;
}
