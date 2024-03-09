#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.71])
AC_INIT([geanyproject],[0.1.0],[endeavor2wako@gmail.com])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PATH_PROG(PKG_CONFIG, pkg-config, no)
IT_PROG_INTLTOOL
GETTEXT_PACKAGE=$PACKAGE
AC_SUBST(GETTEXT_PACKAGE)

# Checks for libraries.
AC_ARG_ENABLE(chooser,
    AS_HELP_STRING([--disable-chooser],[force to use GtkFileSelector]))
if test "$enable_chooser" != "no"; then
    PKG_CHECK_EXISTS(gtk+-2.0 >= 2.24.0,
    #~ PKG_CHECK_EXISTS(gtk+-3.0 >= 3.18.0,
        AC_DEFINE(ENABLE_CHOOSER, 1, [Define if GtkFileChooser is enabled.]))
fi

PKG_CHECK_MODULES(GTK, gtk+-3.0)

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h libintl.h stdlib.h string.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Checks for library functions.
AC_CHECK_FUNCS([select setlocale strchr strrchr strstr])

AC_CONFIG_FILES([Makefile po/Makefile.in
                 src/Makefile])
AC_OUTPUT
