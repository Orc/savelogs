#include "y.tab.h"

#include "cstring.h"

#define SECSPERDAY	24*60*60	/* how many seconds in a day */

/*
 * Commands to execute after a file has been shuffled around.
 */
typedef struct _signal_t {
    char *command;		/* the commands; input to system() */
    int triggered;		/* has this command already fired? */
    int active;			/* does this command need to be fired? */
    int links;			/* how many victims use this command */
    struct _signal_t *next;	/* next in chain */
} signal_t;


/*
 * Backup copies?
 */
typedef struct _backup_t {
    char *dir;
    int count;
} backup_t;


/*
 * I want each log_t to be able to inherit true/false settings from 
 * the global environment iff those haven't been set locally.  Thus
 * this database style true/false/null instead of the traditional
 * scalar boolean.
 */
typedef int* Flag;

#define true	(Flag)1
#define false	(Flag)2


/*
 * A file to save or truncate
 */
typedef struct _log_t {
    char *path;			/* the file we're administering */
    char *class;		/* log maintenance class */
    size_t size;		/* roll out if larger than this size */
    backup_t* backup;		/* backup to directory?  how many times? */
    int truncate;		/* trim the file back to zero size */
    int interval;		/* how often to roll out the barrel */
    int touch;			/* manually create the file with given mode */
    Flag compress_them;		/* compress archived files */
    Flag dotted_backup;		/* use oooo prefix or suffix */
    Flag backup_suffix;		/* where the backup# goes */
    STRING(signal_t*) signals;	/* things to do when backup fires */
    struct _log_t *next;	/* next in chain */
} log_t;


#define YYSTYPE int
