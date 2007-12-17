#ifndef _CSTRING_D
#define _CSTRING_D

#include <stdlib.h>

#define STRING(type)	struct { type *text; int size, alloc; }

#define RESERVE(x,c)	(x).text = malloc(sizeof T(x)[0] * (((x).size=0),((x).alloc=(c))) )
#define CREATE(x)	RESERVE(x,100)
#define EXPAND(x)	(x).text[((x).size < (x).alloc \
			    ? 0 \
			    : !((x).text = realloc((x).text, sizeof T(x)[0] * ((x).alloc += 100)))), \
			(x).size++]

#define T(x)		(x).text
#define S(x)		(x).size

typedef STRING(char) Cstring;

#endif/*_CSTRING_D*/
