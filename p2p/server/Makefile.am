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

# Sources that go into both the program and the tests
p2p_server_sources =					\
	file_watcher.h		file_watcher.cc		\
	peer_update_manager.h	peer_update_manager.cc	\
	service_publisher.h	service_publisher.cc	\
	http_server.h		http_server.cc		\
	$(NULL)

# ------------------------------------------------------------------------

sbin_PROGRAMS = p2p-server

p2p_server_SOURCES = 					\
				main.cc			\
	$(p2p_server_sources)				\
	$(NULL)

p2p_server_CXXFLAGS = 					\
	$(GLIB_CFLAGS)					\
	$(AVAHI_CLIENT_CFLAGS)				\
	$(AVAHI_GLIB_CFLAGS)				\
	$(LIBCHROME_CFLAGS)				\
	$(NULL)

p2p_server_LDADD = 					\
	$(GLIB_LIBS)					\
	$(AVAHI_CLIENT_LIBS)				\
	$(AVAHI_GLIB_LIBS)				\
	$(LIBCHROME_LIBS)				\
	$(top_builddir)/common/libp2p-util.la		\
	$(NULL)

# ------------------------------------------------------------------------

if ENABLE_TESTS

noinst_PROGRAMS = p2p-server-unittests
TESTS = p2p-server-unittests

p2p_server_unittests_SOURCES = 				\
	testrunner.cc					\
	peer_update_manager_unittest.cc			\
	file_watcher_unittest.cc			\
	http_server_unittest.cc				\
	$(p2p_server_sources)				\
	$(NULL)

p2p_server_unittests_CXXFLAGS = 			\
	$(LIBCHROME_CFLAGS)				\
	$(AVAHI_CLIENT_CFLAGS)				\
	$(AVAHI_GLIB_CFLAGS)				\
	$(GLIB_CFLAGS)					\
	$(NULL)

p2p_server_unittests_LDADD = 				\
	$(LIBCHROME_LIBS)				\
	$(AVAHI_CLIENT_LIBS)				\
	$(AVAHI_GLIB_LIBS)				\
	$(GLIB_LIBS)					\
	-lgtest -lgtest_main -lgmock			\
	-lattr						\
	$(top_builddir)/common/libp2p-util.la		\
	$(top_builddir)/common/libp2p-testutil.la	\
	$(NULL)

endif # ENABLE_TESTS

clean-local :
	rm -rf *~
