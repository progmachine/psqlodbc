# Process this file with autoconf to produce a configure script.
AC_INIT(psqlodbc, 17.00.0006, [pgsql-odbc@postgresql.org])
AC_PREREQ(2.57)
AC_CONFIG_AUX_DIR(config)
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([bind.c])
AM_CONFIG_HEADER([config.h])
AM_MAINTAINER_MODE


# 0. Options processing

AC_PROG_CC

AM_CONDITIONAL([GCC], [test -n "$GCC"])

# All AC_CHECK_SIZEOF() or AC_CHECK_TYPES() calls were move from 4.
# unixODBC wants the following to get sane behavior for ODBCINT64
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(long int)	# for unixODBC
AC_CHECK_SIZEOF(void *)
AC_CHECK_TYPES([long long])
AC_CHECK_TYPES([signed char])
AC_CHECK_TYPES([ssize_t])

AC_CHECK_SIZEOF([bool], [],
[#ifdef HAVE_STDBOOL_H
#include <stdbool.h>
#endif])

AC_CHECK_HEADER_STDBOOL()

dnl We use <stdbool.h> if we have it and it declares type bool as having
dnl size 1.  Otherwise, c.h will fall back to declaring bool as unsigned char.
if test "$ac_cv_header_stdbool_h" = yes && test "$ac_cv_sizeof_bool" = 1; then
  AC_DEFINE([PG_USE_STDBOOL], 1,
            [Define to 1 to use <stdbool.h> to define type bool.])
fi

AC_TYPE_SIZE_T

# Check if "-Wall" is valid
if test -n "$GCC" && test "$ac_test_CFLAGS" != set; then

	AC_MSG_CHECKING(-Wall is a valid compile option)
	CFLAGS_SAVE="${CFLAGS}"
	CFLAGS="${CFLAGS} -Wall"
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
		[[#include <stdio.h>]],
		[])],
		[AC_MSG_RESULT(yes)],
		[AC_MSG_RESULT(no)
		 CFLAGS="${CFLAGS_SAVE}"])
fi

#
# Whether unixODBC driver manager is used
#
AC_ARG_WITH(unixodbc, [  --with-unixodbc[[=DIR]]	  [[default=yes]] DIR is the unixODBC base install
			  directory or the path to odbc_config],
[], [with_unixodbc=yes])


#
# Whether iODBC driver manager is used
#
AC_ARG_WITH(iodbc, [  --with-iodbc[[=DIR]]	  [[default=no]] DIR is the iODBC base install
			  directory or the path to iodbc-config],
[], [with_iodbc=no])


if test "$with_iodbc" != no; then
	with_unixodbc=no
	AC_DEFINE(WITH_IODBC, 1, [Define to 1 to build with iODBC support])
	if test "$with_iodbc" = yes; then
		AC_PATH_PROGS(ODBC_CONFIG, iodbc-config)
	else
		ODBC_CONFIG=$with_iodbc
	fi
	if test ! -x "${ODBC_CONFIG}/bin/iodbc-config"; then
		if test ! -x "${ODBC_CONFIG}"; then
			AC_MSG_ERROR([iodbc-config not found (required for iODBC build)])
		fi
	else
		ODBC_CONFIG=${ODBC_CONFIG}/bin/iodbc-config
	fi
fi

wo_odbc_config=__without_odbc_config
if test "$with_unixodbc" != no; then
	AC_DEFINE(WITH_UNIXODBC, 1,
            [Define to 1 to build with unixODBC support])
	if test "$with_unixodbc" = yes; then
		AC_PATH_PROGS(ODBC_CONFIG, odbc_config)
	else
		ODBC_CONFIG=$with_unixodbc
	fi
	if test "${ODBC_CONFIG}" = "${wo_odbc_config}"; then
		:
	elif test ! -x "${ODBC_CONFIG}/bin/odbc_config"; then
		if test ! -x "${ODBC_CONFIG}"; then
			AC_MSG_ERROR([odbc_config not found (required for unixODBC build)])
		fi
	else
		ODBC_CONFIG=${ODBC_CONFIG}/bin/odbc_config
	fi
fi

#
# ODBC include and library
#

if test "$ODBC_CONFIG" != ""; then
	if test "$with_iodbc" != no; then
		ODBC_INCLUDE=`${ODBC_CONFIG} --cflags`
		CPPFLAGS="$CPPFLAGS ${ODBC_INCLUDE}"
		# Linking libiodoc is rather problematic
		[ODBC_LIBDIR=`${ODBC_CONFIG} --libs | sed -e "s/^\(-L\|.*[ \t]-L\)\([^ \n\r\f\t]*\).*$/-L\2/"`]
		LDFLAGS="$LDFLAGS ${ODBC_LIBDIR}"
		LIBODBC=`${ODBC_CONFIG} --libs`	# for regression test
	elif test "${ODBC_CONFIG}" = "${wo_odbc_config}"; then
		ODBC_INCLUDE=/usr/include
		CPPFLAGS="$CPPFLAGS -I${ODBC_INCLUDE}"
		ODBC_LIBDiR=""
		LIBODBC="-lodbc"	# for regression test
	else
		ODBC_INCLUDE=`${ODBC_CONFIG} --include-prefix`
		CPPFLAGS="$CPPFLAGS -I${ODBC_INCLUDE}"
		# Linking libodbc is rather problematic
		ODBC_LIBDIR=`${ODBC_CONFIG} --lib-prefix`
		if test "${ODBC_LIBDIR}" != ""; then
			LDFLAGS="$LDFLAGS -L${ODBC_LIBDIR}"
		fi
		LIBODBC=`${ODBC_CONFIG} --libs`	# for regression test
	fi
	AC_MSG_NOTICE([using $ODBC_INCLUDE $ODBC_LIBDIR])
	AC_MSG_NOTICE([using $LIBODBC for regression test])
fi
AC_SUBST(LIBODBC)

#
# SQLCOLATTRIBUTE_SQLLEN check
#
AC_MSG_CHECKING(last argument to SQLColAttribute is SQLLEN *)
AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
#include <sql.h>

SQLRETURN SQL_API SQLColAttribute(SQLHSTMT StatementHandle,
  SQLUSMALLINT ColumnNumber,
  SQLUSMALLINT FieldIdentifier,
  SQLPOINTER CharacterAttribute,
  SQLSMALLINT BufferLength,
  SQLSMALLINT *StringLength,
  SQLLEN *NumericAttribute)
{
  SQLLEN t = 1;
  *NumericAttribute = t;
  return 0;
}
]])],
  [AC_DEFINE(SQLCOLATTRIBUTE_SQLLEN, 1,
	[Define to 1 if SQLColAttribute use SQLLEN])
   AC_MSG_RESULT(yes)],
  [AC_MSG_RESULT(no)]
)


