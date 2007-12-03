/*
 * savelogs() does simple logfile maintainance
 *
 * usage: savelogs [-C config-file]
 *
 * Copyright (c) 1996 David L Parsons 
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by David L Parsons (orc@pell.chi.il.us).  My name may not be used
 * to endorse or promote products derived from this software without
 * specific prior written permission.  THIS SOFTWARE IS PROVIDED
 * ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 * WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.
 */
#include <stdio.h>
#include <fcntl.h>
#if HAVE_GETOPT_H
#   include <getopt.h>
#endif
#include <string.h>
#if HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <stdlib.h>
#include <stdarg.h>
#include <sys/syslog.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#include "y.tab.h"

#define LOG_CFG "/etc/savelogs.conf"

#define SECSPERDAY	24*60*60	/* how many seconds in a day */

/*
 * Commands to execute after a file has been shuffled around.
 */
typedef struct __action {
    char *command;		/* the commands; input to system() */
    int triggered;		/* has this command already fired? */
    int active;			/* does this command need to be fired? */
    int links;			/* how many victims use this command */
    struct __action *next;	/* next in chain */
} Action;

typedef struct __victim {
    char *pathname;		/* the file we're administering */
    int size;			/* roll out if larger than this size */
    int interval;		/* how often to roll out the barrel?*/
    char *savedir;		/* directory to put archived copies into */
    int save_copies;		/* how many copies to archive */
    int dotruncate;		/* truncate the file instead of archiving */
    int dotouch;		/* manually create the file with given mode */
    char *class;		/* log maintenance class */
    Action **sig;		/* commands to do to alert daemons */
} Victim;

Action *update_cmds = (Action *)0;
Victim *files = (Victim *)0;
int nrfiles = 0;

char *cfgfile = LOG_CFG;
char **class;		/* logging classes, from arg list */
int nrclass;		/* # of logging classes */

int debug = 0;		/* debugging level, set with -d */
int verbose = 0;	/* show progress, set with -v */
int forced = 0;		/* rotate everything now, no matter what */
time_t now;		/* what time of day is it, for the SICK AWFUL
			 * KLUDGE we have to use to expire things by
			 * date
			 */

/*
 * Imports from lex.yy.c
 */
extern int line_number;
extern char yytext[];
extern int yyleng;
extern FILE *yyin;


/*
 * savelogs, in the flesh
 */
main(int argc, char **argv)
{
    int rc;

    update_cmds = (Action*)0;
    files = (Victim*)malloc(1);

    openlog("savelogs", LOG_DAEMON, 0);
    time(&now);

    now /= SECSPERDAY;

    while ((rc=getopt(argc, argv, "vdfVC:")) != EOF) {
	switch (rc) {
	case 'v':	verbose++;				break;
	case 'd':   	debug++;				break;
	case 'f':       forced++;				break;
	case 'C':	cfgfile = strdup(optarg);		break;
	case 'V':	puts("PUT A VERSION HERE");		finish(0);
	default:	syslog(LOG_WARNING, "Bad option <%c>", rc);break;
	}
    }

    argc -= optind;
    argv += optind;

    nrclass = argc;
    class   = argv;

    parsecfg();
    examine();
    finish(0);
} /* main */


/*
 * finish() closes syslog, then exits with the given error code
 */
finish(int status)
{
    closelog();
    exit(status);
} /* finish */



/*
 * Token() reads a token from the scanner, optionally
 * dumping debugging information to the screen
 */
int
Token()
{
    int token = yylex();

    if (debug > 3)
	printf("TOKEN: %d <%s>\n", token, yytext);
    return token;
} /* Token */


/*
 * cfgerror() tells us about an error found in the configuration file
 */
