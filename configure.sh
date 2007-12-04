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

AC_PROG_CC || AC_FAIL "$TARGET need a (functional) C compiler"
AC_PROG_LEX || AC_FAIL "$TARGET needs lex"
AC_PROG_YACC || AC_FAIL "$TARGET needs yacc"
MF_PATH_INCLUDE COMPRESS gzip compress

case "$CF_COMPRESS" in
*gzip)		AC_DEFINE ZEXT	'".gz"' ;;
*compress)	AC_DEFINE ZEXT	'".Z"' ;;
esac

[ "$OS_FREEBSD" -o "$OS_DRAGONFLY" ] || AC_CHECK_HEADERS malloc.h
AC_CHECK_HEADERS getopt.h
AC_CHECK_FUNCS basename
AC_CHECK_FUNCS rename

AC_SUB VERSION `test -f VERSION && cat VERSION`

AC_OUTPUT savelogs.8 Makefile

test "$CF_COMPRESS" || LOG "WARNING: cannot compress files"
