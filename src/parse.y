/* bundled configuration parser
 * (c) jay lang, 2023
 */
%{
#include <sys/types.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "bundled.h"
#include "parse.h"

extern FILE	*yyin;
extern uint64_t	 lineno;

static char	*filename = NULL;
static uint64_t	 errors = 0;
static int	 config_alive = 0;


struct config	 config = { 0 };


static char	*chrooted_to_canonical(char *);
static int	 path_contains(char *, char *);

static int	 config_validate(void);
static void	 config_teardown(void);
static void	 config_setdefaults(void);

extern int yylex(void);
extern int yyerror(const char *, ...);
%}

%token TOK_CHROOT TOK_USER
%token TOK_ARCHIVER TOK_SERVER TOK_NOTARY TOK_WORKDIR

%token TOK_LISTEN TOK_ON TOK_TIMEOUT TOK_CA TOK_CERT
%token TOK_PUBLIC TOK_PRIVATE TOK_KEY

%token TOK_INPUT TOK_OUTPUT

%token TOK_MAX TOK_NAME TOK_FILE TOK_SIGNATURE TOK_FILES TOK_SIZE

%union {
	char		*str;
	uint64_t	 uint;	
}

%token <str> USERNAME
%token <str> PATH
%token <uint> NUMBER

%%
lines	: /* dead */
	| lines TOK_ARCHIVER archiver '\n'
	| lines chroot '\n'
	| lines error '\n'			{ ++errors; yyerrok; }
	| lines TOK_NOTARY notary '\n'
	| lines TOK_SERVER server '\n'
	| lines user '\n'
	| lines '\n'
	;

archiver: TOK_WORKDIR PATH {
		char *absolutepath;

		absolutepath = chrooted_to_canonical($2);
		if (absolutepath == NULL) YYERROR;

		else {
			free(config.archive_path);
			config.archive_path = absolutepath;
		}
	}

	| TOK_MAX TOK_NAME TOK_SIZE NUMBER	{ config.archive_maxnamesize = $4; }
	| TOK_MAX TOK_FILE TOK_SIZE NUMBER	{ config.archive_maxfilesize = $4; }
	| TOK_MAX TOK_SIGNATURE TOK_SIZE NUMBER	{ config.archive_maxsignaturesize = $4; }
	| TOK_MAX TOK_FILES NUMBER		{ config.archive_maxfiles = $3; }
	;

chroot	: TOK_CHROOT PATH			{ config.chroot = absolute_to_canonical($2); }
	;

notary	: TOK_WORKDIR PATH {
		char *absolutepath;

		absolutepath = chrooted_to_canonical($2);
		if (absolutepath == NULL) YYERROR;

		else {
			free(config.signature_path);
			config.signature_path = absolutepath;
		}

	}

	| TOK_PRIVATE TOK_KEY PATH		{ config.signature_privkey = absolute_to_canonical($3); }
	| TOK_PUBLIC TOK_KEY PATH		{ config.signature_pubkey = absolute_to_canonical($3); }
	;

server	: TOK_LISTEN TOK_ON NUMBER {
		if ($3 >= UINT16_MAX) {
			yyerror("requested listen on port number %llu, which "
				"is too high a port number to work!", $3);
			YYERROR;
		}

		config.server_port = (uint16_t)$3;
	}

	| TOK_WORKDIR PATH {
		char *absolutepath;

		absolutepath = chrooted_to_canonical($2);
		if (absolutepath == NULL) YYERROR;

		else {
			free(config.message_path);
			config.message_path = absolutepath;
		}
	}

	| TOK_TIMEOUT NUMBER {
		if ($2 >= UINT32_MAX) {
			yyerror("configured timeout is _unreasonably_ large...");
			YYERROR;
		} else if ($2 == 0) {
			yyerror("tried to configure zero timeout for server connections");
			YYERROR;
		}

		config.server_timeout = (uint32_t)$2;
	}

	| TOK_CA PATH 				{ config.server_ca_path = absolute_to_canonical($2); }
	| TOK_CERT PATH				{ config.server_cert_path = absolute_to_canonical($2); }
	| TOK_PRIVATE TOK_KEY PATH		{ config.server_key_path = absolute_to_canonical($3); }
	;

user	: TOK_USER USERNAME			{ config.user = $2; }
	;

%%

static char *
absolute_to_canonical(char *absolute)
{
	char	*canonical = NULL;

	canonical = malloc(PATH_MAX);
	if (canonical == NULL)
		err(1, "malloc PATH_MAX bytes for canonical path name");

	if (realpath(absolute, canonical) == NULL) {
		yyerror("unable to resolve path %s: %s", absolute, strerror(errno));
		exit(1);
	}

	free(absolute);
	return canonical;
}