void
cfgerror(char *fmt, ...)
{
    va_list ptr;
    char msg[2000];

    va_start(ptr, fmt);

    if (strcmp(cfgfile, "-") != 0)
	sprintf(msg, "%s: ", cfgfile);
    vsprintf(msg+strlen(msg), fmt, ptr);
    sprintf(msg+strlen(msg), " at line %d", line_number);
    va_end(ptr);

    if (debug > 2)
	printf("%s\n", msg);
    else
	syslog(LOG_ERR, msg);
} /* cfgerror */


/*
 * error() tells us about some random error.
 */
void
error(char *fmt, ...)
{
    va_list ptr;

    va_start(ptr, fmt);

    if (debug > 2) {
	printf("savelogs: ");
	vprintf(fmt, ptr);
	putchar('\n');
    }
    else
	vsyslog(LOG_ERR, fmt, ptr);
} /* error */


/*
 * Statement() reads a command from the configuration file and builds
 * the appropriate Victim structure for it.
 */
int
Statement(int token, Victim **vp)
{
    int code;

    *vp = NULL;
    if (token != PATH) {
	cfgerror("expected pathname");
	return -1;
    }

    (*vp) = (Victim*)malloc(20 + sizeof **vp);
    memset((*vp), 0, sizeof (**vp));
    (*vp)->pathname = strdup(yytext);
    for (token = Token(); token != PATH && token != 0; token = Token()) {
	switch (token) {
	case CLASS:	code = GetClass(Token(), vp);	break;
	case SIZE:	code = GetSize(Token(), vp);	break;
	case EVERY:	code = GetEvery(Token(), vp);	break;
	case SIGNAL:	code = GetAction(Token(), vp);	break;
	case SAVE:	code = GetSave(Token(), vp);	break;
	case TRUNCATE:	(*vp)->dotruncate = 1;		break;
	case TOUCH:	code = GetTouch(Token(), vp);	break;
	default:	cfgerror("unexpected input");	return -1;
	}
	if (code < 0)
	    return code;
    }
    return token;
} /* Statement */


/*
 * GetClass() gets an operation class
 */
GetClass(int token, Victim **vp)
{
    if (token != STRING) {
	cfgerror("CLASS expects a class string");
	return -1;
    }
    if ((*vp)->class = strdup(yytext))
	return 0;
    cfgerror("OUT OF MEMORY");
    return -1;
} /* GetClass */


/*
 * AddSig() adds a signal to a victim
 */
void
AddSig(Victim *v, void *action)
{
    if (v->sig == 0) {
	v->sig = malloc( 2 * sizeof v->sig[0]);
	v->sig[0] = action;
	v->sig[1] = 0;
    }
    else {
	int ct;

	for (ct=0; v->sig[ct]; ct++)
	    if (v->sig[ct] == action) {
		/* don't duplicate actions. */
		return;
	    }

	v->sig = realloc(v->sig, (ct+2) * sizeof v->sig[0]);
	v->sig[ct] = action;
	v->sig[1+ct] = 0;
    }
} /* AddSig */


/*
 * GetAction() gets a SIGNAL specification
 */
GetAction(int token, Victim **vp)
{
    int i, x;
    static char bfr[4000];
    Action *tmp;

    if (token != STRING) {
	cfgerror("SIGNAL expects a command string");
	return -1;
    }

    memset(bfr, 0, sizeof bfr);
    for (x=i=0; x < yyleng; x++) {
	if ((yytext[x] == '@') && (yytext[x+1] == 'F')) {
	    strcat(&bfr[i], (*vp)->pathname);
	    i = strlen(bfr);
	    x++;
	}
	else {
	    bfr[i] = yytext[x];
	    i++;
	}
    }
    bfr[i] = 0;

    for (tmp = update_cmds; tmp; tmp = tmp->next)
	if (strcmp(tmp->command, bfr) == 0) {
	    AddSig(*vp, tmp);
	    tmp->links++;
	    return 0;
	}

    if (tmp = malloc (sizeof *tmp))
	tmp->command = strdup(bfr);

    if (tmp == NULL || tmp->command == NULL) {
	cfgerror("OUT OF MEMORY");
	finish(2);
    }

    tmp->links = 1;
    tmp->triggered = 0;
    AddSig(*vp, tmp);
    tmp->next = update_cmds;
    update_cmds = tmp;
    return 0;
} /* GetAction */


