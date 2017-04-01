%{
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "savelogs.h"

int line_number;

extern YYSTYPE yylval;
extern void yyerror(char*);

%}

%x string
%x qfile

DIGIT	[0-9]
FILECHR	[_\-a-zA-Z0-9\.\/]

%%
    char string_buffer[1024];
    char *sbp;

in			{ return IN; }
size			{ return SIZE; }
every			{ return EVERY; }
signal			{ return SIGNAL; }
save			{ return SAVE; }
truncate		{ return TRUNCATE; }
days?			{ return DAY; }
weeks?			{ return WEEK; }
months?			{ return MONTH; }
years?			{ return YEAR; }
touch			{ return TOUCH; }
class			{ return CLASS; }
set			{ return SET; }
dot(s|ted)?		{ return yylval = DOTS; }
number(s|ed)?		{ return yylval = NUMBERED; }
suffix			{ return yylval = SUFFIX; }
prefix			{ return yylval = PREFIX; }
compress(ed)?		{ return yylval = COMPRESS; }
uncompress(ed)?		{ return yylval = PLAIN; }
#[^\n]*\n		{ line_number++; }
\n			{ line_number++; }
[ \t]+			{ }

\[			{   sbp = string_buffer; BEGIN(qfile); }
<qfile>\]		{   *sbp = 0;
			    BEGIN(INITIAL);
			    strcpy(yytext, string_buffer);
			    yyleng = strlen(yytext);
			    return PATH;
			}
<qfile>\"		{   *sbp = 0;
			    BEGIN(INITIAL);
			    yyerror("Unterminated []");
			    strcpy(yytext, string_buffer);
			    yyleng = strlen(yytext);
			    line_number++;
			    return PATH;
			}
<qfile>\\.		{   *sbp++ = yytext[1]; }
<qfile>.		{   *sbp++ = yytext[0]; }

\"			{   sbp = string_buffer; BEGIN(string); }
<string>\"		{   *sbp = 0;
			    BEGIN(INITIAL);
			    strcpy(yytext, string_buffer);
			    yyleng = strlen(yytext);
			    return TEXT; }
<string>\n		{   *sbp = 0;
			    BEGIN(INITIAL);
			    yyerror("Unterminated string");
			    strcpy(yytext, string_buffer);
			    yyleng = strlen(yytext);
			    line_number++;
			    return TEXT;
			}
<string>\\.		{  *sbp++ = yytext[1]; }
<string>.		{  *sbp++ = yytext[0]; }
{DIGIT}+[mMkK]		{ return SIZE_SPECIFICATION; }
{DIGIT}+		{ yylval = atoi(yytext); return NUMBER; }
\/{FILECHR}+		{ return PATH; }
.			{ }

%%

int yywrap() { return 1; }
