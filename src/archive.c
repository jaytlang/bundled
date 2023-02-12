/* imaged archive format
 * (c) jay lang, 2023ish
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/tree.h>

#include <arpa/inet.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <zlib.h>

#include "imaged.h"

#define ARCHIVE_BLOCKSIZE	1048576

struct archivefile {
	char			*name;
	size_t			 offset;

	RB_ENTRY(archivefile)	 entries;
};

static struct archivefile	*archivefile_new(char *, size_t);
static void			 archivefile_teardown(struct archivefile *);
static int			 archivefile_compare(struct archivefile *, struct archivefile *);

RB_HEAD(archivecache, archivefile);
RB_PROTOTYPE_STATIC(archivecache, archivefile, entries, archivefile_compare)


struct archive {
	uint32_t	 	 key;

	int		 	 archivefd;
	char			*path;
	int			 weak;

	char		 	 errstr[ERRSTRSIZE];

	struct archivecache	 cachedfiles;
	RB_ENTRY(archive)	 entries;
};

static int	 archive_compare(struct archive *, struct archive *);
static void	 archive_recorderror(struct archive *, const char *, ...);
static char	*archive_keytopath(uint32_t);

static uint32_t	 archive_takecrc32(struct archive *);
static void	 archive_writecrc32(struct archive *, uint32_t);

static ssize_t	 archive_seektostart(struct archive *);
static ssize_t	 archive_seekpastsignature(struct archive *);
static ssize_t	 archive_seektoend(struct archive *);
static ssize_t	 archive_seektonextfile(struct archive *);

static ssize_t	 archive_readfileinfo(struct archive *, uint16_t *, char *, uint32_t *, uint32_t *);
static void	 archive_appendfileinfo(struct archive *, uint16_t, char *, uint32_t, uint32_t);

static void	 archive_rebuildcache(struct archive *);


RB_HEAD(archivetree, archive);
RB_PROTOTYPE_STATIC(archivetree, archive, entries, archive_compare)

static struct archivetree	 activearchives = RB_INITIALIZER(&activearchives);



RB_GENERATE_STATIC(archivecache, archivefile, entries, archivefile_compare)
RB_GENERATE_STATIC(archivetree, archive, entries, archive_compare)

static struct archivefile *
archivefile_new(char *name, size_t offset)
{
	struct archivefile	*out;

	if (strlen(name) > MAXNAMESIZE) {
		errno = ENAMETOOLONG;
		log_fatal("archivefile_new: name length check");
	}

	out = calloc(1, sizeof(struct archivefile));
	if (out == NULL) log_fatal("archivefile_new: malloc");

	out->name = strdup(name);
	if (out->name == NULL) log_fatal("archivefile_new: strdup");

	out->offset = offset;
	return out;
}

static void
archivefile_teardown(struct archivefile *af)
{
	free(out->name);
	free(out);
}

static int
archivefile_compare(struct archivefile *a, struct archivefile *b)
{
	return strncmp(a->name, b->name, MAXNAMESIZE);
}

static int
archive_compare(struct archive *a, struct archive *b)
{
	int	result = 0;

	if (a->key > b->key) result = 1;
	if (a->key < b->key) result = -1;

	return result;
}

static void
archive_recorderror(struct archive *a, const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	vsnprintf(a->errstr, ERRSTRSIZE, fmt, ap);
	va_end(ap);
}

static char *
archive_keytopath(uint32_t key)
{
	char	*out;

	out = asprintf(&out, "%s/%u.bundle", ARCHIVES, key);
	if (out == NULL)
	return out;
}

static uint32_t
archive_takecrc32(struct archive *a)
{
	char		*readbuffer;
	uint32_t	 checksum;
	ssize_t		 bytesread;

	readbuffer = reallocarray(1, ARCHIVE_BLOCKSIZE, sizeof(char));
	if (readbuffer == NULL) goto end;

	checksum = crc32_z(checksum, NULL, 0);

	if (archive_seekpastsignature(a) < 0) {
		if (errno != EBADMSG)
			log_fatal("archive_takecrc32: archive_seekpastsignature");

	} else {
		while ((bytesread = read(a->archivefd, readbuffer, ARCHIVE_BLOCKSIZE)) > 0)
			checksum = crc32(checksum, readbuffer, (uint32_t)bytesread);

		if (bytesread < 0)
			log_fatal("archive_takecrc32: read");
	}

	if (archive_seektoend(a) < 0)
		log_fatal("archive_takecrc32: archive_seektoend");

	return checksum;
}

static void
archive_writecrc32(struct archive *a, uint32_t checksum)
{
	ssize_t		written;
	uint32_t	bechecksum;

	if (archive_seektostart(a) != 0)
		log_fatal("archive_writecrc32: archive_seektostart");

	bechecksum = htonl(checksum);

	written = write(a->archivefd, &bechecksum, sizeof(uint32_t));
	if (written < 0)
		log_fatal("archive_writecrc32: write");

	if (archive_seektoend(a) < 0)
		log_fatal("archive_writecrc32: archive_seektoend");
}

static ssize_t
archive_seektostart(struct archive *a)
{
	ssize_t		seekstatus;

	seekstatus = lseek(a->archivefd, 0, SEEK_SET);
}

static ssize_t
archive_seekpastsignature(struct archive *a)
{
	ssize_t	endoffset, offset = sizeof(uint32_t) + sizeof(uint16_t) + MAXSIGSIZE;
	ssize_t status = -1;

	endoffset = archive_seektoend(a);
	if (endoffset < 0) log_fatal("archive_seekpastsignature: archive_seektoend");

	if (endoffset < offset) {
		errno = EBADMSG;
		goto end;
	}

	status = lseek(a->archivefd, offset, SEEK_SET);
end:
	return status;
}

static ssize_t
archive_seektoend(struct archive *a)
{
	return lseek(a->archivefd, 0, SEEK_END);
}

static ssize_t
archive_seektonextfile(struct archive *a)
{
	ssize_t		amtread, currentoffset, status = -1;
	uint32_t	compressedsize;

	currentoffset = lseek(a->archivefd, 0, SEEK_CUR);
	if (currentoffset < 0)
		log_fatal("archive_seektonextfile: lseek to current position");

	amtread = archive_readfileinfo(a, NULL, NULL, NULL, &compressedsize);
	if (amtread < 0)
		status = -1;

	} else {
		status = lseek(a->archivefd, (off_t)compressedsize, SEEK_CUR);
		if (status < 0) log_fatal("archive_seektonextfile: lseek");
	}

	return status;
}

/* TOOD: make me ssize_t */
static int
archive_readfileinfo(struct archive *a, uint16_t *labelsize, char *label,
	uint32_t *uncompressedsize, uint32_t *compressedsize)
{
	char		*bufp;
	char		 buf[sizeof(uint16_t) + MAXNAMESIZE + 2 * sizeof(uint32_t)]

	ssize_t		 initial_offset, hdr_count;
	uint16_t	 labelsizecopy;

	int		 status = -1;

	initial_offset = lseek(a->archivefd, 0, SEEK_CUR);
	if (initial_offset < 0) 
		log_fatal("archive_readfileinfo: lseek for initial offset");

	hdr_count = read(a->archivefd, buf, sizeof(uint16_t) + MAXNAMESIZE + 2 * sizeof(uint32_t));

	if (hdr_count < 0)
		log_fatal("archive_readfileinfo: read");
	else if (hdr_count == 0) {
		status = 
		goto end;
	} else if (hdr_count != sizeof(uint16_t) + MAXNAMESIZE + 2 * sizeof(uint32_t)) {
		errno = EFTYPE;
		goto end;
	}

	bufp = buf;
	labelsizecopy = ntohs((uint16_t *)bufp);

	if (labelsize != NULL) *labelsize = labelsizecopy;
	bufp += sizeof(uint16_t);

	if (label != NULL) {
		memcpy(label, bufp, labelsizecopy);
		label[labelsizecopy] = '\0';
	}

	bufp += labelsizecopy;

	if (uncompressedsize != NULL)
		*uncompressedsize = ntohl((uint32_t *)bufp);
	bufp += sizeof(uint32_t);

	if (compressedsize != NULL)
		*compressedsize = ntohl((uint32_t)bufp);

	status = 0;
end:
	if (status < 0)
		if (lseek(a->archivefd, initial_offset, SEEK_SET) != initial_offset)
			log_fatal("archive_readfileinfo: reset to initial offset after EOF hit");

	return status;
}

