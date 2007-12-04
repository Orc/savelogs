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

#include "savelogs.h"

extern job_t *jobs;
extern log_t *files;
extern FILE *yyin;

char *cfgfile = LOG_CFG;

int debug = 0;		/* debugging level, set with -d */
int verbose = 0;	/* show progress, set with -v */
int forced = 0;		/* rotate everything now, no matter what */
time_t now;		/* what time of day is it, for the SICK AWFUL
			 * KLUDGE we have to use to expire things by
			 * date
			 */

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
 * savelogs, in the flesh
 */
main(int argc, char **argv)
{
    int rc;

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

    if ( (yyin = fopen(cfgfile, "r")) == 0 ) {
	error("can't open %s: %s", cfgfile, strerror(errno));
	finish(1);
    }

    if ( yyparse() != 0 ) finish(1);

    examine(argc ? argv[0] : 0);
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
	for (p=files; p; p = p->next) {
	    printf("%s\n", p->path);
	    if (p->class)
		printf("\tCLASS \"%s\"\n", p->class);
	    if (p->size > 0)
		printf("\tSIZE %d\n", p->size);
	    if (p->interval > 0)
		printf("\tEVERY %d DAYS\n", p->interval);
	    if (p->backup) {
		if (p->backup->count == 0)
		    printf("\tTRUNCATE\n");
		else {
		    printf("\tSAVE %d IN %s\n",
			p->backup->count, p->backup->dir);
		}
	    }
	    if (p->touch)
		printf("\tTOUCH %o\n", p->touch);

	    for (i=0; i < NR(p->jobs); i++)
		    printf("\tSIGNAL \"%s\"\n", IT(p->jobs,i)->command);
	}
    }

    /* get the current time so we can do interval checks
     */
    time(&now);

    /*
     * walk the victim list, picking out likely candidates
     */
    for (p = files; p; p = p->next) {
	/* Check if we're actually planning on processing this
	 * entry today
	 */

	doit = (class == 0) || (strcasecmp(class, p->class) == 0);

	if ( !doit ) {
    skip:   if ( debug > 2 ) {
		printf("Skipping %s", p->path);
		if (p->class)
		    printf(" (class %s)", p->class);
		putchar('\n');
	    }
	    continue;
	}

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

	if (!forced) {
	    if (p->interval > 0 && (now % p->interval == 0)) {
		if (debug > 2)
		    printf("It's a dead letter day for %s (%ld/%d)\n",
			    p->path, now, p->interval);
	    }
	    else {
		doit = 0;
		if (debug > 2)
		    printf("It's a good day to be %s\n", p->path);
	    }

	    if (p->size > 0) {
		doit = 0;
		if (status.st_size <= p->size) {
		    if (debug > 2)
			printf("%s is too small to be processed\n",
				p->path);
		}
		else {
		    if (debug > 2)
			printf("%s is big enough to die (%ld/%ld)\n",
				p->path, status.st_size, p->size);
		}
	    }

	    if ((p->size || p->interval) && !doit)
		continue;
	}

	if (verbose)
	    printf("Processing %s\n", p->path);

	for (i = 0; i < NR(p->jobs); i++)
	    IT(p->jobs,i)->active++;

	if ( p->backup ) {
	    if ( p->backup->count == 0 )
		truncate(p->path, 0L);
	    else
		Archive(p);
	}
    }
    /*
     * After shuffling files around, go back through and do all the signalling
     * we need to do
     */
    for (sig = jobs; sig; sig = sig->next)
	if (sig->active) {
	    if (debug > 2)
		printf("firing [%s]\n", sig->command);
	    system(sig->command);
	    sig->active = 0;
	}
} /* examine */


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
	sprintf(to,   "%s/%d.%s", f->backup->dir, i, bn);
	sprintf(from, "%s/%d.%s", f->backup->dir, i-1, bn);

	if (debug > 2)
	    printf("pushback %s to %s\n", from, to);

#if HAVE_RENAME
	rename(from, to);
#else
	unlink(to);
	link(from,to);
	unlink(from);
#endif
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

    sprintf(arcf, "%s/0.%s", f->backup->dir, bn);

    /* try to rename the file into the archive
     */
#if HAVE_RENAME
    rc = rename(f->path, arcf);
#else
    unlink(arcf);
    rc = link(f->path, arcf);
    if ( rc == 0 )
	unlink(f->path);
#endif

    if ( rc == 0 ) return;

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

} /* Archive */
