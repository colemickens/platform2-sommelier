# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

CXX ?= g++
CXXFLAGS ?= -fno-strict-aliasing
CXXFLAGS += -Wall -Wextra -Werror -Wuninitialized -Woverloaded-virtual
# Disable some errors, which occur repeatedly in the dbus-c++ headers.
CXXFLAGS += -Wno-ignored-qualifiers -Wno-unused
CPPFLAGS ?= -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS
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
TEST_LIB_DIRS = $(LIB_DIRS)

DBUS_BINDINGS_DIR = dbus_bindings

DBUS_ADAPTOR_HEADERS = \
	flimflam-device.h \
	flimflam-ipconfig.h \
	flimflam-manager.h \
	flimflam-profile.h \
	flimflam-service.h

DBUS_PROXY_HEADERS = \
	dbus-properties.h \
	dhcpcd.h \
	supplicant-bss.h \
	supplicant-interface.h \
	supplicant-network.h \
	supplicant-process.h

# Generates rules for copying SYSROOT XMLs locally and updates the proxy header
# dependencies.
DBUS_BINDINGS_XML_SYSROOT = \
	org.freedesktop.ModemManager>modem_manager \
	org.freedesktop.ModemManager.Modem>modem \
	org.freedesktop.ModemManager.Modem.Cdma>modem-cdma \
	org.freedesktop.ModemManager.Modem.Gsm.Card>modem-gsm-card \
	org.freedesktop.ModemManager.Modem.Gsm.Network>modem-gsm-network \
	org.freedesktop.ModemManager.Modem.Simple>modem-simple

define ADD_BINDING
$(eval _SOURCE = $(word 1,$(subst >, ,$(1))))
$(eval _TARGET = $(word 2,$(subst >, ,$(1))))
CLEAN_FILES += $(DBUS_BINDINGS_DIR)/$(_TARGET).xml
DBUS_PROXY_HEADERS += $(_TARGET).h
$(DBUS_BINDINGS_DIR)/$(_TARGET).xml: \
	$(SYSROOT)/usr/share/dbus-1/interfaces/$(_SOURCE).xml
	cat $$< > $$@
endef

$(foreach b,$(DBUS_BINDINGS_XML_SYSROOT),$(eval $(call ADD_BINDING,$(b))))

DBUS_ADAPTOR_BINDINGS = \
	$(addprefix $(DBUS_BINDINGS_DIR)/, $(DBUS_ADAPTOR_HEADERS))
DBUS_PROXY_BINDINGS = $(addprefix $(DBUS_BINDINGS_DIR)/, $(DBUS_PROXY_HEADERS))
DBUS_BINDINGS = $(DBUS_ADAPTOR_BINDINGS) $(DBUS_PROXY_BINDINGS)

SHILL_OBJS = \
	byte_string.o \
	cellular.o \
	cellular_service.o \
	connection.o \
	crypto_des_cbc.o \
	crypto_provider.o \
	crypto_rot47.o \
	dbus_adaptor.o \
	dbus_control.o \
	dbus_properties.o \
	dbus_properties_proxy.o \
	default_profile.o \
	device.o \
	device_dbus_adaptor.o \
	device_info.o \
	dhcp_config.o \
	dhcp_provider.o \
	dhcpcd_proxy.o \
	endpoint.o \
	ephemeral_profile.o \
	error.o \
	ethernet.o \
	ethernet_service.o \
	glib.o \
	glib_io_handler.o \
	ip_address.o \
	ipconfig.o \
	ipconfig_dbus_adaptor.o \
	key_file_store.o \
	manager.o \
	manager_dbus_adaptor.o \
	modem.o \
	modem_cdma_proxy.o \
	modem_info.o \
	modem_manager.o \
	modem_manager_proxy.o \
	modem_proxy.o \
	modem_simple_proxy.o \
	profile.o \
	profile_dbus_adaptor.o \
	property_store.o \
	proxy_factory.o \
	routing_table.o \
	resolver.o \
	rtnl_handler.o \
	rtnl_listener.o \
	rtnl_message.o \
	service.o \
	service_dbus_adaptor.o \
	shill_config.o \
	shill_daemon.o \
	shill_event.o \
	sockets.o \
	supplicant_interface_proxy.o \
	supplicant_process_proxy.o \
	wifi.o \
	wifi_endpoint.o \
	wifi_service.o

SHILL_BIN = shill
SHILL_MAIN_OBJ = shill_main.o

TEST_BIN = shill_unittest
TEST_OBJS = \
	byte_string_unittest.o \
	cellular_unittest.o \
	crypto_des_cbc_unittest.o \
	crypto_provider_unittest.o \
	crypto_rot47_unittest.o \
	connection_unittest.o \
	dbus_adaptor_unittest.o \
	dbus_properties_unittest.o \
	default_profile_unittest.o \
	device_info_unittest.o \
	device_unittest.o \
	dhcp_config_unittest.o \
	dhcp_provider_unittest.o \
	error_unittest.o \
	ip_address_unittest.o \
	ipconfig_unittest.o \
	key_file_store_unittest.o \
	manager_unittest.o \
	mock_adaptors.o \
	mock_control.o \
	mock_device.o \
	mock_profile.o \
	mock_service.o \
	modem_info_unittest.o \
	modem_manager_unittest.o \
	modem_unittest.o \
	nice_mock_control.o \
	profile_unittest.o \
	property_accessor_unittest.o \
	property_store_unittest.o \
	resolver_unittest.o \
	routing_table_unittest.o \
	rtnl_handler_unittest.o \
	rtnl_listener_unittest.o \
	rtnl_message_unittest.o \
	service_unittest.o \
	shill_unittest.o \
	testrunner.o \
	wifi_service_unittest.o \
	wifi_unittest.o

.PHONY: all clean

all: $(SHILL_BIN) $(TEST_BIN)

$(DBUS_PROXY_BINDINGS): %.h: %.xml
	$(DBUSXX_XML2CPP) $< --proxy=$@

$(DBUS_ADAPTOR_BINDINGS): %.h: %.xml
	$(DBUSXX_XML2CPP) $< --adaptor=$@

.cc.o:
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(INCLUDE_DIRS) -c $< -o $@

$(SHILL_OBJS): $(DBUS_BINDINGS)

$(SHILL_BIN): $(SHILL_MAIN_OBJ) $(SHILL_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) $(LIB_DIRS) $(LDFLAGS) $^ $(LIBS) \
		-o $@

$(TEST_BIN): CXXFLAGS += -DUNIT_TEST
$(TEST_BIN): $(TEST_OBJS) $(SHILL_OBJS)
	$(CXX) $(CXXFLAGS) $(TEST_LIB_DIRS) $(LDFLAGS) $^ $(TEST_LIBS) -o $@

clean:
	rm -rf \
		*.o \
		$(CLEAN_FILES) \
		$(DBUS_BINDINGS) \
		$(SHILL_BIN) \
		$(TEST_BIN)