static void
archive_appendfileinfo(struct archive *a, uint16_t labelsize, char *label,
	uint32_t uncompressedsize, uint32_t compressedsize)
{
	uint32_t	beuncompressedsize, becompressedsize;
	uint16_t	belabelsize;
	int		status = - 1;

	belabelsize = htons(labelsize);
	beuncompressedsize = htonl(uncompressedsize);
	becompressedsize = htonl(compressedsize);

	if (write(a->archivefd, belabelsize, sizeof(uint16_t)) != sizeof(uint16_t))
		goto end;

	if (write(a->archivefd, label, labelsize) != (ssize_t)labelsize)
		goto end;
	
	if (write(a->archivefd, beuncompressedsize, sizeof(uint32_t)) != sizeof(uint32_t))
		goto end;

	if (write(a->archivefd, becompressedsize, sizeof(uint32_t)) != sizeof(uint32_t))
		goto end;

	status = 0;
end:
	if (status < 0) log_fatal("archive_appendfileinfo: write");
	else return status;
}

static void
archive_rebuildcache(struct archive *a)
{
	char	 		 newname[MAXNAMESIZE + 1];
	uint32_t		 newsize, newrealsize;
	ssize_t	 		 newoffset = 0;

	/* XXX: shouldn't happen, call archive_isvalid somewhere safe before
	 * trying to set up data based off of it. archive_isvalid doesn't check
	 * the cache, so this should mostly pass if everything else is set up.
	 */
	if (!archive_isvalid(a))
		log_fatal("archive_rebuildcache: rebuilt invalid archive cache");

	newoffset = archive_seekpastsignature(a);
	if (newoffset < 0)
		log_fatal("archive_rebuildcache: archive_seekpastsignature");

	while (archive_readfileinfo(NULL, newname, &newrealsize, &newsize) == 0) {
		struct archivefile	*newentry;

		newentry = archivefile_new(newname, (size_t)newoffset);
		RB_INSERT(archivecache, &a->cachedfiles, newentry);

		if (lseek(a->archivefd, (off_t)newsize, SEEK_CUR) < 0)
			log_fatal("archive_rebuildcache: lseek over file body");

		newoffset += sizeof(uint16_t) + 2 * sizeof(uint32_t) + strlen(newname);
		newoffset += newsize;
	}
}

