%{
#include <stdio.h>
#include <string.h>

#include "savelogs.h"

int line_number;

extern YYSTYPE yylval;

#define yywrap()	1
%}

%x string

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
days{0,1}		{ return DAY; }
weeks{0,1}		{ return WEEK; }
months{0,1}		{ return MONTH; }
years{0,1}		{ return YEAR; }
touch			{ return TOUCH; }
class			{ return CLASS; }
#[^\n]*\n		{ line_number++; }
\n			{ line_number++; }
[ \t]+			{ }
\"			{ sbp = string_buffer; BEGIN(string); }
<string>\"		{   *sbp = 0;
			    BEGIN(INITIAL);
			    strcpy(yytext, string_buffer);
			    yyleng = strlen(yytext);
			    return STRING; }
<string>\n		{   *sbp = 0;
			    BEGIN(INITIAL);
			    fprintf(stderr, "Unterminated string on line %d\n", line_number);
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
