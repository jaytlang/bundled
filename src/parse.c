#include <stdlib.h>
#include <string.h>
#define YYBYACC 1
#define YYMAJOR 1
#define YYMINOR 9
#define YYLEX yylex()
#define YYEMPTY -1
#define yyclearin (yychar=(YYEMPTY))
#define yyerrok (yyerrflag=0)
#define YYRECOVERING() (yyerrflag!=0)
#define YYPREFIX "yy"
#line 5 "../../src/parse.y"
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
#line 50 "../../src/parse.y"
#ifndef YYSTYPE_DEFINED
#define YYSTYPE_DEFINED
typedef union {
	char		*str;
	uint64_t	 uint;	
} YYSTYPE;
#endif /* YYSTYPE_DEFINED */
#line 55 "../../src/parse.c"
#define TOK_CHROOT 257
#define TOK_USER 258
#define TOK_ARCHIVER 259
#define TOK_SERVER 260
#define TOK_NOTARY 261
#define TOK_WORKDIR 262
#define TOK_LISTEN 263
#define TOK_ON 264
#define TOK_TIMEOUT 265
#define TOK_CA 266
#define TOK_CERT 267
#define TOK_PUBLIC 268
#define TOK_PRIVATE 269
#define TOK_KEY 270
#define TOK_INPUT 271
#define TOK_OUTPUT 272
#define TOK_MAX 273
#define TOK_NAME 274
#define TOK_FILE 275
#define TOK_SIGNATURE 276
#define TOK_FILES 277
#define TOK_SIZE 278
#define USERNAME 279
#define PATH 280
#define NUMBER 281
#define YYERRCODE 256
const short yylhs[] =
	{                                        -1,
    0,    0,    0,    0,    0,    0,    0,    0,    1,    1,
    1,    1,    1,    2,    3,    3,    3,    4,    4,    4,
    4,    4,    4,    5,
};
const short yylen[] =
	{                                         2,
    0,    4,    3,    3,    4,    4,    3,    2,    2,    4,
    4,    4,    3,    2,    2,    3,    3,    3,    2,    2,
    2,    2,    3,    2,
};
const short yydefred[] =
	{                                      1,
    0,    0,    0,    0,    0,    0,    0,    8,    0,    0,
    4,   14,   24,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    3,    7,    9,
    0,    0,    0,    0,    2,   19,    0,   20,   21,   22,
    0,    6,   15,    0,    0,    5,    0,    0,    0,   13,
   18,   23,   17,   16,   10,   11,   12,
};
const short yydgoto[] =
	{                                       1,
   16,    9,   27,   23,   10,
};
const short yysindex[] =
	{                                      0,
  -10,   -2, -267, -265, -261, -260, -258,    0,    9,   10,
    0,    0,    0, -257, -259,   11, -256, -242, -255, -253,
 -252, -245,   19, -250, -239, -238,   23,    0,    0,    0,
 -244, -243, -241, -240,    0,    0, -237,    0,    0,    0,
 -235,    0,    0, -234, -233,    0, -232, -231, -230,    0,
    0,    0,    0,    0,    0,    0,    0,};
const short yyrindex[] =
	{                                      0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,};
