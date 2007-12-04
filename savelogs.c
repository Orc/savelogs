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

#include "savelogs.h"

extern job_t *jobs;
extern log_t *files;
extern FILE *yyin;
extern char version[];

char *cfgfile = LOG_CFG;

int dontdoit = 0;
int backup_suffix = 0;	/* have backup sequence# as a suffix, not prefix */
int dotted_backup = 0;	/* use o's for backup sequence, not #s */
int compress_them = 0;	/* run compress on the backed-up files */
int debug = 0;		/* debugging level, set with -d */
int verbose = 0;	/* show progress, set with -v */
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

    if (debug > 2) {
	printf("savelogs: ");
	vprintf(fmt, ptr);
	putchar('\n');
    }
    else
	vsyslog(LOG_ERR, fmt, ptr);
} /* error */


/*
 * finish() closes syslog, then exits with the given error code
 */
finish(int status)
{
    closelog();
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
 * examine() looks at all of the offending files
 */
int
examine(char *class)
{
    int x, rc, i;
    int any_class_is_valid = 0;
    log_t *p;
    job_t *sig;
    struct stat status;
    struct stat direct;
    char *workfile;
    int doit;

    if (debug > 2) {
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

	rc = stat(p->path, &status);

	if (rc < 0) {
	    if (debug > 2)
		error("Cannot stat %s", p->path);
	    continue;
	}
	if ((status.st_mode & S_IFREG) == 0) {
	    error("%s is not a regular file", p->path);
	    continue;
	}

	/* Check if we're actually planning on processing this
	 * entry today
	 */

	doit = (class == 0) || (strcasecmp(class, p->class) == 0);

	if ( !doit ) {
	    if ( debug > 2 ) {
		printf("Skipping %s", p->path);
		if (p->class)
		    printf(" (class %s)", p->class);
		putchar('\n');
	    }
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
	    if ( debug > 2 )
		if ( doit )
		    printf("%s is old enough to die\n", p->path);
		else
		    printf("%s will live another day\n", p->path);
	}
	if (p->size > 0) {
	    if (status.st_size <= p->size) {
		if (debug > 2)
		    printf("%s is too small to be processed\n",
			    p->path);
		doit = 0;
	    }
	    else {
		if (debug > 2)
		    printf("%s is big enough to die (%ld/%ld)\n",
			    p->path, status.st_size, p->size);
	    }
	}

	if ( !(doit||forced) )
	    continue;

	if (debug || verbose)
	    printf("Processing %s\n", p->path);

	for (i = 0; i < NR(p->jobs); i++)
	    IT(p->jobs,i)->active++;

	if ( p->backup ) {
	    if ( p->backup->count == 0 ) {
		if ( !dontdoit ) truncate(p->path, 0L);
	    }
	    else
		Archive(p);
	}
	if ( p->touch ) {
	    unlink(p->path);
	    if ( (i = creat(p->path, p->touch)) != -1 )
		close(i);
	    else
		error("touch %s: %s", p->path, strerror(errno));
	}
    }
    /*
     * After shuffling files around, go back through and do all the signalling
     * we need to do
     */
    for (sig = jobs; sig; sig = sig->next)
	if (sig->active) {
	    if (debug > 2) {
		printf("(%d)", sig->active);
		fflush(stdout);
	    }
	    if ( dontdoit || (debug > 3) )
		printf("firing [%s]\n", sig->command);
	    if ( !dontdoit )
		system(sig->command);
	    sig->active = 0;
	}
} /* examine */


/*
 * backup_file() generates the name of a backup file
 */
void
backup_file(char *dest, char *dir, int backup, char *file, int compressed)
{
    char *dots = "ooooooooooooooo";

    if ( backup_suffix ) {
	if ( dotted_backup )
	    sprintf(dest, "%s/%s.%.*s", dir, file, backup+1, dots);
	else
	    sprintf(dest, "%s/%s.%d", dir, file, backup);
    }
    else {
	if ( dotted_backup )
	    sprintf(dest, "%s/%.*s.%s", dir, backup+1, dots, file);
	else
	    sprintf(dest, "%s/%d.%s", dir, backup, file);
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
pushback(log_t *f)
{
    char *to, *from;
    char *bn = basename(f->path);
    int i, siz;

    siz = strlen(f->backup->dir) + strlen(bn) + 20;

    to = alloca(siz);
    from = alloca(siz);

    siz = strlen(f->backup->dir) + 1;

    for (i=f->backup->count-1; i>0 ; --i) {

	backup_file(to, f->backup->dir, i, bn, compress_them);
	backup_file(from, f->backup->dir, i-1, bn, compress_them);

	if (debug > 2)
	    printf("%s -> %s\n", from, to);

	if ( !dontdoit ) rename(from, to);
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

    pushback(f);

    arcf = alloca(strlen(f->backup->dir) + strlen(bn) + 20);

    backup_file(arcf, f->backup->dir, 0, bn, 0);

    /* try to rename the file into the archive
     */

    if ( debug > 2 ) printf("%s -> %s\n", f->path, arcf);

    if ( !dontdoit )  {
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

	if ( debug > 2 )
	    printf("compress %s\n", arcf);
	if ( !dontdoit )
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

    openlog("savelogs", LOG_DAEMON, 0);
    time(&now);

    now /= SECSPERDAY;

    while ((rc=getopt(argc, argv, "C:dfnvV")) != EOF) {
	switch (rc) {
	case 'n':
		dontdoit++;
		break;
	case 'v':
		verbose++;
		break;
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
	default:if ( isatty(fileno(stdout)) ) {
		    fprintf(stderr, "unknown option <%c>\n", optopt);
		    exit(1);
		}
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

    if ( debug > 1 ) printf("examine%s%s\n", argc ? " class " : " *",
					     argc ? argv[0] : "");

    examine(argc ? argv[0] : 0);
    finish(0);
} /* main */