/*
 * GetSave() parses a SAVE specification.
 */
GetSave(int token, Victim **vp)
{
    if (token == NUMBER) {
	(*vp)->save_copies = atoi(yytext);
	token = Token();
    }
    else
	(*vp)->save_copies = 1;
    if (token != IN) {
	cfgerror("expected IN");
	return -1;
    }
    Token();		/* The savepath can be most anything  so we'll
			// just blindly use the next token for it.
			*/
    if (((*vp)->savedir = strdup(yytext)) == NULL) {
	cfgerror("OUT OF MEMORY");
	finish(2);
    }
    (*vp)->dotruncate = 0;
    return 0;
} /* GetSave */


/*
 * GetEvery() parses an EVERY specification.  You may look at this and
 * ask why I didn't use yacc to build this parser?  Why, it's because
 * I'm too lazy to see how yacc grammars work.
 */
GetEvery(int token, Victim** vp)
{
    int interval;

    if (token == NUMBER) {
	interval = atoi(yytext);
	token = Token();
    }
    else
	interval = 1;

    if (token != DAYS && token != WEEKS && token != YEARS) {
	cfgerror("expected DAYS, WEEKS, or YEARS");
	return -1;
    }
    if (token == WEEKS)
	interval *= 7;
    else if (token == YEARS)
	interval *= (52*7);
    (*vp)->interval = interval;
    return 0;
} /* GetEvery */


/*
 * GetTouch() parses a TOUCH specification
 */
GetTouch(int token, Victim**vp)
{
    if (token != NUMBER) {
	cfgerror("expected a file mode");
	return -1;
    }
    if (sscanf(yytext, "%o", &((*vp)->dotouch)) < 1) {
	cfgerror("incomprehensible file mode");
	return -1;
    }
    return 0;
} /* GetTouch */


/*
 * GetSize() parses a SIZE specification
 */
GetSize(int token, Victim** vp)
{
    int size;
    int idx;

    if (token != SIZE_SPECIFICATION && token != NUMBER) {
	cfgerror("expected a size");
	return -1;
    }

    size = atoi(yytext);

    switch (yytext[strlen(yytext)-1]) {
    case 'k':
    case 'K':
	(*vp)->size = size * 1024;
	break;
    case 'm':
    case 'M':
	(*vp)->size = size * 1024 * 1024;
	break;
    default:
	(*vp)->size = size;
	break;
    }
    return 0;
} /* GetSize */


/*
 * AddVictim()
 */
AddVictim(Victim *p)
{
    if (p != 0) {
	files = (Victim *)realloc(files, (1+nrfiles) * sizeof *files);
	files[nrfiles] = *p;
	nrfiles++;
    }
} /* AddVictim */


/*
 * Parsecfg() reads the configuration file and builds the list of
 * files to be analyzed.
 */
int
parsecfg()
{
    /*
    //	IDENT {SIZE SIZE_SPECIFICATION}
    //	      {EVERY [NUMBER] [DAYS|WEEKS]}
    //	      {SAVE [NUMBER] IN IDENT|PATH}
    //	      {TRUNCATE}
    //	      {TOUCH MODE}
    //	      {SIGNAL STRING}
     */

    int code;
    int x;
    Victim *victim;

    /* Open the CFG file and load 'em up, or give a hostile error message
     */
    if (strcmp(cfgfile, "-") == 0)
	yyin = stdin;
    else if ((yyin = fopen(cfgfile, "r")) == (FILE*)0) {
	if (debug > 1)
	    perror(cfgfile);
	else
	    syslog(LOG_WARNING, "Cannot open config file [%s]", cfgfile);
	exit(1);
    }
    for (code = Token(); (code = Statement(code, &victim)) > 0; )
	AddVictim(victim);
    if (code < 0)
	exit(1);
    else
	AddVictim(victim);
    fclose(yyin);
} /* parsecfg */


