#include <sys/types.h>

#include <endian.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "imaged.h"

#define NETMSG_WARN(M, S) warnx(S ": %s", netmsg_error(M))

int		 debug = 1, verbose = 1;

static char	*label = "stop stop stop";
static char	*newlabel = "go go go";
static char	*data = "this is not a dvd";

int
main()
{
	struct netmsg	*uut = NULL, *weakuut = NULL;
	char		*changedlabel;
	int		 unrecoverable, status = -1;

	uut = netmsg_new(NETOP_WRITE);
	if (uut == NULL) err(1, "netmsg_new");

	if (netmsg_setlabel(uut, label) < 0) {
		NETMSG_WARN(uut, "netmsg_setlabel");
		goto end;
	}

	if (netmsg_setdata(uut, data, strlen(data)) < 0) {
		NETMSG_WARN(uut, "netmsg_setdata");
		goto end;
	}

	if (!netmsg_isvalid(uut, &unrecoverable)) {
		NETMSG_WARN(uut, "strong netmsg should be valid but isn't");
		goto end;
	}

	/* in all probability */
	weakuut = netmsg_loadweakly(CHROOT MESSAGES "/0");
	if (weakuut == NULL) err(1, "netmsg_loadweakly");

	if (!netmsg_isvalid(weakuut, &unrecoverable)) {
		NETMSG_WARN(weakuut, "weak netmsg is not valid");
		goto end;
	}

	if (netmsg_setlabel(uut, newlabel) < 0) {
		NETMSG_WARN(uut, "netmsg_setlabel #2");
		goto end;
	}

	if (!netmsg_isvalid(uut, &unrecoverable)) {
		NETMSG_WARN(uut, "strong netmsg isn't valid after label change");
		goto end;
	}

	if (!netmsg_isvalid(weakuut, &unrecoverable)) {
		NETMSG_WARN(weakuut, "weak netmsg isn't valid after weak modification");
		goto end;
	}

	if ((changedlabel = netmsg_getlabel(weakuut)) == NULL) {
		NETMSG_WARN(weakuut, "weak netmsg_getlabel post-change");
		goto end;
	}

	status = strcmp(changedlabel, newlabel);
	if (status != 0) {
		warnx("new label %s does not match target %s", changedlabel, newlabel);
		status = -1;
	}

	free(changedlabel);
end:
	if (uut != NULL) netmsg_teardown(uut);
	if (weakuut != NULL) netmsg_teardown(weakuut);

	return status;
}
