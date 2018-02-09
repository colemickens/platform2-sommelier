#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([camera3hal], [1.0])
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
AC_USE_SYSTEM_EXTENSIONS

AC_ARG_VAR(KERNELSOURCES, path to linux kernel directory (which contains psys driver))

AC_ARG_WITH([kernel-sources], AS_HELP_STRING([--with-kernel-sources=DIR], \
    [path to linux kernel directory (which contains psys driver)]), [], [with_kernel_sources=no])
AC_ARG_WITH([ipu], AS_HELP_STRING([--with-ipu=VERSION], \
    [IPU version which is wanted to be used. Defaults to ipu3.]), [], with_ipu=no)
AC_ARG_WITH([base-version], AS_HELP_STRING([--with-base-version=VERSION], \
    [libbase version]), [], [with_base_version=no])

AC_ARG_ENABLE([remote3a],
    AS_HELP_STRING([--enable-remote3a], [Enable remote 3A]))

AS_IF([test "x$with_kernel_sources" != xno], [KERNELSOURCES=$with_kernel_sources], [])
AS_IF([test "x$with_ipu" != xno], [IPUVERSION=$with_ipu], [IPUVERSION="ipu3"])
AS_IF([test "x$with_base_version" != xno], [BASEVER=$with_base_version],
    [AC_MSG_ERROR([libbase version is not set])])

AS_IF([test "$KERNELSOURCES" != ""], \
      [PSYSH2600="$KERNELSOURCES"/include/uapi/])

AC_SUBST(PSYSH2600)
AC_SUBST(KERNELSOURCES)

AM_CONDITIONAL([NOPSYS], [test "$PSYSH2600" = ""])
AM_CONDITIONAL([IPU3], [test x"$IPUVERSION" = xipu3])

AM_CONDITIONAL([REMOTE_3A_SERVER], [test x"$enable_remote3a" = xyes])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile])
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

PKG_CHECK_MODULES([EXPAT],[expat],[],
    [AC_CHECK_HEADER([expat.h],[], [AC_MSG_ERROR([Expat headers required by GCSS not found])])
    AC_CHECK_LIB([expat],[XML_ParserCreate],[],[AC_MSG_ERROR([Expat library required by GCSS  not found])])
    EXPAT_LIBS="-lexpat"])

PKG_CHECK_MODULES([LIBCHROME], [libchrome-$BASEVER])
PKG_CHECK_MODULES([CROS_CAMERA_ANDROID_HEADERS], [cros-camera-android-headers])
PKG_CHECK_MODULES([LIBCAMERA_METADATA], [libcamera_metadata])
PKG_CHECK_MODULES([LIBCAMERA_CLIENT], [libcamera_client])
PKG_CHECK_MODULES([LIBCBM], [libcbm])
PKG_CHECK_MODULES([LIBSYNC], [libsync])
PKG_CHECK_MODULES([LIBYUV], [libyuv])
PKG_CHECK_MODULES([IA_IMAGING], [ia_imaging])
PKG_CHECK_MODULES([LIBCAMERA_JPEG], [libcamera_jpeg])
PKG_CHECK_MODULES([LIBCAMERA_COMMON], [libcamera_common])

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
