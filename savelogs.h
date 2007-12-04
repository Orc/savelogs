#include "y.tab.h"

#define SECSPERDAY	24*60*60	/* how many seconds in a day */

/*
 * Commands to execute after a file has been shuffled around.
 */
typedef struct _job_t {
    char *command;		/* the commands; input to system() */
    int triggered;		/* has this command already fired? */
    int active;			/* does this command need to be fired? */
    int links;			/* how many victims use this command */
    struct _job_t *next;	/* next in chain */
} job_t;


/*
 * Backup copies?
 */
typedef struct _backup_t {
    char *dir;
    int count;
} backup_t;


/*
 * array type manipulators
 */
#define ARY(x)	struct { x *items; int count; }
#define IT(x,i)	((x).items[i])
#define NR(x)	((x).count)
#define GROW(x)	(((x).items = (x).count ? \
	    realloc((x).items, sizeof ((x).items[0]) * ( 1 + (x).count) ) : \
	    malloc(sizeof((x).items[0]) * (1 + (x).count)) ), \
	    ((x).count += 1) )

/*
 * A file to save or truncate
 */
typedef struct _log_t {
    char *path;		/* the file we're administering */
    char *class;	/* log maintenance class */
    unsigned long size;	/* roll out if larger than this size */
    backup_t* backup;	/* backup to directory?  how many times? */
    int interval;	/* how often to roll out the barrel */
    int touch;		/* manually create the file with given mode */
    ARY(job_t*) jobs;	/* things to do when backup fires */
    struct _log_t *next;/* next in chain */
} log_t;


#define YYSTYPE int
