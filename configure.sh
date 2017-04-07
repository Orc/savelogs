#! /bin/sh

# local options:  ac_help is the help message that describes them
# and LOCAL_AC_OPTIONS is the script that interprets them.  LOCAL_AC_OPTIONS
# is a script that's processed with eval, so you need to be very careful to
# make certain that what you quote is what you want to quote.

# load in the configuration file
#
TARGET=savelogs

. ./configure.inc


# and away we go
#
AC_INIT savelogs

AC_PROG_CC || AC_FAIL "$TARGET needs a (functional) C compiler"
AC_PROG_LEX || AC_FAIL "$TARGET needs lex"
AC_PROG_YACC || AC_FAIL "$TARGET needs yacc"
MF_PATH_INCLUDE COMPRESS gzip compress

if [ "IS_BROKEN_CC" ]; then
    # gcc, clang
    case "$AC_CC $AC_CFLAGS" in
    *-pedantic*) ;;
    *)  # hack around deficiencies in gcc and clang
	#
	AC_DEFINE 'while(x)' 'while( (x) != 0 )'
	AC_DEFINE 'if(x)' 'if( (x) != 0 )'

	if [ "$IS_CLANG" ]; then
	    AC_CC="$AC_CC -Wno-implicit-int"
	elif [ "$IS_GCC" ]; then
	    AC_CC="$AC_CC -Wno-return-type -Wno-implicit-int"
	fi ;;
    esac
fi

case "$CF_COMPRESS" in
*gzip)		AC_DEFINE ZEXT	'".gz"' ;;
*compress)	AC_DEFINE ZEXT	'".Z"' ;;
esac

[ "$OS_FREEBSD" -o "$OS_DRAGONFLY" ] || AC_CHECK_HEADERS malloc.h
AC_CHECK_HEADERS getopt.h
AC_CHECK_FUNCS basename && AC_CHECK_HEADERS libgen.h
AC_CHECK_FUNCS rename

AC_CHECK_NORETURN

AC_DEFINE DEFAULT_CONFIG \"$AC_CONFDIR/savelogs.conf\"

AC_OUTPUT savelogs.8 Makefile

test "$CF_COMPRESS" || LOG "WARNING: cannot compress files"