struct archive *
archive_new(uint32_t key)
{
	struct archive	*a, *out = NULL;
	char		*newpath = NULL;
	uint32_t	 initialcrc;

	int		 newfd, flags = O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC;
	mode_t		 mode = S_IRUSR | S_IRGRP;

	if (archive_fromkey(key) != NULL) {
		errno = EALREADY;
		goto end;
	}

	newpath = archive_keytopath(key);
	newfd = open(newpath, flags, mode);
	if (newfd < 0)
		log_fatal("archive_new: open");

	a = calloc(1, sizeof(struct archive));
	if (a == NULL)
		log_fatal("archive_new: calloc");

	a->key = key;
	a->archivefd = newfd;
	a->weak = 0;
	a->path = newpath;

	RB_INIT(&a->cachedfiles);

	initialcrc = archive_takecrc32(a);
	archive_writecrc32(a, initialcrc);
	archive_writesignature(a, "");

	/* XXX: these steps do nothing but good to have and debug i guess */
	if (!archive_isvalid(a))
		log_fatalx("archive_new: fresh archive is not valid: %s", a->errstr);

	archive_rebuildcache();

	out = a;
	RB_INSERT(archivetree, &activearchives, out);
end:
	if (newfd > 0 && out == NULL) {
		close(newfd);
		if (unlink(newpath) < 0)
			log_fatal("archive_new: unlink");
	}

	if (newpath != NULL && out == NULL)
		free(newpath);

	return out;
}

struct archive *
archive_fromfile(uint32_t key, char *path)
{
	struct archive	*a, *out = NULL;
	char		*loadpath = NULL;
	int		 loadfd;

	if (archive_fromkey(key) != NULL) {
		errno = EALREADY;
		goto end;
	}

	loadpath = strdup(path);
	if (loadpath == NULL)
		log_fatal("archive_fromfile: strdup");

	loadfd = open(path, O_RDONLY);
	if (loadfd < 0) goto end;
	else if (lseek(loadfd, 0, SEEK_END) < 0)
		log_fatal("archive_fromfile: lseek");

	a = calloc(1, sizeof(struct archive));
	if (a == NULL)
		log_fatal("archive_fromfile: calloc");

	a->key = key;
	a->archivefd = loadfd;
	a->weak = 1;
	a->path = loadpath;

	RB_INIT(&a->cachedfiles);

	if (!archive_isvalid(a)) {
		errno = EFTYPE;
		goto end;
	}

	archive_rebuildcache();

	out = a;
	RB_INSERT(archivetree, &activearchives, out);
end:
	if (out == NULL && a != NULL)
		free(a);

	if (loadfd > 0 && out == NULL)	
		close(loadfd);

	if (loadpath != NULL && out == NULL)
		free(loadpath);

	return out;
}

