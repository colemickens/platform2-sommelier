# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

CXX ?= g++
CXXFLAGS ?= -fno-strict-aliasing -Wall -Wextra -Werror -Wuninitialized
CPPFLAGS ?= -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS
AR ?= ar
PKG_CONFIG ?= pkg-config
DBUSXX_XML2CPP = dbusxx-xml2cpp

# libevent, gdk and gtk-2.0 are needed to leverage chrome's MessageLoop
# TODO(cmasone): explore if newer versions of libbase let us avoid this.
BASE_LIBS = -lbase -lchromeos -levent -lpthread -lrt
BASE_INCLUDE_DIRS = -I..
BASE_LIB_DIRS =

LIBS = $(BASE_LIBS)
INCLUDE_DIRS = $(BASE_INCLUDE_DIRS) $(shell $(PKG_CONFIG) --cflags dbus-c++-1 \
	glib-2.0 gdk-2.0 gtk+-2.0)
LIB_DIRS = $(BASE_LIB_DIRS) $(shell $(PKG_CONFIG) --libs dbus-c++-1 glib-2.0 \
	gdk-2.0 gtk+-2.0)

TEST_LIBS = $(BASE_LIBS) -lgmock -lgtest
TEST_INCLUDE_DIRS = $(INCLUDE_DIRS)
TEST_LIB_DIRS = $(LIB_DIRS)

DBUS_ADAPTOR_HEADERS = \
	flimflam-device.h \
	flimflam-ipconfig.h \
	flimflam-manager.h \
	flimflam-profile.h \
	flimflam-service.h

DBUS_PROXY_HEADERS = \
	dhcpcd.h \
	supplicant-bss.h \
	supplicant-interface.h \
	supplicant-network.h \
	supplicant-process.h

DBUS_HEADERS = $(DBUS_ADAPTOR_HEADERS) $(DBUS_PROXY_HEADERS)

SHILL_OBJS = \
	dbus_adaptor.o \
	dbus_control.o \
	device.o \
	device_dbus_adaptor.o \
	device_info.o \
	dhcp_config.o \
	dhcp_provider.o \
	dhcpcd_proxy.o \
	ethernet.o \
	ethernet_service.o \
	glib.o \
	glib_io_handler.o \
	ipconfig.o \
	manager.o \
	manager_dbus_adaptor.o \
	rtnl_handler.o \
	rtnl_listener.o \
	service.o \
	service_dbus_adaptor.o \
	shill_config.o \
	shill_daemon.o \
	shill_event.o \
	wifi.o

SHILL_BIN = shill
SHILL_MAIN_OBJ = shill_main.o

TEST_BIN = shill_unittest
TEST_OBJS = \
	dbus_adaptor_unittest.o \
	device_info_unittest.o \
	dhcp_config_unittest.o \
	ipconfig_unittest.o \
	manager_unittest.o \
	mock_control.o \
	mock_device.o \
	mock_service.o \
	shill_unittest.o \
	testrunner.o

all: $(SHILL_BIN) $(TEST_BIN)
integration_tests: wifi_integrationtest

$(DBUS_PROXY_HEADERS): %.h: %.xml
	$(DBUSXX_XML2CPP) $< --proxy=$@

$(DBUS_ADAPTOR_HEADERS): %.h: %.xml
	$(DBUSXX_XML2CPP) $< --adaptor=$@

.cc.o:
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(INCLUDE_DIRS) -c $< -o $@

$(SHILL_OBJS): $(DBUS_HEADERS)

$(SHILL_BIN): $(SHILL_MAIN_OBJ) $(SHILL_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) $(LIB_DIRS) $(LDFLAGS) $^ $(LIBS) \
		-o $@

$(TEST_BIN): CXXFLAGS += -DUNIT_TEST
$(TEST_BIN): $(TEST_OBJS) $(SHILL_OBJS)
	$(CXX) $(CXXFLAGS) $(TEST_INCLUDE_DIRS) $(TEST_LIB_DIRS) $(LDFLAGS) $^ \
		$(TEST_LIBS) -o $@

# NB(quiche): statically link gmock, gtest, as test device will not have them
wifi_integrationtest: CXXFLAGS += -DUNIT_TEST
wifi_integrationtest: wifi_integrationtest.o $(SHILL_OBJS)
	$(CXX) $(CXXFLAGS) $(TEST_INCLUDE_DIRS) $(TEST_LIB_DIRS) $(LDFLAGS) $^ \
		$(BASE_LIBS) -Wl,-Bstatic -lgmock -lgtest -Wl,-Bdynamic -o $@

clean:
	rm -rf *.o $(DBUS_HEADERS) $(SHILL_BIN) $(TEST_BIN)