/*
 * buildtemp() makes a tempfilename that lives in the same directory as
 * the file we're about to deal with
 */
char *
buildtemp(char *file)
{
    char *p;
    char *tmp;

    tmp = malloc(strlen(file)+20);
    strcpy(tmp, file);

    if (p = strrchr(tmp, '/'))
	sprintf(++p, "SAVE.%04x", getpid());
    else
	sprintf(tmp, "SAVE.%04x", getpid());
    return tmp;
} /* buildtemp */


/*
 * examine() looks at all of the offending files
 */
int
examine()
{
    int x, rc, i;
    int any_class_is_valid = 0;
    Victim *p;
    Action *sig;
    struct stat status;
    struct stat direct;
    char *workfile;
    int tagged;

    if (debug > 2) {
	for (x=0, p=files; x<nrfiles; x++, p++) {
	    printf("%s\n", p->pathname);
	    if (p->class)
		printf("\tCLASS \"%s\"\n", p->class);
	    if (p->size > 0)
		printf("\tSIZE %d\n", p->size);
	    if (p->interval > 0)
		printf("\tEVERY %d DAYS\n", p->interval);
	    if (p->savedir)
		printf("\tSAVE %d IN %s\n", p->save_copies, p->savedir);
	    else if (p->dotruncate)
		printf("\tTRUNCATE\n");
	    if (p->dotouch)
		printf("\tTOUCH %o\n", p->dotouch);
	    if (p->sig) {
		for (i=0; p->sig[i]; i++)
		    printf("\tSIGNAL \"%s\"\n", p->sig[i]->command);
	    }
	}
    }

    /*
     * check for wildcard service classes
     */
    for (x=0; x<nrclass; x++)
	if (strcasecmp(class[x], "any") == 0)
	    any_class_is_valid++;

    /* get the current time so we can do interval checks
     */
    time(&now);

    /*
     * walk the victim list, picking out likely candidates
     */
    for (x=0, p=files; x<nrfiles; x++, p++) {
	/* Check if we're actually planning on processing this
	 * entry today
	 */
	tagged = 0;
	if (!any_class_is_valid) {
	    if (p->class == 0) {
		if (nrclass != 0)
		    goto skip;
	    }
	    else {
		for (i=0; i<nrclass; i++)
		    if (strcasecmp(p->class, class[i]) == 0)
			break;
		if (i >= nrclass) {
	    skip:   if (debug > 2) {
			printf("Skipping %s", p->pathname);
			if (p->class)
			    printf(" (class %s)", p->class);
			putchar('\n');
		    }
		    continue;
		}
	    }
	}

	rc = stat(p->pathname, &status);

	if (rc < 0) {
	    if (debug > 2)
		error("Cannot stat %s", p->pathname);
	    continue;
	}
	if ((status.st_mode & S_IFREG) == 0) {
	    error("%s is not a regular file", p->pathname);
	    continue;
	}

	if (!forced) {
	    if (p->interval > 0 && (now % p->interval == 0)) {
		if (debug > 2)
		    printf("It's a dead letter day for %s (%ld/%d)\n",
			    p->pathname, now, p->interval);
		tagged++;
	    }
	    else {
		if (debug > 2)
		    printf("It's a good day to be %s\n", p->pathname);
	    }

	    if (p->size > 0) {
		if (status.st_size <= p->size) {
		    if (debug > 2)
			printf("%s is too small to be processed\n",
				p->pathname);
		}
		else {
		    if (debug > 2)
			printf("%s is big enough to die (%ld/%ld)\n",
				p->pathname, status.st_size, p->size);
		    tagged++;
		}
	    }

	    if ((p->size || p->interval) && !tagged)
		continue;
	}

	if (verbose)
	    printf("Processing %s\n", p->pathname);

	if (p->dotruncate) {
	    truncate(p->pathname, 0L);
	    if (p->sig) {
		for (i=0; p->sig[i]; i++)
		    p->sig[i]->active++;
	    }

	}
	else {
	    workfile = buildtemp(p->pathname);

	    rename(p->pathname, workfile);

	    if (p->dotouch)
		close(creat(p->pathname, p->dotouch));

	    if (p->sig) {
		for (i=0; p->sig[i]; i++)
		    p->sig[i]->active++;
	    }

	    /* save the log in a save directory */
	    if (p->savedir)
		if ((rc = stat(p->savedir, &direct)) >= 0) 
		    Archive(workfile, p, &status);
		else
		    error("Cannot stat savedir <%s>", p->savedir);
	}
    }
    /*
     * After shuffling files around, go back through and do all the signalling
     * we need to do
     */
    for (sig = update_cmds; sig; sig = sig->next)
	if (sig->active) {
	    if (debug > 2)
		printf("firing [%s]\n", sig->command);
	    system(sig->command);
	    sig->active = 0;
	}
} /* examine */