struct archive *
archive_fromkey(uint32_t key)
{
	struct archive	dummy, *out;

	dummy.key = key;
	out = RB_FIND(archivetree, &activearchives, &dummy); 

	if (out == NULL) errno = EINVAL;
	return out;
}

void
archive_teardown(struct archive *a)
{
	struct archivefile	*thisfile;

	while (!RB_EMPTY(&a->cachedfiles)) {
		thisfile = RB_ROOT(&a->cachedfiles);
		RB_REMOVE(archivecache, &a->cachedfiles, thisfile);

		archivefile_teardown(thisfile);
	}

	RB_REMOVE(archivetree, &activearchives, a);

	close(a->archivefd);
	if (!a->weak)
		if (unlink(a->path) < 0)
			log_fatal("archive_teardown: unlink");

	free(a->path);
	free(a);
}

int
archive_addfile(struct archive *a, char *fname, char *data, size_t datasize)
{
	struct archivefile	*newcacheentry;
	size_t			 newcacheoffset;
	uint32_t		 newcrc;
	uint16_t		 newlabelsize;

	char			*compressedbuf;
	size_t		 	 compressedsize, compressedsizebound;
	int		 	 zstatus, status = -1;

	if (datasize > MAXFILESIZE) {
		archive_recorderror(a, "data passed (size %lu) too large", datasize);
		goto end;

	} else if (strlen(fname) > MAXNAMESIZE) {
		archive_recorderror(a, "fname passed (size %lu) too large", strlen(fname));
		goto end;
	}

	newcacheoffset = lseek(a->archivefd, 0, SEEK_CUR);
	if (newcacheoffset < 0)
		log_fatal("archive_addfile: lseek");

	compressedsizebound = compressBound(datasize);

	compressedbuf = reallocarray(NULL, compressedsizebound, sizeof(char));
	if (compressedbuf == NULL) log_fatal("archive_addfile: malloc");

	compressedsize = compressedsizebound;
	zstatus = compress(compressedbuf, &compressedsize, data, datasize);

	if (zstatus != Z_OK)
		log_fatalx("archive_addfile: compress: zstatus = %d", zstatus);

	archive_appendfileinfo(a, strlen(fname), fname, datasize, compressedsize);

	if (write(a->archivefd, compressedbuf, compressedsize) < 0)
		log_fatal("archive_addfile: write");

	newcacheentry = archivefile_new(fname, newcacheoffset);
	RB_INSERT(archivecache, &a->cachedfiles, newcacheentry);

	newcrc = archive_takecrc32(a);
	archive_writecrc32(a, newcrc);

	status = 0;
end:
	return status;
}

int
archive_hasfile(struct archive *a, char *fname)
{
	struct archivefile	*found, dummy;
	int			 present = 0;

	if (strlen(fname) > MAXNAMESIZE) {
		archive_recorderror(a, "fname passed (size %lu) too large", strlen(fname));
		goto end;
	}

	dummy.name = fname;
	found = RB_FIND(archivecache, &a->cachedfiles, &dummy);
	
	if (found != NULL) present = 1;
end:
	return present;
}

char *
archive_loadfile(struct archive *a, char *fname, size_t *datasizeout)
{
	struct archivefile	*found, dummy;
	char			*compressedbuf, *bufout = NULL;
	size_t			 compressedsize;
	int			 zstatus;
	
	if (strlen(fname) > MAXNAMESIZE) {
		archive_recorderror(a, "fname passed (size %lu) too large", strlen(fname));
		goto end;
	}

	dummy.fname = fname;
	found = RB_FIND(archivecache, &a->cachedfiles, &dummy);

	if (found == NULL) {
		archive_recorderror(a, "tried to load non-existent file %s from archive", fname);
		goto end;
	}

	if (lseek(a->archivefd, found->offset, SEEK_SET) < 0)
		log_fatal("archive_loadfile: lseek");

	if (archive_readfileinfo(a, NULL, NULL, datasizeout, &compressedsize) < 0)
		log_fatal("archive_loadfile: archive_readfileinfo");

	compressedbuf = reallocarray(NULL, compressedsize, sizeof(char));
	if (compressedbuf == NULL)
		log_fatal("archive_loadfile: reallocarray -> compressedbuf");

	bufout = reallocarray(NULL, *datasizeout, sizeof(char));
	if (bufout == NULL)
		log_fatal("archive_loadfile: reallocarray -> output buffer");

	if (read(a->archivefd, compressedbuf, compressedsize) < 0)
		log_fatal("archive_loadfile: read");

	zstatus = uncompress(bufout, datasizeout, compressedbuf, compressedsize);
	if (zstatus != Z_OK)
		log_fatalx("archive_loadfile: uncompress returned zstatus %d", zstatus);

	free(compressedbuf);
end:
	if (lseek(a->archivefd, 0, SEEK_END) < 0)
		log_fatal("archive_loadfile: reset seek pointer to end");

	return bufout;
}