#
# Whether libpq functionalities are used
#
AC_ARG_WITH(libpq, [  --with-libpq[[=DIR]]	  DIR is the PostgreSQL base install
			  directory or the path to pg_config],
[], [with_libpq=yes])

if test "$with_libpq" = no; then
	AC_MSG_ERROR([libpq is mandatory, --without-libpq is no longer supported])
fi

if test "$with_libpq" != yes; then
	if test -d "$with_libpq"; then
		PATH="$with_libpq/bin:$PATH"
		CPPFLAGS="$CPPFLAGS -I$with_libpq/include -I$with_libpq/include/postgresql/internal"
		LDFLAGS="$LDFLAGS -L$with_libpq/lib"
	else
		if test -x "$with_libpq"; then
			PG_CONFIG=$with_libpq
		else
			AC_MSG_ERROR([specified pg_config not found])
		fi
	fi
fi

#
# Pthreads
#

PGAC_ARG_BOOL(enable, pthreads, yes,
              [  --disable-pthreads      do not build with POSIX threads],
              [AC_DEFINE(POSIX_MULTITHREAD_SUPPORT, 1,
                         [Define to 1 to build with pthreads support (--enable-pthreads)])
               AC_DEFINE(_REENTRANT, 1, [Define _REENTRANT for several platforms])])


