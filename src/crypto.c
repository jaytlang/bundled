/* bundled archive authenticity verification
 * (c) jay lang, 2023
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <arpa/inet.h>

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "bundled.h"

#define INPUT_FNAME		"crc32.bin"
#define OUTPUT_FNAME	"crc32.sig"

static char	*signatureinpath = NULL, *signatureoutpath = NULL;

static void	 crc32_write(char *, uint32_t);
static void	 crc32_teardown(char *);

static void	 signify_sign(char *, char *);
static int	 signify_verify(char *, char *);

static char	*signature_read(char *);
static void	 signature_write(char *, char *);
static void	 signature_teardown(char *);

static void
crc32_write(char *path, uint32_t crc)
{
	uint32_t	becrc;
	int 		fd;
	int		flags = O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC;
	mode_t		mode = S_IRUSR | S_IRGRP;

	if ((fd = open(path, flags, mode)) < 0)
		log_fatal("write_crc32: open");

	becrc = htonl(crc);

	if (write(fd, &becrc, sizeof(uint32_t)) < 0)
		log_fatal("write_crc32: write");

	close(fd);
}

static void
crc32_teardown(char *path)
{
	if (unlink(path) < 0)
		log_fatal("crc32_teardown: unlink");
}

static void
signify_sign(char *message, char *signature)
{
	int	wstatus;
	pid_t	pid;

	if ((pid = fork()) < 0)
		log_fatal("signify_sign: fork");

	else if (pid == 0) {
		execl(CRYPTO_SIGNIFY, CRYPTO_SIGNIFY, "-S", "-n",
			"-x", signature,
			"-s", CRYPTO_SECKEY,
			"-m", message, NULL);

		log_fatal("signify_sign: execl");
	}

	do {
		if (waitpid(pid, &wstatus, 0) < 0)
			log_fatal("signify_sign: waitpid");

	} while (!WIFEXITED(wstatus) && !WIFSIGNALED(wstatus));

	if (WIFSIGNALED(wstatus))
		log_fatalx("signify_sign: signify terminated by signal %d", WTERMSIG(wstatus));
	else if (WEXITSTATUS(wstatus) != 0)
		log_fatalx("signify_sign: signify exited with status %d", WEXITSTATUS(wstatus));

}

static int
signify_verify(char *message, char *signature)
{
	int	wstatus, result = -1;
	pid_t	pid;

	if ((pid = fork()) < 0)
		log_fatal("signify_verify: fork");

	else if (pid == 0) {
		freopen("/dev/null", "a", stdout);
		freopen("/dev/null", "a", stderr);

		execl(CRYPTO_SIGNIFY, CRYPTO_SIGNIFY, "-V", "-q",
			"-x", signature,
			"-p", CRYPTO_PUBKEY,
			"-m", message, NULL);

		log_fatal("signify_verify: execl");
	}

	do {
		if (waitpid(pid, &wstatus, 0) < 0)
			log_fatal("signify_verify: waitpid");

	} while (!WIFEXITED(wstatus) && !WIFSIGNALED(wstatus));

	if (WIFSIGNALED(wstatus))
		log_fatalx("signify_verify: signify terminated by signal %d", WTERMSIG(wstatus));
	else if (WEXITSTATUS(wstatus) == 0)
		result = 0;

	return result;
}

static char *
signature_read(char *path)
{
	int	 fd, flags = O_RDONLY | O_CLOEXEC;
	ssize_t	 bufsize;
	char	*bufout;

	if ((fd = open(path, flags)) < 0)
		log_fatal("signature_read: open");

	if ((bufsize = lseek(fd, 0, SEEK_END)) < 0 || lseek(fd, 0, SEEK_SET) < 0)
		log_fatal("signature_read: lseek");

	bufout = reallocarray(NULL, bufsize + 1, sizeof(char));
	if (bufout == NULL)
		log_fatal("signature_read: reallocarray");

	if (read(fd, bufout, (size_t)bufsize) < 0)
		log_fatal("signature_read: read");

	close(fd);

	bufout[bufsize] = '\0';
	return bufout;
}

static void
signature_write(char *signature, char *path)
{
	size_t		signaturesize;
	int		fd, flags = O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC;
	mode_t		mode = S_IRUSR | S_IRGRP;

	signaturesize = strnlen(signature, MAXSIGSIZE);
	if (signature[signaturesize] != '\0')
		log_fatalx("signature_write: non-nul-terminated signature");

	if ((fd = open(path, flags, mode)) < 0)
		log_fatal("signature_write: open");

	if (write(fd, signature, signaturesize) < 0)
		log_fatal("signature_write: write");

	close(fd);
}

static void
signature_teardown(char *path)
{
	if (unlink(path) < 0)
		log_fatal("signature_teardown: unlink");
}

char *
crypto_takesignature(struct archive *a)
{
	char		*signature;
	uint32_t	 crc32 = archive_getcrc32(a);

	if (signatureinpath == NULL) {
		if (asprintf(&signatureinpath, "%s/%s", SIGNATURES, INPUT_FNAME) < 0)
			log_fatal("asprintf signature input filepath");
		if (asprintf(&signatureoutpath, "%s/%s", SIGNATURES, OUTPUT_FNAME) < 0)
			log_fatal("asprintf signature output filepath");
	}

	crc32_write(signatureinpath, crc32);
	signify_sign(signatureinpath, signatureoutpath);

	signature = signature_read(signatureoutpath);

	crc32_teardown(signatureinpath);
	signature_teardown(signatureoutpath);

	return signature;
}

int
crypto_verifysignature(struct archive *a)
{
	char		*signature = archive_getsignature(a);
	uint32_t	 crc32 = archive_getcrc32(a);
	int		 status = -1;

	if (signatureinpath == NULL) {
		if (asprintf(&signatureinpath, "%s/%s", SIGNATURES, INPUT_FNAME) < 0)
			log_fatal("asprintf signature input filepath");
		if (asprintf(&signatureoutpath, "%s/%s", SIGNATURES, OUTPUT_FNAME) < 0)
			log_fatal("asprintf signature output filepath");
	}

	if (strnlen(signature, MAXSIGSIZE) == 0)
		goto end;

	crc32_write(signatureinpath, crc32);
	signature_write(signature, signatureoutpath);

	status = signify_verify(signatureinpath, signatureoutpath);

	crc32_teardown(signatureinpath);
	signature_teardown(signatureoutpath);
	
end:
	free(signature);
	return status;
}