uint32_t
archive_getcrc32(struct archive *a)
{
	uint32_t	crcout;

	if (lseek(a->archivefd, 0, SEEK_SET) < 0)
		log_fatal("archive_getcrc32: reset seek pointer to crc32");

	if (read(a->archivefd, &crcout, sizeof(uint32_t)) < 0)
		log_fatal("archive_getcrc32: read");

	crcout = ntohl(crcout);

	if (lseek(a->archivefd, 0, SEEK_END) < 0)
		log_fatal("archive_getcrc32: reset seek pointer to end");

	return crcout;
}

char *
archive_getsignature(struct archive *a)
{
	uint16_t	 signaturelen;
	char		*signature;

	if (lseek(a->archivefd, sizeof(uint32_t), SEEK_SET) < 0)
		log_fatal("archive_getsignature: lseek to signature length");

	if (read(a->archivefd, &signaturelen, sizeof(uint16_t)) < 0)
		log_fatal("archive_getsignature: read");

	signaturelen = ntohs(signaturelen);

	signature = reallocarray(NULL, signaturelen + 1, sizeof(char));
	if (signature == NULL)
		log_fatal("archive_getsignature: reallocarray");

	if (read(a->archivefd, signature, signaturelen) < 0)
		log_fatal("archive_getsignature: read");

	signature[signaturelen] = '\0';

	if (lseek(a->archivefd, 0, SEEK_END) < 0)
		log_fatal("archive_getsignature: reset seek ptr to end");

	return signature;
}

void
archive_writesignature(struct archive *a, char *signature)
{
	size_t		signaturelen;
	ssize_t		written;
	uint16_t	shortsignaturelen, besignaturelen;

	char		writebuffer[sizeof(uint16_t) + MAXSIGSIZE];

	signaturelen = strlen(signature);
	if (signaturelen > UINT16_MAX)
		log_fatalx("archive_writesignature: signature length exceeds maximum allowed");

	shortsignaturelen = (uint16_t)signaturelen;
	besignaturelen = htons(shortsignaturelen);

	bzero(writebuffer, sizeof(uint16_t) + MAXSIGSIZE);

	memcpy(writebuffer, besignaturelen, sizeof(uint16_t));
	memcpy(writebuffer + sizeof(uint16_t), signature, signaturelen);

	if (lseek(a->archivefd, sizeof(uint32_t), SEEK_SET) < 0)
		log_fatal("archive_writesignature: lseek past crc32");

	written = write(a->archivefd, writebuffer, sizeof(uint16_t) + MAXSIGSIZE);
	if (written < 0)
		log_fatal("archive_writesignature: write");

	if (archive_seektoend(a) < 0)
		log_fatal("archive_writesignature: archive_seektoend");
}

int
archive_isvalid(struct archive *a)
{
	/* things to do:
	 * -> check archive length is sane (>= signature + crc)
	 * -> check the crc is correct
	 * -> for each file,
	 *	-> ensure header can be read at all; i.e. that it is long enough
	 *	-> ensure file name size is sane
	 *	-> ensure file compressed size + uncompressed sizes are sane
	 *	-> ensure file compressed size <= compressBound(uncompressed)
	 */

	ssize_t		archivelength;
	uint32_t	claimedcrc, actualcrc;

	char		*label;
	uint32_t	 compressedsize, uncompressedsize;
	uint16_t	 labelsize;
	int		 status = -1;
	
	archivelength = lseek(a->archivefd, 0, SEEK_END);
	if (archivelength < 0)
		log_fatal("archive_isvalid: lseek to ascertain length");

	if (archivelength < sizeof(uint32_t) + sizeof(uint16_t) + MAXSIGSIZE) {
		archive_recorderror(a, "archive is too short (length %ld)", archivelength);
		goto end;
	}

	claimedcrc = archive_getcrc32(a);
	actualcrc = archive_takecrc32(a);

	if (claimedcrc != actualcrc) {
		archive_recorderror(a, "incorrect crc recorded for archive");
		goto end;
	}

	

	return status;
}
