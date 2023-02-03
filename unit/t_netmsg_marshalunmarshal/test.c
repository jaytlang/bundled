#include <sys/types.h>

#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "imaged.h"

#define TEST_ITERS 5

int		 debug = 1, verbose = 1;

static char	*label = "i am a label";
static char	*data = "not your mom's datum";

int
main()
{
	struct netmsg 	*uuts[NETOP_MAX + 1];
	uint8_t		 i, iters = 0;

dotest:
	for (i = 0; i <= NETOP_MAX; i++) {
		struct netmsg	*uut;
		char		*marshalled_uut;
		ssize_t		 marshalled_uutsize;
		int		 unrecoverable;

		uut = netmsg_new(i);

		/* make message */

		if (i == NETOP_UNUSED || i >= NETOP_MAX) {
			if (uut != NULL)
				errx(1, "successfully made netmsg with illegal opcode %u", i);
			else continue;
		}

		if (uut == NULL)
			err(1, "failed to create netmsg with opcode %u", i);

		if (i == NETOP_WRITE || i == NETOP_BUNDLE || i == NETOP_ERROR)
			if (netmsg_setlabel(uut, label) < 0)
				errx(1, "failed to set label on uut of type %u", i);

		if (i == NETOP_WRITE || i == NETOP_BUNDLE)
			if (netmsg_setdata(uut, data, strlen(data) + 1) < 0)
				errx(1, "failed to set data on uut of type %u: %s",
					i, netmsg_error(uut));

		if (!netmsg_isvalid(uut, &unrecoverable))
			errx(1, "netmsg invalid: %s", netmsg_error(uut));

		/* marshal message */
		
		marshalled_uutsize = netmsg_seek(uut, 0, SEEK_END);
		if (marshalled_uutsize < 0)
			errx(1, "netmsg_seek to end failed: %s", netmsg_error(uut));

		if (netmsg_seek(uut, 0, SEEK_SET) != 0)
			errx(1, "netmsg_seek to start failed: %s", netmsg_error(uut));

		marshalled_uut = reallocarray(NULL, marshalled_uutsize, sizeof(char));
		if (marshalled_uut == NULL)
			err(1, "reallocarray for marshalled uut");

		if (netmsg_read(uut, marshalled_uut, marshalled_uutsize) != marshalled_uutsize)
			errx(1, "netmsg_read could not read whole message: %s", netmsg_error(uut));

		/* leap of faith... */

		netmsg_teardown(uut);
		uut = uuts[i] = netmsg_new(i);

		if (uut == NULL)
			err(1, "failed to create netmsg with opcode %u", i);

		/* unmarshal message */

		if (netmsg_write(uut, marshalled_uut, marshalled_uutsize) != marshalled_uutsize)
			errx(1, "netmsg_write could not write message: %s", netmsg_error(uut));

		if (!netmsg_isvalid(uut, &unrecoverable))
			errx(1, "new netmsg invalid: %s", netmsg_error(uut));

		if (netmsg_gettype(uut) != i) errx(1, "new netmsg has incorrect type");

		if (i == NETOP_WRITE || i == NETOP_BUNDLE || i == NETOP_ERROR) {
			char	*newlabel;

			newlabel = netmsg_getlabel(uut);
			if (newlabel == NULL)
				errx(1, "could not read new netmsg label: %s", netmsg_error(uut));

			if (strcmp(newlabel, label) != 0)
				errx(1, "incorrect label %s received", newlabel);

			free(newlabel);
		}

		if (i == NETOP_WRITE || i == NETOP_BUNDLE) {
			char		*newdata;
			uint64_t	 newdatasize;

			newdata = netmsg_getdata(uut, &newdatasize);
			if (newdata == NULL)
				errx(1, "could not read new netmsg data: %s", netmsg_error(uut));

			if (newdatasize != strlen(data) + 1)
				errx(1, "wrong netmsg data size received");

			if (strcmp(newdata, data) != 0)
				errx(1, "incorrect data %s received", newdata);

			free(newdata);
		}


		free(marshalled_uut);
	}

	for (i = 0; i < NETOP_MAX; i++) {
		if (uuts[i] != NULL) {
			netmsg_teardown(uuts[i]);
			uuts[i] = NULL;
		}
	}

	if (++iters != TEST_ITERS) goto dotest;

	return 0;
}
