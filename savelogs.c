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
 * by David L Parsons (orc@pell.portland.or.us).  My name may not be used
 * to endorse or promote products derived from this software without
 * specific prior written permission.  THIS SOFTWARE IS PROVIDED
 * ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 * WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.
 */
#include "config.h"

#include <stdio.h>
#include <fcntl.h>
#if HAVE_GETOPT_H
# include <getopt.h>
#endif
#include <string.h>
#if HAVE_MALLOC_H
# include <malloc.h>
#endif
#if HAVE_LIBGEN_H
# include <libgen.h>
#endif
#include <stdlib.h>
#include <stdarg.h>
#include <sys/syslog.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#include "savelogs.h"

extern signal_t *signals;
extern log_t *files;
extern FILE *yyin;
extern char version[];

char *cfgfile = DEFAULT_CONFIG;
char *pgm;

int doitnow = 1;
Flag backup_suffix = false;	/* have backup sequence# as a suffix, not prefix */
Flag dotted_backup = false;	/* use o's for backup sequence, not #s */
Flag compress_them = false;	/* run compress on the backed-up files */
int debug = 0;		/* debugging level, set with -d */
int forced = 0;		/* rotate everything now, no matter what */
time_t now;		/* what time of day is it, for the SICK AWFUL
			 * KLUDGE we have to use to expire things by
			 * date
			 */

#ifndef HAVE_BASENAME
/*
 * basename() for systems that don't have it
 */
char *
basename(char *path)
{
    char *p = strrchr(path, '/');

    return p ? (1+p) : path;
}
#endif


#ifndef HAVE_RENAME
/*
 * rename() for systems that don't have it
 */
int
rename(char *old, char *new)
{
    int rc;

    unlink(new);

    if ( (rc = link(old,new)) == 0 )
	unlink(old);

    return rc;
}
#endif


/*
 * error() tells us about some random error.
 */
void
error(char *fmt, ...)
{
    va_list ptr;

    va_start(ptr, fmt);
    if ( (debug > 2) || isatty(fileno(stdout)) ) {
	fprintf(stderr, "%s: ", pgm);
	vfprintf(stderr, fmt, ptr);
	fputc('\n', stderr);
    }
    else
	vsyslog(LOG_ERR, fmt, ptr);
    va_end(ptr);
} /* error */


/*
 * Trace() does debug logging
 */
void
Trace(int level, char *fmt, ...)
{
    va_list ptr;

    va_start(ptr,fmt);
    if (debug >= level) {
	vfprintf(stderr, fmt, ptr);
	fputc('\n', stderr);
    }
    va_end(ptr);
}


/*
 * show what a flag is set to, if it's set
 */
static char*
sayflag(char *prefix, Flag f, char *yes, char *no)
{
    if ( f )
	printf("%s%s\n", prefix, (f == true)?yes:no);
}


/*
 * finish() closes syslog, then exits with the given error code
 */
finish(int status)
{
    if ( !isatty(fileno(stdout)) ) closelog();
    exit(status);
} /* finish */


/*
 * recursively print all of the log_t entries in a linked list.
 */
static void
printfiles(log_t *p)
{
    int i;

    if ( p == 0 ) return;

    printfiles(p->next);

    printf("%s\n", p->path);

    if ( p->class )
	printf("\tCLASS \"%s\"\n", p->class);

    if ( p->size )
	printf("\tSIZE %d\n", p->size);

    if ( p->dotted_backup != dotted_backup )
	sayflag("\t", p->dotted_backup, "DOTS", "NUMBERS");
    if ( p->backup_suffix != backup_suffix )
	sayflag("\t", p->backup_suffix, "SUFFIX", "PREFIX");
    if ( p->compress_them != compress_them )
	sayflag("\t", p->compress_them, "COMPRESSED" , "UNCOMPRESSED");
    
    if (p->interval ) {
	printf("\tEVERY ");
	switch (p->interval) {
	case DAY:   printf("DAY"); break;
	case WEEK:  printf("WEEK"); break;
	case MONTH: printf("MONTH"); break;
	case YEAR:  printf("YEAR"); break;
	default:    printf("CONFUSION!"); break;
	}
	putchar('\n');
    }

    if ( p->truncate )
	printf("\tTRUNCATE\n");

    if ( p->backup )
	printf("\tSAVE %d IN %s\n",
	    p->backup->count, p->backup->dir); 

    if ( p->touch )
	printf("\tTOUCH %o\n", p->touch);

    for (i=0; i < S(p->signals); i++)
	    printf("\tSIGNAL \"%s\"\n", T(p->signals)[i]->command);
}


