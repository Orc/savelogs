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

if ! AC_PROG_CC; then
    LOG "You need to have a functional C compiler to build savelogs"
    exit 1
fi

if ! AC_PROG_LEX; then
    LOG "You need lex or flex to build savelogs"
    exit 1
fi

AC_CHECK_HEADERS getopt.h

AC_SUB VERSION `test -f VERSION && cat VERSION`

AC_OUTPUT savelogs.8 Makefile