static char *
chrooted_to_canonical(char *chrooted)
{
	char	*absolute, *canonical = NULL;

	if (config.chroot == NULL) {
		yyerror("tried to configure archiver working directory without first "
			"specifying bundled chroot directory - try adding 'chroot "
			"/var/bundled' to the top of your configuration file");
		goto end;
	}

	if (asprintf(&absolute, "%s/%s", config.chroot, chrooted) < 0)
		err(1, "asprintf for chrooted -> absolute path");

	canonical = malloc(PATH_MAX);
	if (canonical == NULL)
		err(1, "malloc PATH_MAX bytes for canonical path name");

	if (realpath(absolute, canonical) == NULL) {
		yyerror("unable to resolve path %s: %s", absolute, strerror(errno));
		exit(1);
	}

	free(chrooted);
	free(absolute);
end:
	return canonical;
}

/* XXX: this isn't a perfect function. but it works well enough.
 * if you start using .. in your configuration files, this will erroneously
 * pass, but defense-in-depth with chrooting + unveil means that things
 * won't malfunction too badly
 */
static int
path_contains(char *haystack, char *needle)
{
	if (strcmp(haystack, "/") == 0) return 1;

	if (strlen(haystack) >= strlen(needle)) return 0;
	else if (memcmp(needle, haystack, strlen(haystack)) != 0) return 0;

	return 1;
}

#define RESTORE_DEFAULT(K, V) config.K = V

#define RESTORE_DEFAULT_STR(K, V) do {					\
	config.K = strdup(V);						\
	if (config.K == NULL)						\
		err(1, "strdup configuration default for key %s", #K);	\
} while (0)

static void
config_setdefaults(void)
{
	RESTORE_DEFAULT_STR(user, "_bundled");

	RESTORE_DEFAULT_STR(archive_path, "/var/bundled/archives");
	RESTORE_DEFAULT_STR(message_path, "/var/bundled/messages");
	RESTORE_DEFAULT_STR(signature_path, "/var/bundled/signatures");

	RESTORE_DEFAULT(server_port, 443);	
	RESTORE_DEFAULT(server_timeout, 1);

	RESTORE_DEFAULT_STR(server_ca_path, "/etc/ssl/authority");
	RESTORE_DEFAULT_STR(server_cert_path, "/etc/ssl/server.pem");
	RESTORE_DEFAULT_STR(server_key_path, "/etc/ssl/private/server.key");

	RESTORE_DEFAULT_STR(signature_privkey, "/etc/signify/bundled.sec");
	RESTORE_DEFAULT_STR(signature_pubkey, "/etc/signify/bundled.pub");

	RESTORE_DEFAULT(archive_maxnamesize, 1024);
	RESTORE_DEFAULT(archive_maxfilesize, 10485760);
	RESTORE_DEFAULT(archive_maxsignaturesize, 177);
	RESTORE_DEFAULT(archive_maxfiles, 100);
}

/* XXX: the wording of this could be better for sure... */
#define CHECK_PATH_CONTAINS(H, N) do {						\
	if (!path_contains(H, N)) {						\
		warnx(#H "= %s does not (conclusively) contain " #N " = %s",	\
			H, N);							\
		warnx("i know this is a shitty error message, "			\
			"ctrl+f the source and you'll see what this "		\
			"means i promise");					\
		return -1;							\
	}									\
} while (0)

static int
config_validate(void)
{
	if (!config_alive) return -1;
	else if (errors > 0) return -1;

	if (config.chroot == NULL) {
		yyerror("chroot for bundled not specified in configuration file. "
			"try placing 'chroot /var/bundled' at the top of the file "
			"and trying again...");
		return -1;
	}

	CHECK_PATH_CONTAINS(config.chroot, config.archive_path);
	CHECK_PATH_CONTAINS(config.chroot, config.message_path);
	CHECK_PATH_CONTAINS(config.chroot, config.signature_path);

	return 0;
}

static void
config_teardown(void)
{
	if (config_alive) {
		free(config.chroot);
		free(config.user);
	
		free(config.archive_path);
		free(config.message_path);
		free(config.signature_path);
	
		free(config.server_ca_path);
		free(config.server_cert_path);
		free(config.server_key_path);
	
		free(config.signature_privkey);
		free(config.signature_pubkey);
	
		bzero(&config, sizeof(struct config));
	
		config_alive = 0;
	}
}

int
yyerror(const char *fmt, ...)
{
	char 	*stem;
	int	 stemout;

	va_list ap;
	va_start(ap, fmt);

	stemout = asprintf(&stem, "%s near line %llu: %s", filename, lineno, fmt);
	if (stemout < 0) vwarnx(fmt, ap);
	else {
		vwarnx(stem, ap);
		free(stem);
	}

	va_end(ap);
	return 0;
}

int
yywrap(void)
{
	return 1;
}

void
config_parse(char *fpath)
{
	config_teardown();

	filename = fpath;
	errors = 0;

	config_setdefaults();
	config_alive = 1;
	lineno = 1;

	if (fpath != NULL) {
		yyin = fopen(filename, "r");
		if (yyin == NULL) err(1, "fopen %s", filename);

		yyparse();
		fclose(yyin);

	} else RESTORE_DEFAULT_STR(chroot, "/var/bundled");

	if (config_validate() < 0) {
		config_teardown();
		errx(1, "%llu errors detected in configuration", errors);
	}
}
