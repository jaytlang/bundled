#include <sys/types.h>

#include <err.h>
#include <unistd.h>

#include "imaged.h"

int
main()
{
	char	readbuffer[500];
	int	desc;

	if ((desc = buffer_open()) < 0)
		err(1, "can't create new buffer");

	if (buffer_seek(desc, sizeof(uint8_t), SEEK_SET) < 0)
		err(1, "can't buffer_seek to +1 off of start");

	if (buffer_read(desc, readbuffer, sizeof(uint64_t)) != 0)
		err(1, "read bytes that we have never written!");

	buffer_close(desc);
	return 0;
}
