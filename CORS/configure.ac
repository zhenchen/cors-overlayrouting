#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(FULL-PACKAGE-NAME, VERSION, BUG-REPORT-ADDRESS)
AM_INIT_AUTOMAKE(cors, 0.3)
AC_CONFIG_SRCDIR([src/proxy/corsproxy.h])
AM_CONFIG_HEADER(config.h)

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_INSTALL
AM_PROG_LIBTOOL

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([netinet/in.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_HEADER_TIME

# Checks for library functions.
AC_PROG_GCC_TRADITIONAL
AC_FUNC_MALLOC
AC_FUNC_SELECT_ARGTYPES
AC_CHECK_FUNCS([gettimeofday select socket strchr])

# Ask user for path to thirdparty libraries
AC_ARG_ENABLE(corsdb, [ --disable-corsdb  Do not compile corsdb ],,)
if test "x${enable_corsdb}" = "x" ; then
   enable_corsdb=yes
fi

if test ${enable_corsdb} = yes; then 
AC_ARG_WITH(mysql, 
	[--with-mysql=<path> prefix of MySQL installation. e.g. /usr/local or /usr],
	[MYSQL_PREFIX=$with_mysql]
	#AC_MSG_ERROR([You must call configure with the --with-mysql option. This tells configure where to find the MySQL C library and headers. e.g. --with-mysql=/usr/local or --with-mysql=/usr])
)
AC_SUBST(MYSQL_PREFIX)
MYSQL_LIBS="-L${MYSQL_PREFIX}/lib/mysql -lmysqlclient"
MYSQL_CFLAGS="-I${MYSQL_PREFIX}/include"
AC_SUBST(MYSQL_LIBS)
AC_SUBST(MYSQL_CFLAGS)
fi

AM_CONDITIONAL(DISABLE_CORSDB, test ${enable_corsdb} = no)

AC_CONFIG_FILES([Makefile
		 src/Makefile
		 src/common/Makefile
		 src/utils/Makefile
		 src/proxy/Makefile
		 src/client/Makefile		 		
		 src/client/corssocket.pc
		 src/corsdb/Makefile
		 src/test/Makefile])
		 
AC_OUTPUT
