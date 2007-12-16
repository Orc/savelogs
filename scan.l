%{
#include <stdio.h>
#include <string.h>

#include "savelogs.h"

int line_number;

extern YYSTYPE yylval;

#define yywrap()	1
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
dots			{ return yylval = DOTS; }
suffix			{ return yylval = SUFFIX; }
compressed		{ return yylval = COMPRESSED; }
number(ed)?		{ return yylval = NUMBERED; }
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
			    return STRING; }
<string>\n		{   *sbp = 0;
			    BEGIN(INITIAL);
			    yyerror("Unterminated string");
			    strcpy(yytext, string_buffer);
			    yyleng = strlen(yytext);
			    line_number++;
			    return STRING;
			}
<string>\\.		{  *sbp++ = yytext[1]; }
<string>.		{  *sbp++ = yytext[0]; }
{DIGIT}+[mMkK]		{ return SIZE_SPECIFICATION; }
{DIGIT}+		{ yylval = atoi(yytext); return NUMBER; }
\/{FILECHR}+		{ return PATH; }
.			{ }