const short yygindex[] =
	{                                      0,
    0,    0,    0,    0,    0,
};
#define YYTABLESIZE 251
const short yytable[] =
	{                                       8,
   14,   17,   18,   24,   19,   20,   21,   11,   22,   25,
   26,   15,   12,   13,   31,   32,   33,   34,   28,   29,
   35,   37,   30,   36,   41,   38,   39,   40,   42,   43,
   44,   45,   46,   47,   48,    0,   49,    0,    0,    0,
   50,    0,    0,   51,   52,   53,   54,    0,   55,   56,
   57,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    2,    3,    4,    5,    6,
    7,
};
const short yycheck[] =
	{                                      10,
  262,  262,  263,  262,  265,  266,  267,   10,  269,  268,
  269,  273,  280,  279,  274,  275,  276,  277,   10,   10,
   10,  264,  280,  280,  270,  281,  280,  280,   10,  280,
  270,  270,   10,  278,  278,   -1,  278,   -1,   -1,   -1,
  281,   -1,   -1,  281,  280,  280,  280,   -1,  281,  281,
  281,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,  256,  257,  258,  259,  260,
  261,
};
#define YYFINAL 1
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#define YYMAXTOKEN 281
#if YYDEBUG
const char * const yyname[] =
	{
"end-of-file",0,0,0,0,0,0,0,0,0,"'\\n'",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"TOK_CHROOT","TOK_USER",
"TOK_ARCHIVER","TOK_SERVER","TOK_NOTARY","TOK_WORKDIR","TOK_LISTEN","TOK_ON",
"TOK_TIMEOUT","TOK_CA","TOK_CERT","TOK_PUBLIC","TOK_PRIVATE","TOK_KEY",
"TOK_INPUT","TOK_OUTPUT","TOK_MAX","TOK_NAME","TOK_FILE","TOK_SIGNATURE",
"TOK_FILES","TOK_SIZE","USERNAME","PATH","NUMBER",
};
const char * const yyrule[] =
	{"$accept : lines",
"lines :",
"lines : lines TOK_ARCHIVER archiver '\\n'",
"lines : lines chroot '\\n'",
"lines : lines error '\\n'",
"lines : lines TOK_NOTARY notary '\\n'",
"lines : lines TOK_SERVER server '\\n'",
"lines : lines user '\\n'",
"lines : lines '\\n'",
"archiver : TOK_WORKDIR PATH",
"archiver : TOK_MAX TOK_NAME TOK_SIZE NUMBER",
"archiver : TOK_MAX TOK_FILE TOK_SIZE NUMBER",
"archiver : TOK_MAX TOK_SIGNATURE TOK_SIZE NUMBER",
"archiver : TOK_MAX TOK_FILES NUMBER",
"chroot : TOK_CHROOT PATH",
"notary : TOK_WORKDIR PATH",
"notary : TOK_PRIVATE TOK_KEY PATH",
"notary : TOK_PUBLIC TOK_KEY PATH",
"server : TOK_LISTEN TOK_ON NUMBER",
"server : TOK_WORKDIR PATH",
"server : TOK_TIMEOUT NUMBER",
"server : TOK_CA PATH",
"server : TOK_CERT PATH",
"server : TOK_PRIVATE TOK_KEY PATH",
"user : TOK_USER USERNAME",
};
#endif
#ifdef YYSTACKSIZE
#undef YYMAXDEPTH
#define YYMAXDEPTH YYSTACKSIZE
#else
#ifdef YYMAXDEPTH
#define YYSTACKSIZE YYMAXDEPTH
#else
#define YYSTACKSIZE 10000
#define YYMAXDEPTH 10000
#endif
#endif
#define YYINITSTACKSIZE 200
/* LINTUSED */
int yydebug;
int yynerrs;
int yyerrflag;
int yychar;
short *yyssp;
YYSTYPE *yyvsp;
YYSTYPE yyval;
YYSTYPE yylval;
short *yyss;
short *yysslim;
YYSTYPE *yyvs;
unsigned int yystacksize;
int yyparse(void);
#line 151 "../../src/parse.y"

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
	RESTORE_DEFAULT(archive_maxfilesize, 1048576);
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
#line 459 "../../src/parse.c"
/* allocate initial stack or double stack size, up to YYMAXDEPTH */
static int yygrowstack(void)
{
    unsigned int newsize;
    long sslen;
    short *newss;
    YYSTYPE *newvs;

    if ((newsize = yystacksize) == 0)
        newsize = YYINITSTACKSIZE;
    else if (newsize >= YYMAXDEPTH)
        return -1;
    else if ((newsize *= 2) > YYMAXDEPTH)
        newsize = YYMAXDEPTH;
    sslen = yyssp - yyss;
#ifdef SIZE_MAX
#define YY_SIZE_MAX SIZE_MAX
#else
#define YY_SIZE_MAX 0xffffffffU
#endif
    if (newsize && YY_SIZE_MAX / newsize < sizeof *newss)
        goto bail;
    newss = (short *)realloc(yyss, newsize * sizeof *newss);
    if (newss == NULL)
        goto bail;
    yyss = newss;
    yyssp = newss + sslen;
    if (newsize && YY_SIZE_MAX / newsize < sizeof *newvs)
        goto bail;
    newvs = (YYSTYPE *)realloc(yyvs, newsize * sizeof *newvs);
    if (newvs == NULL)
        goto bail;
    yyvs = newvs;
    yyvsp = newvs + sslen;
    yystacksize = newsize;
    yysslim = yyss + newsize - 1;
    return 0;
bail:
    if (yyss)
            free(yyss);
    if (yyvs)
            free(yyvs);
    yyss = yyssp = NULL;
    yyvs = yyvsp = NULL;
    yystacksize = 0;
    return -1;
}

