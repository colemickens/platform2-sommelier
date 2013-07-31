# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

NULL =

INCLUDES = 						\
	-I$(top_builddir) -I$(top_srcdir)		\
	-DPACKAGE_LIB_DIR=\""$(libdir)"\" 		\
	$(NULL)

p2p_client_sources =					\
	clock_interface.h				\
	clock.h			clock.cc		\
	peer.h						\
	peer_selector.h		peer_selector.cc	\
	service_finder.h	service_finder.cc	\
	$(NULL)

# ----------------------------------------------------------------------------

sbin_PROGRAMS = p2p-client

p2p_client_SOURCES = 					\
	main.cc						\
	$(p2p_client_sources)				\
	$(NULL)

p2p_client_CXXFLAGS = 					\
	$(GLIB_CFLAGS)					\
	$(AVAHI_CLIENT_CFLAGS)				\
	$(AVAHI_GLIB_CFLAGS)				\
	$(LIBCHROME_CFLAGS)				\
	$(NULL)

p2p_client_LDADD = 					\
	$(GLIB_LIBS)					\
	$(AVAHI_CLIENT_LIBS)				\
	$(AVAHI_GLIB_LIBS)				\
	$(LIBCHROME_LIBS)				\
	-lmetrics					\
	-lrt						\
	$(top_builddir)/common/libp2p-util.la		\
	$(NULL)

# ----------------------------------------------------------------------------

if ENABLE_TESTS

noinst_PROGRAMS = p2p-client-unittests
TESTS = p2p-client-unittests

p2p_client_unittests_SOURCES = 				\
	testrunner.cc					\
	fake_clock.h					\
	fake_service_finder.cc	fake_service_finder.h	\
	peer_selector_unittest.cc			\
	$(p2p_client_sources)				\
	$(NULL)

p2p_client_unittests_CXXFLAGS = 			\
	-Wall -Werror					\
	$(GLIB_CFLAGS)					\
	$(AVAHI_CLIENT_CFLAGS)				\
	$(AVAHI_GLIB_CFLAGS)				\
	$(LIBCHROME_CFLAGS)				\
	$(NULL)

p2p_client_unittests_LDADD = 				\
	$(GLIB_LIBS)					\
	$(AVAHI_CLIENT_LIBS)				\
	$(AVAHI_GLIB_LIBS)				\
	$(LIBCHROME_LIBS)				\
	-lgtest -lgtest_main -lgmock			\
	-lrt						\
	$(top_builddir)/common/libp2p-util.la		\
	$(NULL)

endif # ENABLE_TESTS

# ----------------------------------------------------------------------------

clean-local :
	rm -rf *~