#
# Find libpq headers and libraries
#

if test -z "$PG_CONFIG"; then
  AC_PATH_PROGS(PG_CONFIG, pg_config)
fi

if test -n "$PG_CONFIG"; then
  pg_includedir=`"$PG_CONFIG" --includedir`
  pg_pkg_includedir=`"$PG_CONFIG" --pkgincludedir`
  pg_libdir=`"$PG_CONFIG" --libdir`
  CPPFLAGS="$CPPFLAGS -I$pg_includedir -I$pg_pkg_includedir/internal"
  LDFLAGS="$LDFLAGS -L$pg_libdir"
fi



# 1. Programs

# 'prove' is used to pretty-print regression test results, if available.
AC_CHECK_PROGS(PROVE, prove)

# 2. Libraries

AC_LIBTOOL_WIN32_DLL
AC_DISABLE_STATIC
AC_LIBTOOL_DLOPEN
AC_PROG_LIBTOOL

if test "$with_unixodbc" != no; then
  AC_SEARCH_LIBS(SQLGetPrivateProfileString, odbcinst, [],
                 [AC_MSG_ERROR([unixODBC library "odbcinst" not found])])
fi

if test "$with_iodbc" != no; then
  AC_SEARCH_LIBS(SQLGetPrivateProfileString, iodbcinst, [],
                 [AC_MSG_ERROR([iODBC library "iodbcinst" not found])])
fi

if test "$enable_pthreads" = yes; then
  AC_CHECK_LIB(pthreads, pthread_create,
               [],
	       [AC_CHECK_LIB(pthread, pthread_create)])
fi

AC_CHECK_LIB(pq, PQsetSingleRowMode, [],
	      [AC_MSG_ERROR([libpq library version >= 9.2 is required])])

# 3. Header files

AC_CHECK_HEADERS(locale.h sys/time.h uchar.h)
AC_CHECK_HEADER(libpq-fe.h,,[AC_MSG_ERROR([libpq header not found])])
AC_HEADER_TIME
AC_HEADER_STDBOOL


# 4. Types(all members were moved to 0.)

# 5. Structures

AC_STRUCT_TM

# 6. Compiler characteristics

AC_C_CONST


# 7. Functions, global variables

AC_FUNC_STRERROR_R
AC_CHECK_FUNCS(strtoul strtoll strlcat mbstowcs wcstombs mbrtoc16 c16rtomb)

AC_CHECK_FUNCS(PQsslInUse)

if test "$enable_pthreads" = yes; then
  AC_CHECK_FUNCS(localtime_r strtok_r pthread_mutexattr_settype)

  if test x"$ac_cv_func_pthread_mutexattr_settype" = xyes; then
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <pthread.h>]],
                 [[int i = PTHREAD_MUTEX_RECURSIVE;]])],
                 [AC_DEFINE(PG_RECURSIVE_MUTEXATTR, PTHREAD_MUTEX_RECURSIVE,
                            [Define if you have PTHREAD_MUTEX_RECURSIVE])],
                 [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <pthread.h>]],
		                 [[int i = PTHREAD_MUTEX_RECURSIVE_NP;]])],
		                 [AC_DEFINE(PG_RECURSIVE_MUTEXATTR, PTHREAD_MUTEX_RECURSIVE_NP,
                                            [Define if you have PTHREAD_MUTEX_RECURSIVE_NP])])])
  fi
fi

# 8. Libltdl	This release doesn't need libltdl
# AC_CHECK_LIB(ltdl, lt_dlopen)


AC_CONFIG_FILES([Makefile test/Makefile])
AC_OUTPUT
