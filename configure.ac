#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(rslogin, 0.30.2, jason.curtis@verizon.com)
AC_CONFIG_SRCDIR([main.cpp])
AC_CONFIG_HEADER([config.h])

#Check GNU SOURCE
AC_GNU_SOURCE

# Checks for programs.
AC_PROG_CXX
<<<<<<< .mine
#AC_PROG_CC
#AC_PROG_INSTALL
AC_PROG_RANLIB
=======
>>>>>>> .r30

# Checks for libraries.
AC_SEARCH_LIBS(inet_ntoa, nsl)

# Checks for header files.
AC_CHECK_HEADERS([netdb.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_TYPE_SIZE_T

# Checks for library functions.
AC_CHECK_FUNCS([gethostbyname uname])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