#define YYABORT goto yyabort
#define YYREJECT goto yyabort
#define YYACCEPT goto yyaccept
#define YYERROR goto yyerrlab
int
yyparse(void)
{
    int yym, yyn, yystate;
#if YYDEBUG
    const char *yys;

    if ((yys = getenv("YYDEBUG")))
    {
        yyn = *yys;
        if (yyn >= '0' && yyn <= '9')
            yydebug = yyn - '0';
    }
#endif /* YYDEBUG */

    yynerrs = 0;
    yyerrflag = 0;
    yychar = (-1);

    if (yyss == NULL && yygrowstack()) goto yyoverflow;
    yyssp = yyss;
    yyvsp = yyvs;
    *yyssp = yystate = 0;

yyloop:
    if ((yyn = yydefred[yystate]) != 0) goto yyreduce;
    if (yychar < 0)
    {
        if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, reading %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
    }
    if ((yyn = yysindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: state %d, shifting to state %d\n",
                    YYPREFIX, yystate, yytable[yyn]);
#endif
        if (yyssp >= yysslim && yygrowstack())
        {
            goto yyoverflow;
        }
        *++yyssp = yystate = yytable[yyn];
        *++yyvsp = yylval;
        yychar = (-1);
        if (yyerrflag > 0)  --yyerrflag;
        goto yyloop;
    }
    if ((yyn = yyrindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
        yyn = yytable[yyn];
        goto yyreduce;
    }
    if (yyerrflag) goto yyinrecovery;
#if defined(__GNUC__)
    goto yynewerror;
#endif
yynewerror:
    yyerror("syntax error");
#if defined(__GNUC__)
    goto yyerrlab;
#endif
yyerrlab:
    ++yynerrs;
yyinrecovery:
    if (yyerrflag < 3)
    {
        yyerrflag = 3;
        for (;;)
        {
            if ((yyn = yysindex[*yyssp]) && (yyn += YYERRCODE) >= 0 &&
                    yyn <= YYTABLESIZE && yycheck[yyn] == YYERRCODE)
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: state %d, error recovery shifting\
 to state %d\n", YYPREFIX, *yyssp, yytable[yyn]);
#endif
                if (yyssp >= yysslim && yygrowstack())
                {
                    goto yyoverflow;
                }
                *++yyssp = yystate = yytable[yyn];
                *++yyvsp = yylval;
                goto yyloop;
            }
            else
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: error recovery discarding state %d\n",
                            YYPREFIX, *yyssp);
#endif
                if (yyssp <= yyss) goto yyabort;
                --yyssp;
                --yyvsp;
            }
        }
    }
    else
    {
        if (yychar == 0) goto yyabort;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, error recovery discards token %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
        yychar = (-1);
        goto yyloop;
    }
yyreduce:
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: state %d, reducing by rule %d (%s)\n",
                YYPREFIX, yystate, yyn, yyrule[yyn]);
