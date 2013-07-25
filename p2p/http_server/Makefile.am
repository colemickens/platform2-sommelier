# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

NULL =

INCLUDES = 						\
	-I$(top_builddir) -I$(top_srcdir)	 	\
	-DPACKAGE_LIB_DIR=\""$(libdir)"\" 		\
	$(NULL)

p2p_http_server_sources =				\
	server.h		server.cc		\
	connection_delegate.h	connection_delegate.cc	\
	$(NULL)

# ----------------------------------------------------------------------------

sbin_PROGRAMS = p2p-http-server

p2p_http_server_SOURCES = 				\
	main.cc						\
	$(p2p_http_server_sources)			\
	$(NULL)

p2p_http_server_CXXFLAGS = 				\
	$(LIBCHROME_CFLAGS)				\
	$(GLIB_CFLAGS)					\
	$(NULL)

p2p_http_server_LDADD = 				\
	$(LIBCHROME_LIBS)				\
	$(GLIB_LIBS)					\
	-lpthread					\
	-lattr						\
	$(top_builddir)/common/libp2p-util.la		\
	$(NULL)

# ----------------------------------------------------------------------------

if ENABLE_TESTS

noinst_PROGRAMS = p2p-http-server-unittests
TESTS = p2p-http-server-unittests

p2p_http_server_unittests_SOURCES = 			\
	testrunner.cc					\
	server_unittest.cc				\
	$(p2p_http_server_sources)			\
	$(NULL)

p2p_http_server_unittests_CXXFLAGS = 			\
	$(LIBCHROME_CFLAGS)				\
	$(GLIB_CFLAGS)					\
	$(NULL)

p2p_http_server_unittests_LDADD = 			\
	$(LIBCHROME_LIBS)				\
	$(GLIB_LIBS)					\
	-lgtest -lgtest_main -lgmock			\
	-lattr						\
	$(top_builddir)/common/libp2p-testutil.la	\
	$(NULL)

endif # ENABLE_TESTS

# ----------------------------------------------------------------------------

clean-local :
	rm -rf *~

