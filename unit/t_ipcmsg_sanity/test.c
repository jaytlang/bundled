#include <sys/types.h>

#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "bundled.h"

static char	*teststr = "hello, interprocess communication";
static uint32_t	 testkey = 5536;

int		 debug = 1, verbose = 1;

int
main()
{
	struct ipcmsg	*uut;
	char		*marshalled, *message;
	uint32_t	 messagekey;
	uint16_t	 marshalledsize;

	uut = ipcmsg_new(testkey, teststr);
	if (uut == NULL) err(1, "ipcmsg_new");

	message = ipcmsg_getmsg(uut);
	if (message == NULL) err(1, "ipcmsg_getmsg");

	if (strcmp(message, teststr) != 0)
		errx(1, "ipcmsg_getmsg returned incorrect message");

	free(message);

	messagekey = ipcmsg_getkey(uut);
	if (messagekey != testkey)
		errx(1, "ipcmsg_getkey returned incorrect key");

	marshalled = ipcmsg_marshal(uut, &marshalledsize);
	if (marshalled == NULL)
		err(1, "ipcmsg_marshal");

	ipcmsg_teardown(uut);

	uut = ipcmsg_unmarshal(marshalled, marshalledsize);
	if (uut == NULL) err(1, "ipcmsg_unmarshal");

	message = ipcmsg_getmsg(uut);
	if (message == NULL) err(1, "ipcmsg_getmsg");

	if (strcmp(message, teststr) != 0)
		errx(1, "ipcmsg_getmsg returned incorrect message %s", message);

	free(message);

	messagekey = ipcmsg_getkey(uut);
	if (messagekey != testkey)
		errx(1, "ipcmsg_getkey returned incorrect key");
	
	ipcmsg_teardown(uut);
	return 0;
}
