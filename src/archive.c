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

static int	 archive_takecrc32(struct archive *, uint32_t *);
static int	 archive_writecrc32(struct archive *, uint32_t);
static int	 archive_writesignature(struct archive *, char *);

static ssize_t	 archive_seektostart(struct archive *);
static ssize_t	 archive_seekpastsignature(struct archive *);
static ssize_t	 archive_seektoend(struct archive *);
static ssize_t	 archive_seektonextfile(struct archive *);

static int	 archive_readfileinfo(struct archive *, uint16_t *, char **, uint32_t *, uint32_t *);
static int	 archive_appendfileinfo(struct archive *, uint16_t, char *, uint32_t, uint32_t);

static int	 archive_rebuildcache(struct archive *);


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
	int	result = 0;

	if (a->offset > b->offset) result = 1;
	if (a->offset < b->offset) result = -1;

	return result;
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
	return out;
}

static int
archive_takecrc32(struct archive *a, uint32_t *checksum)
{
	char		*readbuffer;
	ssize_t		 bytesread;

	int		 status = -1;

	readbuffer = malloc(ARCHIVE_BLOCKSIZE);
	if (readbuffer == NULL) goto end;

	*checksum = crc32_z(*checksum, NULL, 0);

	if (archive_seekpastsignature(a) < 0) {
		if (errno != EBADMSG)
			goto end;
		else status = 0;

	} else {
		while ((bytesread = read(a->archivefd, readbuffer, ARCHIVE_BLOCKSIZE)) > 0)
			*checksum = crc32(*checksum, readbuffer, (uint32_t)bytesread);

		if (bytesread == 0) status = 0;
	}

end:
	if (archive_seektoend(a) < 0)
		status = -1;

	if (readbuffer != NULL) free(readbuffer);
	return status;
}

static int
archive_writecrc32(struct archive *a, uint32_t checksum)
{
	uint32_t	bechecksum;
	int		status = -1;

	if (archive_seektostart(a) != 0) goto end;

	bechecksum = htonl(checksum);
	if (write(a->archivefd, &bechecksum, sizeof(uint32_t)) != sizeof(uint32_t))
		goto end;

	status = 0
end:
	if (archive_seektoend(a) < 0)
		status = -1;

	return status;
}

static int
archive_writesignature(struct archive *a, char *signature)
{
	size_t		signaturelen;
	uint16_t	shortsignaturelen, besignaturelen;
	char		zero[MAXSIGSIZE];
	int		status = -1;

	signaturelen = strlen(signature);
	if (signaturelen > UINT16_MAX) {
		errno = EINVAL;
		goto end;
	}

	shortsignaturelen = (uint16_t)signaturelen;
	besignaturelen = htons(shortsignaturelen);

	if (lseek(a->archivefd, sizeof(uint32_t), SEEK_SET) != sizeof(uint32_t))
		goto end;

	if (write(a->archivefd, besignaturelen, sizeof(uint16_t)) != sizeof(uint16_t))
		goto end;

	if (write(a->archivefd, signature, signaturelen) != signaturelen)
		goto end;

	bzero(zero, MAXSIGSIZE * sizeof(char));
	if (write(a->archivefd, zero, MAXSIGSIZE - signaturelen) < 0)
		goto end;

	status = 0;
end:
	if (archive_seektoend(a) < 0)
		status = -1;

	return status;
}

static ssize_t
archive_seektostart(struct archive *a)
{
	return lseek(a->archivefd, 0, SEEK_SET);
}

static ssize_t
archive_seekpastsignature(struct archive *a)
{
	size_t	offset = sizeof(uint32_t) + sizeof(uint16_t) + MAXSIGSIZE;

	return lseek(a->archivefd, offset, SEEK_SET);
}

static ssize_t
archive_seektoend(struct archive *a)
{
	return lseek(a->archivefd, 0, SEEK_END);
}

static ssize_t
archive_seektonextfile(struct archive *a)
{
	ssize_t		status = -1;
	uint32_t	compressedsize;

	if (archive_readfileinfo(a, NULL, NULL, NULL, &compressedsize) < 0)
		goto end;

	status = lseek(a->archivefd, (off_t)compressedsize, SEEK_CUR);
end:
	return status;
}

