# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

NULL =

INCLUDES = 						\
	-I$(top_builddir) -I$(top_srcdir)	 	\
	-DPACKAGE_LIB_DIR=\""$(libdir)"\" 		\
	-DPACKAGE_SBIN_DIR=\""$(sbindir)"\" 		\
	-DTOP_BUILDDIR=\""$(top_builddir)"\" 		\
	$(NULL)

noinst_LTLIBRARIES = libp2p-util.la

# ------------------------------------------------------------------------

libp2p_util_la_SOURCES =				\
	clock_interface.h				\
	clock.h			clock.cc		\
	constants.h					\
	server_message.h	server_message.cc	\
	struct_serializer.h				\
	util.h			util.cc			\
	$(NULL)

libp2p_util_la_CXXFLAGS = 				\
	$(LIBCHROME_CFLAGS)				\
	$(GLIB_CFLAGS)					\
	-D__STDC_FORMAT_MACROS=1			\
	$(NULL)

libp2p_util_la_LIBADD = 				\
	-lrt						\
	$(NULL)

# ------------------------------------------------------------------------

if ENABLE_TESTS

noinst_LTLIBRARIES += libp2p-testutil.la

libp2p_testutil_la_SOURCES =				\
	constants.h					\
	fake_clock.h					\
	testutil.h		testutil.cc		\
	$(NULL)

libp2p_testutil_la_CXXFLAGS = 				\
	$(LIBCHROME_CFLAGS)				\
	$(GLIB_CFLAGS)					\
	-D__STDC_FORMAT_MACROS=1			\
	$(NULL)

endif # ENABLE_TESTS

# ------------------------------------------------------------------------

if ENABLE_TESTS

noinst_PROGRAMS = libp2p-unittests
TESTS = libp2p-unittests

libp2p_unittests_SOURCES = 				\
	testrunner.cc					\
	server_message_unittest.cc			\
	struct_serializer_unittest.cc			\
	testutil_unittest.cc				\
	$(NULL)

libp2p_unittests_CXXFLAGS = 				\
	$(LIBCHROME_CFLAGS)				\
	$(GLIB_CFLAGS)					\
	-D__STDC_FORMAT_MACROS=1			\
	$(NULL)

libp2p_unittests_LDADD = 				\
	$(LIBCHROME_LIBS)				\
	$(GLIB_LIBS)					\
	-lgtest -lgtest_main -lgmock			\
	-lgmock						\
	libp2p-util.la					\
	libp2p-testutil.la				\
	$(NULL)

endif # ENABLE_TESTS

clean-local :
	rm -rf *~