/*
 * process a single signal_t
 */
void
onejob(log_t *p, char *class, time_t now)
{
    struct stat st;
    int mode = 0644;
    int i;

    if ( class && (strcasecmp(class, p->class) != 0) )
	return;

    if ( stat(p->path, &st) == -1 ) {
	Trace(2, "Cannot stat %s", p->path);
	return;
    }
    if ( !S_ISREG(st.st_mode) ) {
	Trace(2, "%s is not a regular file", p->path);
	return;
    }

    if ( p->interval && !forced ) {
	if (st.st_ctime == now)
	    return;
	else {
	    struct tm tnow, tfile, *tp;

	    if ( tp = localtime(&now) )
		memcpy(&tnow, tp, sizeof *tp);
	    if ( tp = localtime(&st.st_ctime) )
		memcpy(&tfile, tp, sizeof *tp);

	    switch (p->interval) {
	    case YEAR:  if ( tfile.tm_year == tnow.tm_year )
			    return;
			else
	    case MONTH: if ( tfile.tm_mon == tnow.tm_mon )
			    return;
			else
	    case WEEK:  if ( (tfile.tm_yday % 7) != (tnow.tm_yday % 7) )
			    return;
			else
	    case DAY:   if ( tfile.tm_yday != tnow.tm_yday )
			    return;
	    }
	}
	Trace(2, "%s is old enough to die", p->path);
    }

    if ( p->size && !forced ) {
	if (st.st_size < p->size)
	    return;

	Trace(2, "%s is big enough to die ( %ld > %ld )",
		    p->path, (long)st.st_size, (long)p->size);
    }

    Trace(1, "Processing %s", p->path);

    for (i = 0; i < S(p->signals); i++)
	T(p->signals)[i]->active++;

    if ( p->backup && p->backup->count )
	Archive(p);

    if ( p->touch ) {
	Trace(1, "touch [%o] %s", p->touch, p->path);
	mode = p->touch;
    }

    if ( doitnow && (p->truncate || p->touch) ) {
	if ( (i=open(p->path, O_CREAT|O_TRUNC, mode)) != -1 )
	    close(i);
	else
	    error("creat %s: %s", p->path, strerror(errno));
    }
}


/*
 * process() looks at all of the offending files
 */
int
process(char *class)
{
    log_t *p;
    signal_t *sig;
    char *workfile;
    int doit;

    if ( debug ) {
	sayflag("SET ", dotted_backup, "DOTS", "NUMBERS");
	sayflag("SET ", backup_suffix, "SUFFIX" , "PREFIX");
	sayflag("SET ", compress_them, "COMPRESSED", "UNCOMPRESSED");
	printfiles(files);
    }

    time(&now);

    for (p = files; p; p = p->next)
	onejob(p, class, now);

    for (sig = signals; sig; sig = sig->next)
	if (sig->active) {
	    Trace(3, "firing ``%s'' (%d request%s)",
			sig->command, sig->active, (sig->active==1)?"":"s");
	    if ( doitnow )
		system(sig->command);
	    sig->active = 0;
	}
} /* process */


/*
 * approxlog10() does a q&d approximate log10 of an integer, using
 * the Bunnies And Burrows counting system ( 1, 2, 3, 4, many, many, many )
 */
static int
approxlog10(int x)
{
    if ( x < 10 ) return 0;
    if ( x < 100 ) return 1;
    if ( x < 1000 ) return 2;
    if ( x < 10000 ) return 3;
    return 4;
}


/*
 * bkupname() generates the name of a backup file
 */
void
bkupname(char *dest, log_t *p, int age, char *file, int rename_backup)
{
    char *dots = "ooooooooooooooo";
    char *dir = p->backup->dir;
    int width = 1 + approxlog10(p->backup->count);

    if ( p->backup_suffix == true ) {
	if ( p->dotted_backup == true )
	    sprintf(dest, "%s/%s.%*.*s", dir, file, age+1, age+1, dots);
	else
	    sprintf(dest, "%s/%s.%0*d", dir, file, width, age);
    }
    else {
	if ( p->dotted_backup == true )
	    sprintf(dest, "%s/%*.*s.%s", dir, age+1, age+1, dots, file);
	else
	    sprintf(dest, "%s/%0*d.%s", dir, width, age, file);
    }
#ifdef ZEXT
    if ( (p->compress_them == true) && rename_backup )
	strcat(dest, ZEXT);
#endif
}


