#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT([chat], [0.1.0], [boothj5web@gmail.com])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/server/server.c])
AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([foreign subdir-objects])

### Checks for programs.
AC_PROG_CC

AC_CANONICAL_HOST
PLATFORM="unknown"
AS_CASE([$host_os],
    [darwin*], [PLATFORM="osx"],
    [PLATFORM="nix"])

AS_IF([test "x$PLATFORM" = xosx],
    [AC_DEFINE([PLATFORM_OSX], [1], [OSx])])

PKG_CHECK_MODULES([glib], [glib-2.0 >= 2.26], [],
    [AC_MSG_ERROR([glib 2.26 or higher is required])])

AC_CHECK_LIB([pthread], [main], [],
    [AC_MSG_ERROR([pthread is required])])

### Check for ncurses library
PKG_CHECK_MODULES([ncursesw], [ncursesw],
    [NCURSES_CFLAGS="$ncursesw_CFLAGS"; NCURSES_LIBS="$ncursesw_LIBS"; NCURSES="ncursesw"],
    [PKG_CHECK_MODULES([ncurses], [ncurses],
        [NCURSES_CFLAGS="$ncurses_CFLAGS"; NCURSES_LIBS="$ncurses_LIBS"; NCURSES="ncurses"],
        [AC_CHECK_LIB([ncursesw], [main], [],
            [AC_CHECK_LIB([ncurses], [main], [],
                [AC_MSG_ERROR([ncurses is required for chat])])])])])
AM_CPPFLAGS="$AM_CPPFLAGS $NCURSES_CFLAGS"
LIBS="$NCURSES_LIBS $LIBS"
AS_IF([test "x$PLATFORM" = xosx], [LIBS="-lncurses $LIBS"])

### Check wide characters support in ncurses library
CFLAGS_RESTORE="$CFLAGS"
CFLAGS="$CFLAGS $NCURSES_CFLAGS"
AC_CACHE_CHECK([for wget_wch support in $NCURSES], ncurses_cv_wget_wch,
   [AC_LINK_IFELSE([AC_LANG_SOURCE([
       void wget_wch(void);
       int main() {
           wget_wch();
           return 0;
       }
       ])],
       [ncurses_cv_wget_wch=yes],
       [ncurses_cv_wget_wch=no])
   ])
CFLAGS="$CFLAGS_RESTORE"

AS_IF([test "x$ncurses_cv_wget_wch" != xyes],
    [AC_MSG_ERROR([ncurses does not support wide characters])])

### Check for ncursesw/ncurses.h first, Arch linux uses ncurses.h for ncursesw
AC_CHECK_HEADERS([ncursesw/ncurses.h], [], [])
AC_CHECK_HEADERS([ncurses.h], [], [])

AM_CFLAGS="-Wall -Wno-deprecated-declarations"
AM_CFLAGS="$AM_CFLAGS -Wunused -Werror"
AM_CPPFLAGS="$AM_CPPFLAGS $glib_CFLAGS"
LIBS="$glib_LIBS $LIBS"

AC_SUBST(AM_CFLAGS)
AC_SUBST(AM_CPPFLAGS)

### Checks for library functions.
AC_CHECK_FUNCS([atexit memset strdup strstr])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
