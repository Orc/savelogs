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

extern job_t *jobs;
extern log_t *files;
extern FILE *yyin;
extern char version[];

char *cfgfile = DEFAULT_CONFIG;
char *pgm;

int doitnow = 1;
int backup_suffix = 0;	/* have backup sequence# as a suffix, not prefix */
int dotted_backup = 0;	/* use o's for backup sequence, not #s */
int compress_them = 0;	/* run compress on the backed-up files */
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
 * finish() closes syslog, then exits with the given error code
 */
finish(int status)
{
    if ( !isatty(fileno(stdout)) ) closelog();
    exit(status);
} /* finish */


static void
dumplog_t(log_t *p)
{
    int i;

    if ( p == 0 ) return;

    dumplog_t(p->next);

    printf("%s\n", p->path);
    if (p->class)
	printf("\tCLASS \"%s\"\n", p->class);
    if (p->size > 0)
	printf("\tSIZE %d\n", p->size);
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
    if (p->backup) {
	if (p->backup->count == 0)
	    printf("\tTRUNCATE\n");
	else {
	    printf("\tSAVE %d IN %s",
		p->backup->count, p->backup->dir); 
	    putchar('\n');
	}
    }
    if (p->touch)
	printf("\tTOUCH %o\n", p->touch);

    for (i=0; i < NR(p->jobs); i++)
	    printf("\tSIGNAL \"%s\"\n", IT(p->jobs,i)->command);
}


/*
 * process() looks at all of the offending files
 */
int
process(char *class)
{
    int x, rc, i;
    int any_class_is_valid = 0;
    log_t *p;
    job_t *sig;
    struct stat status;
    struct stat direct;
    char *workfile;
    int doit;

    if ( debug ) {
	if ( dotted_backup )
	    printf("SET DOTS\n");
	if ( backup_suffix )
	    printf("SET SUFFIX\n");
	if ( compress_them )
	    printf("SET COMPRESSED\n");
	dumplog_t(files);
    }

    /* get the current time so we can do interval checks
     */
    time(&now);

    /*
     * walk the victim list, picking out likely candidates
     */
    for (p = files; p; p = p->next) {

	doit = (class == 0) || (p->class && (strcasecmp(class, p->class) == 0));

	if ( !doit ) continue;


	rc = stat(p->path, &status);

	if (rc < 0) {
	    Trace(2, "Cannot stat %s", p->path);
	    continue;
	}
	if ((status.st_mode & S_IFREG) == 0) {
	    Trace(2, "%s is not a regular file", p->path);
	    continue;
	}

	if ( p->interval ) {
	    if (status.st_ctime == now)
		doit = 0;
	    else {
		struct tm tnow, tfile, *tp;

		if ( tp = localtime(&now) )
		    memcpy(&tnow, tp, sizeof *tp);
		if ( tp = localtime(&status.st_ctime) )
		    memcpy(&tfile, tp, sizeof *tp);

		switch (p->interval) {
		case YEAR:  if ( tfile.tm_year == tnow.tm_year )
				doit = 0;
			    else
		case MONTH: if ( tfile.tm_mon == tnow.tm_mon )
				doit = 0;
			    else
		case WEEK:  if ( (tfile.tm_yday % 7) != (tnow.tm_yday % 7) )
				doit = 0;
			    else
		case DAY:   if ( tfile.tm_yday != tnow.tm_yday )
				doit = 0;
		}
	    }
	    if ( doit ) Trace(2, "%s is old enough to die", p->path);
	}
	if (p->size > 0) {
	    if (status.st_size <= p->size)
		doit = 0;
	    else
		Trace(2, "%s is big enough to die (is %ld / min %ld)\n",
			p->path, status.st_size, p->size);
	}

	if ( !(doit||forced) )
	    continue;

	Trace(0, "Processing %s", p->path);

	for (i = 0; i < NR(p->jobs); i++)
	    IT(p->jobs,i)->active++;

	if ( p->backup ) {
	    if ( p->backup->count == 0 ) {
		if ( doitnow ) truncate(p->path, 0L);
	    }
	    else
		Archive(p);
	}
	if ( p->touch ) {
	    Trace(1, "touch [%o] %s", p->touch, p->path);

	    if ( doitnow ) {
		unlink(p->path);
		if ( (i = creat(p->path, p->touch)) != -1 )
		    close(i);
		else
		    error("touch %s: %s", p->path, strerror(errno));
	    }
	}
    }
    /*
     * After shuffling files around, go back through and do all the signalling
     * we need to do
     */
    for (sig = jobs; sig; sig = sig->next)
	if (sig->active) {
	    Trace(3, "firing ``%s'' (%d request%s)",
			sig->command, sig->active, (sig->active==1)?"":"s");
	    if ( doitnow )
		system(sig->command);
	    sig->active = 0;
	}
} /* process */


/*
 * bkupname() generates the name of a backup file
 */
void
bkupname(char *dest, char *dir, int age, char *file, int compressed)
{
    char *dots = "ooooooooooooooo";

    if ( backup_suffix ) {
	if ( dotted_backup )
	    sprintf(dest, "%s/%s.%*.*s", dir, file, age+1, age+1, dots);
	else
	    sprintf(dest, "%s/%s.%d", dir, file, age);
    }
    else {
	if ( dotted_backup )
	    sprintf(dest, "%s/%*.*s.%s", dir, age+1, age+1, dots, file);
	else
	    sprintf(dest, "%s/%d.%s", dir, age, file);
    }
#ifdef ZEXT
    if ( compressed )
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

    bkupname(to, f->backup->dir, level+1, bn, compress_them);
    bkupname(from, f->backup->dir, level, bn, compress_them);

    if ( stat(to, &st) == 0 )
	pushback(f, level+1);

    Trace(2, "%s -> %s", from, to);
    if ( doitnow ) rename(from, to);
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

    bkupname(arcf, f->backup->dir, 0, bn, 0);

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
    if ( compress_them ) {
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

    Trace(1, "process%s%s\n", argc ? " class " : " *", argc ? argv[0] : "");

    process(argc ? argv[0] : 0);
    finish(0);
} /* main */