#endif
    yym = yylen[yyn];
    if (yym)
        yyval = yyvsp[1-yym];
    else
        memset(&yyval, 0, sizeof yyval);
    switch (yyn)
    {
case 4:
#line 63 "../../src/parse.y"
{ ++errors; yyerrok; }
break;
case 9:
#line 70 "../../src/parse.y"
{
		char *absolutepath;

		absolutepath = chrooted_to_canonical(yyvsp[0].str);
		if (absolutepath == NULL) YYERROR;

		else {
			free(config.archive_path);
			config.archive_path = absolutepath;
		}
	}
break;
case 10:
#line 82 "../../src/parse.y"
{ config.archive_maxnamesize = yyvsp[0].uint; }
break;
case 11:
#line 83 "../../src/parse.y"
{ config.archive_maxfilesize = yyvsp[0].uint; }
break;
case 12:
#line 84 "../../src/parse.y"
{ config.archive_maxsignaturesize = yyvsp[0].uint; }
break;
case 13:
#line 85 "../../src/parse.y"
{ config.archive_maxfiles = yyvsp[0].uint; }
break;
case 14:
#line 88 "../../src/parse.y"
{ config.chroot = absolute_to_canonical(yyvsp[0].str); }
break;
case 15:
#line 91 "../../src/parse.y"
{
		char *absolutepath;

		absolutepath = chrooted_to_canonical(yyvsp[0].str);
		if (absolutepath == NULL) YYERROR;

		else {
			free(config.signature_path);
			config.signature_path = absolutepath;
		}

	}
break;
case 16:
#line 104 "../../src/parse.y"
{ config.signature_privkey = absolute_to_canonical(yyvsp[0].str); }
break;
case 17:
#line 105 "../../src/parse.y"
{ config.signature_pubkey = absolute_to_canonical(yyvsp[0].str); }
break;
case 18:
#line 108 "../../src/parse.y"
{
		if (yyvsp[0].uint >= UINT16_MAX) {
			yyerror("requested listen on port number %llu, which "
				"is too high a port number to work!", yyvsp[0].uint);
			YYERROR;
		}

		config.server_port = (uint16_t)yyvsp[0].uint;
	}
break;
case 19:
#line 118 "../../src/parse.y"
{
		char *absolutepath;

		absolutepath = chrooted_to_canonical(yyvsp[0].str);
		if (absolutepath == NULL) YYERROR;

		else {
			free(config.message_path);
			config.message_path = absolutepath;
		}
	}
break;
case 20:
#line 130 "../../src/parse.y"
{
		if (yyvsp[0].uint >= UINT32_MAX) {
			yyerror("configured timeout is _unreasonably_ large...");
			YYERROR;
		} else if (yyvsp[0].uint == 0) {
			yyerror("tried to configure zero timeout for server connections");
			YYERROR;
		}

		config.server_timeout = (uint32_t)yyvsp[0].uint;
	}
break;
case 21:
#line 142 "../../src/parse.y"
{ config.server_ca_path = absolute_to_canonical(yyvsp[0].str); }
break;
case 22:
#line 143 "../../src/parse.y"
{ config.server_cert_path = absolute_to_canonical(yyvsp[0].str); }
break;
case 23:
#line 144 "../../src/parse.y"
{ config.server_key_path = absolute_to_canonical(yyvsp[0].str); }
break;
case 24:
#line 147 "../../src/parse.y"
{ config.user = yyvsp[0].str; }
break;
#line 768 "../../src/parse.c"
    }
    yyssp -= yym;
    yystate = *yyssp;
    yyvsp -= yym;
    yym = yylhs[yyn];
    if (yystate == 0 && yym == 0)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: after reduction, shifting from state 0 to\
 state %d\n", YYPREFIX, YYFINAL);
#endif
        yystate = YYFINAL;
        *++yyssp = YYFINAL;
        *++yyvsp = yyval;
        if (yychar < 0)
        {
            if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
            if (yydebug)
            {
                yys = 0;
                if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
                if (!yys) yys = "illegal-symbol";
                printf("%sdebug: state %d, reading %d (%s)\n",
                        YYPREFIX, YYFINAL, yychar, yys);
            }
#endif
        }
        if (yychar == 0) goto yyaccept;
        goto yyloop;
    }
    if ((yyn = yygindex[yym]) && (yyn += yystate) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yystate)
        yystate = yytable[yyn];
    else
        yystate = yydgoto[yym];
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: after reduction, shifting from state %d \
to state %d\n", YYPREFIX, *yyssp, yystate);
#endif
    if (yyssp >= yysslim && yygrowstack())
    {
        goto yyoverflow;
    }
    *++yyssp = yystate;
    *++yyvsp = yyval;
    goto yyloop;
yyoverflow:
    yyerror("yacc stack overflow");
yyabort:
    if (yyss)
            free(yyss);
    if (yyvs)
            free(yyvs);
    yyss = yyssp = NULL;
    yyvs = yyvsp = NULL;
    yystacksize = 0;
    return (1);
yyaccept:
    if (yyss)
            free(yyss);
    if (yyvs)
            free(yyvs);
    yyss = yyssp = NULL;
    yyvs = yyvsp = NULL;
    yystacksize = 0;
    return (0);
}
