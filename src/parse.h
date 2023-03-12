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
#ifndef YYSTYPE_DEFINED
#define YYSTYPE_DEFINED
typedef union {
	char		*str;
	uint64_t	 uint;	
} YYSTYPE;
#endif /* YYSTYPE_DEFINED */
extern YYSTYPE yylval;
