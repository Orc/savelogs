%{
#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>

#include "savelogs.h"

extern Flag dotted_backup;
extern Flag backup_suffix;
extern Flag compress_them;

extern char yytext[];
FILE *yyin;

log_t this;
log_t *files = 0;
signal_t *signals = 0;

/*
 * whine bitterly, then die on the spot.   No error correction or
 * anything fancy here!
 */
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


/*
 * duplicate a string or die trying
 */
char *
xstrdup(char *s)
{
    char *t = strdup(s);

    if ( !t ) yyerror("out of memory");

    return t;
}


/*
 * add the current log_t to the list of already compiled ones
 */
void
mkadd()
{
    log_t *new = calloc(1, sizeof *new);

    if ( this.class == 0 ) this.class = "*";
    
    if ( !this.compress_them ) this.compress_them = compress_them;
    if ( !this.dotted_backup ) this.dotted_backup = dotted_backup;
    if ( !this.backup_suffix ) this.backup_suffix = backup_suffix;
    
    if ( new ) {
	memcpy(new, &this, sizeof this);

	new->next = files;
	files = new;
    }
    else
	yyerror("out of memory");
}


/*
 * the first thing to do when building a log_t is to zero
 * it and assign the pathname into it.
 */
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
sizeandunits(char *val)
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

    if ( this.backup && this.backup->count ) yyerror("duplicate backup");

    if ( (this.backup = newbackup()) &&  destdir ) {
	if ( stat(destdir, &st) == -1 ) yyerror(destdir);
	else if ( !S_ISDIR(st.st_mode) ) yyerror("not a directory");
	this.backup->dir = xstrdup(destdir);
	this.backup->count = number;
    }
}


/*
 * add a new signal/link to an existing signal 
 */
void
mksignal(char *sig)
{
    signal_t *jot;
    int idx;
    char *p, *q;


    /* signals are unix command lines, so leading and
     * trailing whitespace isn't significant.   So trim
     * it away.
     */
    for (p=sig; isspace(*p); ++p)
	;

    for (idx = strlen(p)+1; idx && isspace(p[idx-1]); --idx)
	;

    p[idx] = 0;


    /* then see if the signal is already listed in the global
     * signal list.   If it is, just bump the linkcount, otherwise
     * link a new signal into the list.
     */
    for (jot = signals; jot; jot = jot->next)
	if ( strcmp(sig, jot->command) == 0 )
	    break;

    if (jot)
	jot->links++;
    else if ( (jot = calloc(1, sizeof *jot)) != 0 ) {
	jot->command = xstrdup(sig);
	jot->links = 1;
	jot->next = signals;
	signals = jot;
    }
    else
	yyerror("out of memory");

    /* and no matter what we still need to remember to
     * execute it if the log is rotated.
     */
    EXPAND(this.signals) = jot;
}


void
mkinterval(interval)
{
    if (this.interval) yyerror("duplicate interval");
    this.interval = interval;
}

%}

%token COMMA PATH SIZE SAVE IN SIGNAL TEXT
%token NUMBER TOKEN EVERY DAY WEEK MONTH YEAR
%token SIZE_SPECIFICATION TRUNCATE TOUCH CLASS
%token SET DOTS NUMBERED SUFFIX PREFIX COMPRESS PLAIN

%%

config:		config stanza
	|	/*nothing*/

stanza:		prefix commands
		{ mkadd(); }
	|	SET option
		{   switch ($2) {
		    case DOTS:     dotted_backup = true;  break;
		    case NUMBERED: dotted_backup = false; break;
		    case SUFFIX:   backup_suffix = true;  break;
		    case PREFIX:   backup_suffix = false; break;
		    case COMPRESS: compress_them = true;  break;
		    case PLAIN:    compress_them = false; break;
		    }
		}
	;

option:		DOTS
	|	NUMBERED
	|	SUFFIX
	|	PREFIX
	|	COMPRESS
	|	PLAIN
	;

prefix:		PATH
		{ mksave(yytext); }

commands:	commands statement
	|	/*nothing*/
	;

statement:	CLASS TEXT
		{ mkclass(yytext); }
	|	TOUCH NUMBER
		{ mktouch(yytext); }
	|	SIGNAL TEXT
		{ mksignal(yytext); }
	|	SIZE SIZE_SPECIFICATION
		{ mksize(sizeandunits(yytext)); }
	|	EVERY every
	|	save_in
	|	option
		{   switch ($1) {
		    case DOTS:     this.dotted_backup = true;  break;
		    case NUMBERED: this.dotted_backup = false; break;
		    case SUFFIX:   this.backup_suffix = true;  break;
		    case PREFIX:   this.backup_suffix = false; break;
		    case COMPRESS: this.compress_them = true;  break;
		    case PLAIN:    this.compress_them = false; break;
		    }
		}
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
		{ this.truncate = 1; }
	;
