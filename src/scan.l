/* bundled configuration 
 * (c) jay lang, 2023
 */

%{
#include <sys/types.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "bundled.h"
#include "parse.h"

static uint64_t	sizetobytes(char *);

extern YYSTYPE	yylval;
uint64_t	lineno;
%}

%%
chroot			return TOK_CHROOT;
user			return TOK_USER;

archiver		return TOK_ARCHIVER;
server			return TOK_SERVER;
notary			return TOK_NOTARY;
workdir			return TOK_WORKDIR;

listen			return TOK_LISTEN;
on			return TOK_ON;
timeout			return TOK_TIMEOUT;
ca			return TOK_CA;
certificate		return TOK_CERT;
public			return TOK_PUBLIC;
private			return TOK_PRIVATE;
key			return TOK_KEY;

input			return TOK_INPUT;
output			return TOK_OUTPUT;

max			return TOK_MAX;
name			return TOK_NAME;
file			return TOK_FILE;
signature		return TOK_SIGNATURE;
files			return TOK_FILES;
size			return TOK_SIZE;

[0-9]+			yylval.uint = (uint64_t)strtoull(yytext, NULL, 10); return NUMBER;
[0-9]+[KMG]		yylval.uint = sizetobytes(yytext); return NUMBER;
[a-z_][a-z0-9_-]{0,31}	yylval.str = strdup(yytext); return USERNAME;
\"[^\0\n\"]+\"		yylval.str = strdup(yytext + 1); yylval.str[strlen(yytext) - 2] = '\0'; return PATH;

#[^\0\n]*		/* dead */
[\t ]+			/* dead */

\n			++lineno; return '\n';
.			return (int)*yytext;
%%


static uint64_t
sizetobytes(char *size)
{
	uint64_t	realsize;
	char		unit;	

	unit = size[strlen(size) - 1];

	size[strlen(size) - 1] = '\0';
	realsize = (uint64_t)strtoull(size, NULL, 10);

	switch (unit) {
	case 'K': realsize *= 1024; break;
	case 'M': realsize *= 1048576; break;
	case 'G': realsize *= 1073741824; break;
	}

	return realsize;
}