/*
 * pushback() adds leading 'o's to an archived file
 */
pushback(char *savedir, char *filename, int level)
{
    char *to, *from;
    int x;

    x = strlen(savedir) + strlen(filename) + level + 4;
    to   = malloc(x + 1);
    from = malloc(x);
    sprintf(to, "%s/o", savedir);
    sprintf(from, "%s/", savedir);

    for (x=level-1; x>0; x--) {
	strcat(to, "o");
	strcat(from, "o");
    }
    sprintf(to+strlen(to), ".%s", filename);
    sprintf(from+strlen(from), ".%s", filename);
    if (debug > 2)
	printf("pushback %s to %s\n", from, to);
    rename(from, to);
    free(to);
    free(from);
} /* pushback */


/*
 * Archive() copies a logfile into a backup directory, keeping old backup
 * copies around for however many generations the user might want.
 */
Archive(char *workfile, Victim *p, struct stat status)
{
    int rc, x;

    char *tp;
    char *newname;

    /* find the filename part
     */
    if (tp = strrchr(p->pathname, '/'))
	++tp;
    else
	tp = p->pathname;

    /* push previously saved versions of this logfile back a bit
     */
    for (x=p->save_copies; x>1; --x)
	pushback(p->savedir, tp, x);

    /* build the name the archived logfile will have
     */
    newname = malloc(strlen(p->savedir) + strlen(tp) + 3);

    sprintf(newname, "%s/o.%s", p->savedir, tp);

    /* first push all the archive copies into oblivion... */
    /* first try to do a rename.... */

    rc = rename(workfile, newname);

    /* If that fails due to it being a crossdevice move, try to copy
     * it over
     */
    if (rc == EXDEV) {
	static char bfr[10240];
	int from, to, size;

	size = -1;

	if ((to = creat(newname, status.st_mode)) < 0) {
	    error("can't create %s: %s", newname, strerror(errno));
	    goto bail;
	}
	if ((from = open(workfile, O_RDONLY)) < 0) {
	    error("can't open %s: %s", workfile, strerror(errno));
	    close(to);
	    goto bail;
	}

	while ((size = read(from, bfr, sizeof bfr)) >= 0) {
	    if (write(to, bfr, size) != size) {
		error("write error on %s: %s", newname, strerror(errno));
		break;
	    }
	}
	if (size < 0)
	    error("read error on %s: %s", workfile, strerror(errno));

bail:	close(to);
	close(from);
	if (size == 0)
	    unlink(workfile);
	free(workfile);
    }
    else if (rc != 0)
	error("could not rename %s to %s: %s",
		workfile, newname, strerror(errno));
} /* Archive */