/*
 * pushback() pushes back all of the archived copies of the file
 */
void
pushback(log_t *f, int level)
{
    char *to, *from;
    char *bn = basename(f->path);
    int i, siz;
    struct stat st;

    if ( level >= f->backup->count ) return;

    siz = strlen(f->backup->dir) + strlen(bn) + 20;

    to = alloca(siz);
    from = alloca(siz);

    siz = strlen(f->backup->dir) + 1;

    bkupname(to, f, level+1, bn, 1);
    bkupname(from, f, level, bn, 1);

    if ( stat(to, &st) == 0 )
	pushback(f, level+1);

    if ( stat(from, &st) == 0 ) {
	Trace(2, "%s -> %s", from, to);
	if ( doitnow ) rename(from, to);
    }
} /* pushback */


/*
 * Archive() copies a logfile into a backup directory, keeping old backup
 * copies around for however many generations the user might want.
 */
Archive(log_t *f)
{
    char ftext[1024];
    char *bn = basename(f->path);
    char *arcf;
    int ffd, tfd;
    struct stat st;
    int rc, mode, size;

    pushback(f, 0);

    arcf = alloca(strlen(f->backup->dir) + strlen(bn) + 20);

    bkupname(arcf, f, 0, bn, 0);

    /* try to rename the file into the archive
     */

    Trace(2, "%s -> %s", f->path, arcf);

    if ( doitnow )  {
	if ( rename(f->path, arcf) == -1 ) {
	    if ( errno != EXDEV ) {
		error("could not rename %s to %s: %s",
		    f->path, arcf, strerror(errno));
		return;
	    }

	    if ( (ffd = open(f->path, O_RDONLY)) == -1 ) {
		error("can't open %s: %s", f->path, strerror(errno));
		return;
	    }

	    if ( fstat(ffd, &st) == -1 ) mode = 0400;
	    else mode = st.st_mode;

	    unlink(f->path);

	    if ( (tfd = open(arcf, O_WRONLY|O_CREAT|O_TRUNC, mode)) == -1 ) {
		error("can't create %s: %s", arcf, strerror(errno));
		close(ffd);
		return;
	    }

	    while ( (size=read(ffd, ftext, sizeof ftext)) > 0 ) 
		write(tfd, ftext, size);

	    close(ffd);
	    close(tfd);
	}
    }

#ifdef ZEXT
    if ( f->compress_them == true ) {
	char *squash = alloca(sizeof PATH_COMPRESS + strlen(arcf) + 4);

	sprintf(squash, "%s %s", PATH_COMPRESS, arcf);

	Trace(2, "compress %s", arcf);
	if ( doitnow )
	    system(squash);
    }
#endif
} /* Archive */


/*
 * savelogs, in the flesh
 */
main(int argc, char **argv)
{
    int rc;

    argv[0] = pgm = basename(argv[0]);

    time(&now);

    now /= SECSPERDAY;

    if ( isatty(fileno(stdout)) ) opterr++;
    else openlog("savelogs", LOG_DAEMON, 0);

    while ((rc=getopt(argc, argv, "C:dfnvV")) != EOF) {
	switch (rc) {
	case 'n':
		doitnow = 0;
		break;
	case 'v':
	case 'd':
		debug++;
		break;
	case 'f':
		forced++;
		break;
	case 'C':
		cfgfile = strdup(optarg);
		break;
	case 'V':
		puts(version);
		finish(0);
	default:if ( isatty(fileno(stdout)) ) finish(1);
	}
    }

    argc -= optind;
    argv += optind;

    if ( strcmp(cfgfile, "-") == 0 )
	yyin = stdin;
    else if ( (yyin = fopen(cfgfile, "r")) == 0 ) {
	error("can't open %s: %s", cfgfile, strerror(errno));
	finish(1);
    }

    if ( yyparse() != 0 ) finish(1);

    Trace(1, "process%s%s", argc ? " class " : " *", argc ? argv[0] : "");

    process(argc ? argv[0] : 0);
    finish(0);
} /* main */
