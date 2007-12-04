%{
#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "savelogs.h"

extern int dotted_backup;
extern int backup_suffix;
extern int compress_them;

extern char yytext[];
FILE *yyin;

log_t this;
log_t *files = 0;
job_t *jobs = 0;

int
yyerror(char *why)
{
    extern int line_number;
    extern char *cfgfile;

    if ( isatty(fileno(stderr)) )
	fprintf(stderr, "%s line %d: %s\n", cfgfile, line_number, why);
    else
	syslog(LOG_ERR, "%s line %d: %s", cfgfile, line_number, why);
    exit(1);
}


char *
xstrdup(char *s)
{
    char *t = strdup(s);

    if ( !t ) yyerror("out of memory");

    return t;
}


void
mkadd()
{
    log_t *new = calloc(1, sizeof *new);

    if ( new ) {
	memcpy(new, &this, sizeof this);

	new->next = files;
	files = new;
    }
    else
	yyerror("out of memory");
}


void
mksave(char *path)
{
    bzero(&this, sizeof this);

    this.path = xstrdup(path);
}

void
mkclass(char *class)
{
    if ( this.class ) yyerror("duplicate class");
    this.class = xstrdup(class);
}

void
mktouch(char *str)
{
    int mode = strtoul(str, 0, 8);

    if ( this.touch ) yyerror("duplicate touch");
    if ( mode == 0 ) yyerror("touch mode may not be zero");
    this.touch = mode;
}

long
specify(char *val)
{
    char *last;
    unsigned long size = 0;

    size = strtoul(val, &last, 10);

    if ( last == val ) yyerror("size parse error");
    else if ( size == 0 ) yyerror("size limit must be > zero");

    switch (*last) {
    case 'g':
    case 'G':   size *= 1024;
    case 'm':
    case 'M':   size *= 1024;
    case 'k':
    case 'K':   size *= 1024;
		break;
    default:	yyerror("unknown size suffix");
    }
    return size;
}

void
mksize(long size)
{
    if ( this.size ) yyerror("duplicate size");
    if ( size == 0 ) yyerror("size must be > zero");
    this.size = size;
}

static backup_t*
newbackup()
{
    backup_t* ret = calloc(1, sizeof *ret);

    if ( !ret ) yyerror("out of memory");

    return ret;
}

void
mkbackup(int number, char *destdir)
{
    struct stat st;

    if ( this.backup ) yyerror("duplicate backup");

    if ( (this.backup = newbackup()) &&  destdir ) {
	if ( stat(destdir, &st) == -1 ) yyerror(destdir);
	else if ( !S_ISDIR(st.st_mode) ) yyerror("not a directory");
	this.backup->dir = xstrdup(destdir);
	this.backup->count = number;
    }
}

void
mksignal(char *sig)
{
    job_t *jot;
    int idx;
    char *p, *q;


    for (p=sig; isspace(*p); ++p)
	;

    for (idx = strlen(p)+1; idx && isspace(p[idx-1]); --idx)
	;

    p[idx] = 0;


    /* add a new job entry
     */
    idx = NR(this.jobs);
    GROW(this.jobs);

    /* see if the job is already listed
     */
    for (jot = jobs; jot; jot = jot->next)
	if ( strcmp(sig, jot->command) == 0 )
	    break;

    if (jot)
	jot->links++;
    else if ( (jot = calloc(1, sizeof *jot)) != 0 ) {
	jot->command = xstrdup(sig);
	jot->links = 1;
	jot->next = jobs;
	jobs = jot;
    }
    else
	yyerror("out of memory");

    IT(this.jobs,idx) = jot;
}


void
mkinterval(interval)
{
    if (this.interval) yyerror("duplicate interval");
    this.interval = interval;
}

%}

%token COMMA PATH SIZE SAVE IN SIGNAL STRING
%token NUMBER TOKEN EVERY DAY WEEK MONTH YEAR
%token SIZE_SPECIFICATION TRUNCATE TOUCH CLASS
%token SET DOTS SUFFIX COMPRESSED

%%

config:		config stanza
	|	/*nothing*/

stanza:		prefix commands
		{ mkadd(); }
	|	SET option
	;

option:		DOTS
		{ dotted_backup = 1; }
	|	SUFFIX
		{ backup_suffix = 1; }
	|	COMPRESSED
		{ compress_them = 1; }
	;

prefix:		PATH
		{ mksave(yytext); }

commands:	commands statement
	|	/*nothing*/
	;

statement:	CLASS STRING
		{ mkclass(yytext); }
	|	TOUCH NUMBER
		{ mktouch(yytext); }
	|	SIGNAL STRING
		{ mksignal(yytext); }
	|	SIZE SIZE_SPECIFICATION
		{ mksize(specify(yytext)); }
	|	EVERY every
	|	save_in
	;

every:		WEEK
		{ mkinterval(WEEK); }
	|	MONTH
		{ mkinterval(MONTH); }
	|	YEAR
		{ mkinterval(YEAR); }
	;

save_in:	SAVE NUMBER IN PATH
		{ mkbackup($2,yytext); }
	|	SAVE IN PATH
		{ mkbackup(1, yytext); }
	|	TRUNCATE
		{ mkbackup(0, 0); }
	;
