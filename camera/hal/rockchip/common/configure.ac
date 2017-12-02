#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([halcommon], [1.0])
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
AC_USE_SYSTEM_EXTENSIONS

AC_ARG_WITH([os], AS_HELP_STRING([--with-os=PLATFORM], \
    [OS which is wanted to be used. Defaults to linux.]), [], with_os=no)

AS_IF([test "x$with_os" != xno], [OS=$with_os], [OS="linux"])

AC_CONFIG_FILES([Makefile libhalcommon.pc])
AC_CONFIG_MACRO_DIR([m4])


# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_MAKE_SET

# Checks for libraries.
LT_PREREQ([2.4.2])
LT_INIT

PKG_CHECK_MODULES([IA_IMAGING], [ia_imaging])
PKG_CHECK_MODULES([LIBIACSS], [libiacss])
PKG_CHECK_MODULES([LIBCAMERANVM], [libcameranvm])
PKG_CHECK_MODULES([EXPAT],[expat],[],
    [AC_CHECK_HEADER([expat.h],[], [AC_MSG_ERROR([Expat headers required by GCSS not found])])
    AC_CHECK_LIB([expat],[XML_ParserCreate],[],[AC_MSG_ERROR([Expat library required by GCSS  not found])])
    EXPAT_LIBS="-lexpat"])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h inttypes.h limits.h stdint.h stdlib.h string.h sys/ioctl.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_INT8_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_CHECK_MEMBERS([struct stat.st_rdev])
AC_STRUCT_TIMEZONE
AC_TYPE_UID_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_FUNC_STRNLEN
AC_CHECK_FUNCS([memmove memset munmap pow realpath strchr strerror strncasecmp strrchr strspn strstr strtol])

AC_OUTPUT
