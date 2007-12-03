%{
#include <stdio.h>
#include <syslog.h>

int
yyerror(char *why)
{
    extern int line_number;
    extern char *cfgfile;

    if ( isatty(fileno(stderr)) )
	fprintf(stderr, "%s line %d: %s\n", cfgfile, line_number, why);
    else
	syslog(LOG_ERR, "%s line %d: %s", cfgfile, line_number, why);
}

%}

%token COMMA PATH SIZE SAVE IN SIGNAL STRING
%token NUMBER TOKEN EVERY DAYS WEEKS YEARS
%token SIZE_SPECIFICATION TRUNCATE TOUCH CLASS

%%

stanza: PATH commands ;

commands:	commands statement
	|	/*nothing*/
	;

statement:	CLASS STRING
	|	save_in
	|	TOUCH NUMBER
	|	SIGNAL STRING
	|	SIZE SIZE_SPECIFICATION
	;

save_in:	SAVE NUMBER IN PATH
	|	SAVE IN PATH
	|	TRUNCATE
	;
