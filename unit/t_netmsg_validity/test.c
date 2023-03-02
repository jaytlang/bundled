#include <sys/types.h>

#include <endian.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bundled.h"

#define NETMSG_ERROR(M, S) errx(1, S ": %s", netmsg_error(M))

int		 debug = 1, verbose = 1;

static char	*label = "aaaaaaaaaaaaaaaaaaaaaa";
static char	*data = "meow meow meow meow meow";


int
main()
{
	struct netmsg	*uut;
	char		*dataptr;
	uint64_t	 lengthvalue, belengthvalue;
	int		 unrecoverable;

	config_parse(NULL);

	uut = netmsg_new(NETOP_WRITE);
	if (uut == NULL) err(1, "netmsg_new failed");

	/* first, partial write of label length */

	belengthvalue = lengthvalue = 0;

	if (netmsg_seek(uut, sizeof(uint8_t), SEEK_SET) != sizeof(uint8_t))
		NETMSG_ERROR(uut, "netmsg_seek over type failed");

	if (netmsg_write(uut, &belengthvalue, sizeof(uint32_t)) != sizeof(uint32_t))	
		NETMSG_ERROR(uut, "netmsg_write partial label length failed");

	if (netmsg_isvalid(uut, &unrecoverable))
		errx(1, "netmsg valid with only partial label length");

	/* finished write of label length */

	lengthvalue = (uint64_t)strlen(label);
	belengthvalue = htobe64(lengthvalue);

	if (netmsg_seek(uut, -4, SEEK_CUR) != sizeof(uint8_t))
		NETMSG_ERROR(uut, "netmsg_seek failed");

	if (netmsg_write(uut, &belengthvalue, sizeof(uint64_t)) != sizeof(uint64_t))
		NETMSG_ERROR(uut, "netmsg_write failed");

	if (netmsg_isvalid(uut, &unrecoverable))
		errx(1, "netmsg valid with only label length");

	/* partial label write */

	dataptr = label;

	if (netmsg_write(uut, dataptr, 10) != 10)
		NETMSG_ERROR(uut, "netmsg_write failed");

	if (netmsg_isvalid(uut, &unrecoverable))
		errx(1, "netmsg valid with partial label");

	dataptr += 10;

	/* complete label write */

	if (netmsg_write(uut, dataptr, strlen(dataptr)) != (ssize_t)strlen(dataptr))
		NETMSG_ERROR(uut, "netmsg_write failed");

	if (netmsg_isvalid(uut, &unrecoverable))
		errx(1, "netmsg valid with only label");

	/* partial data length write */

	belengthvalue = lengthvalue = 0;

	if (netmsg_write(uut, &belengthvalue, sizeof(uint32_t)) != sizeof(uint32_t))
		NETMSG_ERROR(uut, "netmsg_write partial data length failed");

	if (netmsg_isvalid(uut, &unrecoverable))
		errx(1, "netmsg valid with partial data length");

	/* complete data length write */

	lengthvalue = (uint64_t)strlen(data);
	belengthvalue = htobe64(lengthvalue);

	if (netmsg_seek(uut, 0 - (off_t)sizeof(uint32_t), SEEK_CUR) < 0)
		NETMSG_ERROR(uut, "netmsg_seek failed");

	if (netmsg_write(uut, &belengthvalue, sizeof(uint64_t)) != sizeof(uint64_t))
		NETMSG_ERROR(uut, "netmsg_write complete data length failed");

	if (netmsg_isvalid(uut, &unrecoverable))
		errx(1, "netmsg valid with only data length");

	/* partial data write */

	dataptr = data;

	if (netmsg_write(uut, dataptr, 10) != 10)
		NETMSG_ERROR(uut, "netmsg_write failed");

	if (netmsg_isvalid(uut, &unrecoverable))
		errx(1, "netmsg valid with partial data");

	dataptr += 10;

	/* finished message */

	if (netmsg_write(uut, dataptr, strlen(dataptr)) != (ssize_t)strlen(dataptr))
		NETMSG_ERROR(uut, "netmsg_write failed");

	if (!netmsg_isvalid(uut, &unrecoverable))
		NETMSG_ERROR(uut, "completely constructed netmsg invalid");

	netmsg_teardown(uut);

	uut = netmsg_new(NETOP_SIGN);
	if (uut == NULL) err(1, "netmsg_new failed");

	if (!netmsg_isvalid(uut, &unrecoverable))
		NETMSG_ERROR(uut, "completely constructed simple netmsg invalid");

	netmsg_teardown(uut);
	return 0;
}
