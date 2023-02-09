/* imaged archive format
 * (c) jay lang, 2023ish
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/tree.h>

#include <arpa/inet.h>

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>

#include "imaged.h"

#define ARCHIVE_ERRSTRSIZE	500

struct archivefile {
	char			*name;
	size_t		 	 size;
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

	char		 	 errstr[ARCHIVE_ERRSTRSIZE];

	struct archivecache	 cachedfiles;
	RB_ENTRY(archive)	 entries;
};

static int	 archive_compare(struct archive *, struct archive *);
static int	 archive_repopulatecachedfiles(struct archive *);
static void	 archive_recorderror(struct archive *, const char *, ...);
static char	*archive_keytopath(uint32_t);

RB_HEAD(archivetree, archive);
RB_PROTOTYPE_STATIC(archivetree, archive, entries, archive_compare)

static struct archivetree	 activearchives = RB_INITIALIZER(&activearchives);



RB_GENERATE_STATIC(archivecache, archivefile, entries, archivefile_compare)
RB_GENERATE_STATIC(archivetree, archive, entries, archive_compare)

static struct archivefile *
archivefile_new(char *name, size_t offset)
{
	struct archivefile	*out;

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
	vsnprintf(a->errstr, ARCHIVE_ERRSTRSIZE, fmt, ap);
	va_end(ap);
}

static char *
archive_keytopath(uint32_t key)
{
	char	*out;

	out = asprintf(&out, "%s/%u.bundle", ARCHIVES, key);
	return out;
}

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
