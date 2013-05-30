
NULL =

INCLUDES = 						\
	-I$(top_builddir) -I$(top_srcdir)		\
	-DPACKAGE_LIB_DIR=\""$(libdir)"\" 		\
	$(NULL)


sbin_PROGRAMS = p2p-client

p2p_client_SOURCES = 					\
				main.cc			\
	peer.h						\
	service_finder.h	service_finder.cc	\
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
	$(top_builddir)/common/libp2p-util.la		\
	$(NULL)

clean-local :
	rm -rf *~

