# TODO(zeuthen): Convert buildsystem to common.mk, see
#                https://code.google.com/p/chromium/issues/detail?id=244632

AC_PREREQ([2.64])
AC_INIT([p2p],
        [0.1],
        [mailto:zeuthen@google.com],
        [p2p])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.11 foreign dist-bzip2])

AM_SILENT_RULES([yes])

# Check for programs
AC_PROG_CC
AC_PROG_CXX
AC_PROG_LIBTOOL

PKG_PROG_PKG_CONFIG()
PKG_CHECK_MODULES(GLIB, [gio-2.0])
PKG_CHECK_MODULES(AVAHI_GLIB, [avahi-glib])
PKG_CHECK_MODULES(AVAHI_CLIENT, [avahi-client])
PKG_CHECK_MODULES(LIBCHROME, [libchrome-180609])

AC_CHECK_LIB([attr], [setxattr])
AC_CHECK_HEADERS([attr/xattr.h])

if test "x$GCC" = "xyes"; then
        GCC_CFLAGS="-Wall -g -Wstrict-prototypes -Wmissing-prototypes -fvisibility=hidden"
fi
AC_SUBST(GCC_CFLAGS)

CXXFLAGS="$CXXFLAGS -Wall -std=gnu++0x -fno-rtti"

AC_ARG_ENABLE(tests,
              AS_HELP_STRING([--enable-tests],
              [build and run unit tests]))

AM_CONDITIONAL([ENABLE_TESTS], [test "$enable_tests" != no])

AC_CONFIG_FILES([Makefile
                 client/Makefile
                 common/Makefile
                 data/Makefile
                 http_server/Makefile
                 server/Makefile])
AC_OUTPUT