static int
archive_readfileinfo(struct archive *a, uint16_t *labelsize, char *label,
	uint32_t *uncompressedsize, uint32_t *compressedsize)
{
	char		*labelptr, labelscratch[MAXNAMESIZE];
	uint32_t	*compressedsizeptr, compressedsizescratch;
	uint32_t	*uncompressedsizeptr, uncompressedsizescratch;
	uint16_t	*labelsizeptr, labelsizescratch;

	ssize_t		 initial_offset;
	int		 status = -1;

	initial_offset = lseek(a->archivefd, 0, SEEK_CUR);
	if (initial_offset < 0) goto end;

	if (labelsize != NULL) labelsizeptr = labelsize;
	else labelsizeptr = &labelsizescratch;

	if (label != NULL) labelptr = label;
	else labelptr = labelscratch;

	if (uncompressedsize != NULL) uncompressedsizeptr = uncompressedsize;
	else uncompressedsizeptr = &uncompressedsizescratch;

	if (compressedsize != NULL) compressedsizeptr = compressedsize;
	else compressedsizeptr = &compressedsizescratch;

	if (read(a->archivefd, labelsizeptr, sizeof(uint16_t)) != sizeof(uint16_t))
		goto end;

	*labelsizeptr = ntohs(*labelsizeptr);

	if (read(a->archivefd, labelptr, *labelsizeptr) != *labelsizeptr))
		goto end;

	if (read(a->archivefd, uncompressedsizeptr, sizeof(uint32_t)) != sizeof(uint32_t))
		goto end;

	*uncompressedsizeptr = ntohl(*uncompressedsizeptr);

	if (read(a->archivefd, compressedsizeptr, sizeof(uint32_t)) != sizeof(uint32_t))
		goto end;

	*compressedsizeptr = ntohl(*compressedsizeptr);

	status = 0;
end:
	if (initial_offset > 0)
		if (lseek(a->archivefd, initial_offset, SEEK_SET) != initial_offset)
			status = -1;

	return status;
}

static int
archive_appendfileinfo(struct archive *a, uint16_t labelsize, char *label,
	uint32_t uncompressedsize, uint32_t compressedsize);

struct archive *
archive_new(uint32_t key)
{
	struct archive	*a, *out = NULL;
	char		*newpath = NULL;
	uint64_t	 zeroes = 0;

	int		 newfd, flags = O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC;
	mode_t		 mode = S_IRUSR | S_IRGRP;

	if (archive_fromkey(key) != NULL) {
		errno = EALREADY;
		goto end;
	}

	newpath = archive_keytopath(key);
	if (newpath == NULL) goto end;

	newfd = open(newpath, flags, mode);
	if (newfd < 0) goto end;

	if (write(newfd, &zeroes, 6 * sizeof(char)) != 6 * sizeof(char)) goto end;
	else if (lseek(newfd, 0, SEEK_SET) != 0) goto end;

	a = calloc(1, sizeof(struct archive));
	if (a == NULL) goto end;

	a->key = key;
	a->archivefd = newfd;
	a->weak = 0;
	a->path = newpath;

	RB_INIT(&a->cachedfiles);
	RB_INSERT(archivetree, &activearchives, a);

	out = a;
end:
	if (newfd > 0 && out == NULL) {
		close(newfd);
		unlink(newpath);
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
	if (loadpath == NULL) goto end;

	loadfd = open(path, O_RDONLY);
	if (loadfd < 0) goto end;

	a = calloc(1, sizeof(struct archive));
	if (a == NULL) goto end;

	a->key = key;
	a->archivefd = loadfd;
	a->weak = 1;
	a->path = loadpath;

	RB_INIT(&a->cachedfiles);
	RB_INSERT(archivetree, &activearchives, a);

	out = a;

	/* TODO: repopulate cached files */
end:
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
	if (!a->weak) unlink(a->path);

	free(a->path);
	free(a);
}

/* TODO: I am disgusting and long, make me less so */
int
archive_addfile(struct archive *a, char *fname, char *data, size_t datasize)
{
	struct archivefile	*newfile;
	ssize_t			 newoffset = -1;

	char			*compressedbuffer;
	size_t			 compressedsize, namesize;

	uint16_t		 shortnamesize;
	uint32_t		 shortcompressedsize, shortdatasize;

	int			 zstatus, status = -1;

	if (archive_hasfile(a, fname)) {
		archive_recorderror(a, "archive_addfile: %s already in archive", fname);
		goto end;
	}

	newoffset = lseek(a->archivefd, 0, SEEK_CUR);
	if (newoffset < 0) {
		archive_recorderror(a, "archive_addfile: lseek: %s", strerror(errno));
		goto end;
	}

	namesize = strlen(fname);

	if (write(a->archivefd, htons(namesize), sizeof() != namesize) {
		archive_recorderror(a, "archive_addfile: write: %s", strerror(errno));
		goto end;
	}

	else if (write(a->archivefd, 

	compressedsize = compressBound(datasize);
	compressedbuffer = reallocarray(NULL, compressedsize, sizeof(char));	

	if (compressedbuffer == NULL) {
		archive_recorderror(a, "archive_addfile: reallocarray: %s", strerror(errno));
		goto end;
	}

	

	status = 0;
end:
	if (compressedbuffer != NULL)
		free(compressedbuffer);

	if (status < 0 && newoffset > 0) {
		if (ftruncate(a->archivefd, newoffset) != newoffset)
			log_fatal("archive_addfile: ftruncate during error recovery");
	}

	return status;
}
