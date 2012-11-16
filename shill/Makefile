# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

AR ?= ar
CC ?= gcc
CXX ?= g++
CFLAGS += \
	-Wall \
	-Werror \
	-Wextra \
	-Wno-missing-field-initializers \
	-Wno-unused-parameter \
	-Wno-unused-result \
	-Wuninitialized
CXXFLAGS ?= -fno-strict-aliasing
CXXFLAGS += $(CFLAGS) -Woverloaded-virtual
CXXFLAGS += $(EXTRA_CXXFLAGS)
CPPFLAGS ?= -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS
PKG_CONFIG ?= pkg-config
DBUSXX_XML2CPP = dbusxx-xml2cpp

BUILDDIR = build

LIBDIR = /usr/lib
SHIMDIR = $(LIBDIR)/shill/shims
CPPFLAGS += -DSHIMDIR=\"$(SHIMDIR)\"

BASE_VER = 125070
COMMON_PC_DEPS = libchrome-$(BASE_VER) libchromeos-$(BASE_VER)
SHILL_PC_DEPS = \
	$(COMMON_PC_DEPS) \
	dbus-c++-1 \
	gio-2.0 \
	glib-2.0 \
	ModemManager \
	libnl-3.0 \
	libnl-genl-3.0
NSS_GET_CERT_PC_DEPS = $(COMMON_PC_DEPS) nss
OPENVPN_SCRIPT_PC_DEPS = $(COMMON_PC_DEPS) dbus-c++-1
PPPD_PLUGIN_PC_DEPS = $(COMMON_PC_DEPS) dbus-c++-1
SET_APN_HELPER_PC_DEPS = dbus-1
INCLUDE_DIRS = \
	-iquote.. \
	-iquote $(BUILDDIR) \
	$(shell $(PKG_CONFIG) --cflags \
		$(NSS_GET_CERT_PC_DEPS) \
		$(OPENVPN_SCRIPT_PC_DEPS) \
		$(PPPD_PLUGIN_PC_DEPS) \
		$(SET_APN_HELPER_PC_DEPS) \
		$(SHILL_PC_DEPS))
SHILL_LIBS = \
	-lbootstat \
	-lcares \
	-lmobile-provider \
	-lmetrics \
	-lminijail \
	$(shell $(PKG_CONFIG) --libs $(SHILL_PC_DEPS))
NSS_GET_CERT_LIBS = $(shell $(PKG_CONFIG) --libs $(NSS_GET_CERT_PC_DEPS))
OPENVPN_SCRIPT_LIBS = $(shell $(PKG_CONFIG) --libs $(OPENVPN_SCRIPT_PC_DEPS))
PPPD_PLUGIN_LIBS = $(shell $(PKG_CONFIG) --libs $(PPPD_PLUGIN_PC_DEPS))
SET_APN_HELPER_LIBS = $(shell $(PKG_CONFIG) --libs $(SET_APN_HELPER_PC_DEPS))
TEST_LIBS = $(SHILL_LIBS) $(NSS_GET_CERT_LIBS) -lgmock -lgtest

DBUS_BINDINGS_DIR = dbus_bindings
BUILD_DBUS_BINDINGS_DIR = $(BUILDDIR)/shill/$(DBUS_BINDINGS_DIR)
BUILD_DBUS_BINDINGS_SHIMS_DIR = $(BUILD_DBUS_BINDINGS_DIR)/shims

BUILD_SHIMS_DIR = $(BUILDDIR)/shims

_CREATE_BUILDDIR := $(shell mkdir -p \
	$(BUILDDIR) \
	$(BUILD_DBUS_BINDINGS_DIR) \
	$(BUILD_DBUS_BINDINGS_SHIMS_DIR) \
	$(BUILD_SHIMS_DIR))

DBUS_ADAPTOR_HEADERS :=

DBUS_PROXY_HEADERS = \
	dbus-objectmanager.h \
	dbus-properties.h \
	dbus-service.h \
	dhcpcd.h \
	modem-gobi.h \
	power_manager.h \
	shims/flimflam-task.h \
	supplicant-bss.h \
	supplicant-interface.h \
	supplicant-network.h \
	supplicant-process.h

# Generates rules for copying SYSROOT XMLs locally and updates the proxy header
# dependencies.
DBUS_BINDINGS_XML_SYSROOT = \
	org.chromium.WiMaxManager>wimax_manager \
	org.chromium.WiMaxManager.Device>wimax_manager-device \
	org.chromium.WiMaxManager.Network>wimax_manager-network \
	org.freedesktop.ModemManager>modem_manager \
	org.freedesktop.ModemManager.Modem>modem \
	org.freedesktop.ModemManager.Modem.Cdma>modem-cdma \
	org.freedesktop.ModemManager.Modem.Gsm.Card>modem-gsm-card \
	org.freedesktop.ModemManager.Modem.Gsm.Network>modem-gsm-network \
	org.freedesktop.ModemManager.Modem.Simple>modem-simple \
	org.freedesktop.ModemManager1.Bearer>mm1-bearer \
	org.freedesktop.ModemManager1.Modem>mm1-modem \
	org.freedesktop.ModemManager1.Modem.Modem3gpp>mm1-modem-modem3gpp \
	org.freedesktop.ModemManager1.Modem.ModemCdma>mm1-modem-modemcdma \
	org.freedesktop.ModemManager1.Modem.Simple>mm1-modem-simple \
	org.freedesktop.ModemManager1.Sim>mm1-sim

# Rename local XML files with the names required by DBus to XML files with the
# names required by the style guide, which will then be turned into generated
# headers later.
DBUS_BINDINGS_XML_LOCAL = \
	org.chromium.flimflam.Device>flimflam-device \
	org.chromium.flimflam.IPConfig>flimflam-ipconfig \
	org.chromium.flimflam.Manager>flimflam-manager \
	org.chromium.flimflam.Profile>flimflam-profile \
	org.chromium.flimflam.Service>flimflam-service \
	org.chromium.flimflam.Task>flimflam-task

define ADD_BINDING
$(eval _SOURCE = $(word 1,$(subst >, ,$(1))))
$(eval _TARGET = $(word 2,$(subst >, ,$(1))))
DBUS_PROXY_HEADERS += $(_TARGET).h
$(BUILD_DBUS_BINDINGS_DIR)/$(_TARGET).xml: \
	$(SYSROOT)/usr/share/dbus-1/interfaces/$(_SOURCE).xml
	cat $$< > $$@
endef

define ADD_LOCAL_BINDING
$(eval _SOURCE = $(word 1,$(subst >, ,$(1))))
$(eval _TARGET = $(word 2,$(subst >, ,$(1))))
DBUS_ADAPTOR_HEADERS += $(_TARGET).h
$(BUILD_DBUS_BINDINGS_DIR)/$(_TARGET).xml: $(DBUS_BINDINGS_DIR)/$(_SOURCE).xml
	cp $$< $$@
endef

$(foreach b,$(DBUS_BINDINGS_XML_SYSROOT),$(eval $(call ADD_BINDING,$(b))))
$(foreach b,$(DBUS_BINDINGS_XML_LOCAL),$(eval $(call ADD_LOCAL_BINDING,$(b))))

DBUS_ADAPTOR_BINDINGS = \
	$(addprefix $(BUILD_DBUS_BINDINGS_DIR)/, $(DBUS_ADAPTOR_HEADERS))
DBUS_PROXY_BINDINGS = \
	$(addprefix $(BUILD_DBUS_BINDINGS_DIR)/, $(DBUS_PROXY_HEADERS))

SHILL_LIB = $(BUILDDIR)/shill.a
SHILL_OBJS = $(addprefix $(BUILDDIR)/, \
	arp_client.o \
	arp_packet.o \
	async_connection.o \
	byte_string.o \
	callback80211_metrics.o \
	callback80211_object.o \
	cellular.o \
	cellular_capability.o \
	cellular_capability_cdma.o \
	cellular_capability_classic.o \
	cellular_capability_gsm.o \
	cellular_capability_universal.o \
	cellular_error.o \
	cellular_operator_info.o \
	cellular_service.o \
	config80211.o \
	connection.o \
	crypto_des_cbc.o \
	crypto_provider.o \
	crypto_rot47.o \
	dbus_adaptor.o \
	dbus_control.o \
	dbus_manager.o \
	dbus_objectmanager_proxy.o \
	dbus_properties.o \
	dbus_properties_proxy.o \
	dbus_service_proxy.o \
	default_profile.o \
	device.o \
	device_dbus_adaptor.o \
	device_info.o \
	dhcp_config.o \
	dhcp_provider.o \
	dhcpcd_proxy.o \
	diagnostics_reporter.o \
	dns_client.o \
	endpoint.o \
	ephemeral_profile.o \
	error.o \
	ethernet.o \
	ethernet_service.o \
	event_dispatcher.o \
	geolocation_info.o \
	glib.o \
	glib_io_ready_handler.o \
	glib_io_input_handler.o \
	hook_table.o \
	http_proxy.o \
	http_request.o \
	http_url.o \
	ip_address.o \
	ipconfig.o \
	ipconfig_dbus_adaptor.o \
	kernel_bound_nlmessage.o \
	key_file_store.o \
	key_value_store.o \
	link_monitor.o \
	l2tp_ipsec_driver.o \
	manager.o \
	manager_dbus_adaptor.o \
	memory_log.o \
	metrics.o \
	minijail.o \
	mm1_bearer_proxy.o \
	mm1_modem_modem3gpp_proxy.o \
	mm1_modem_modemcdma_proxy.o \
	mm1_modem_proxy.o \
	mm1_modem_simple_proxy.o \
	mm1_sim_proxy.o \
	modem.o \
	modem_1.o \
	modem_cdma_proxy.o \
	modem_classic.o \
	modem_gobi_proxy.o \
	modem_gsm_card_proxy.o \
	modem_gsm_network_proxy.o \
	modem_info.o \
	modem_manager.o \
	modem_manager_1.o \
	modem_manager_proxy.o \
	modem_proxy.o \
	modem_simple_proxy.o \
	netlink_socket.o \
	nl80211_socket.o \
	nss.o \
	openvpn_driver.o \
	openvpn_management_server.o \
	portal_detector.o \
	power_manager.o \
	power_manager_proxy.o \
	process_killer.o \
	profile.o \
	profile_dbus_adaptor.o \
	profile_dbus_property_exporter.o \
	property_store.o \
	proxy_factory.o \
	routing_table.o \
	resolver.o \
	rpc_task.o \
	rpc_task_dbus_adaptor.o \
	rtnl_handler.o \
	rtnl_listener.o \
	rtnl_message.o \
	scope_logger.o \
	service.o \
	service_dbus_adaptor.o \
	shill_ares.o \
	shill_config.o \
	shill_daemon.o \
	shill_test_config.o \
	shill_time.o \
	sockets.o \
	static_ip_parameters.o \
	supplicant_bss_proxy.o \
	supplicant_interface_proxy.o \
	supplicant_process_proxy.o \
	technology.o \
	user_bound_nlmessage.o \
	virtio_ethernet.o \
	vpn.o \
	vpn_driver.o \
	vpn_provider.o \
	vpn_service.o \
	wifi.o \
	wifi_endpoint.o \
	wifi_service.o \
	wimax.o \
	wimax_device_proxy.o \
	wimax_manager_proxy.o \
	wimax_network_proxy.o \
	wimax_provider.o \
	wimax_service.o \
	wpa_supplicant.o \
	)

SHILL_BIN = shill
# Broken out separately, because (unlike other SHILL_OBJS), it
# shouldn't be linked into TEST_BIN.
SHILL_MAIN_OBJ = $(BUILDDIR)/shill_main.o

TEST_BIN = shill_unittest
TEST_OBJS = $(addprefix $(BUILDDIR)/, \
	arp_client_unittest.o \
	arp_packet_unittest.o \
	async_connection_unittest.o \
	byte_string_unittest.o \
	cellular_capability_cdma_unittest.o \
	cellular_capability_classic_unittest.o \
	cellular_capability_gsm_unittest.o \
	cellular_capability_universal_unittest.o \
	cellular_operator_info_unittest.o \
	cellular_service_unittest.o \
	cellular_unittest.o \
	crypto_des_cbc_unittest.o \
	crypto_provider_unittest.o \
	crypto_rot47_unittest.o \
	config80211_unittest.o \
	connection_unittest.o \
	dbus_adaptor_unittest.o \
	dbus_manager_unittest.o \
	dbus_properties_unittest.o \
	default_profile_unittest.o \
	device_info_unittest.o \
	device_unittest.o \
	dhcp_config_unittest.o \
	dhcp_provider_unittest.o \
	diagnostics_reporter_unittest.o \
	dns_client_unittest.o \
	error_unittest.o \
	ethernet_service_unittest.o \
	hook_table_unittest.o \
	http_proxy_unittest.o \
	http_request_unittest.o \
	http_url_unittest.o \
	ip_address_unittest.o \
	ipconfig_unittest.o \
	key_file_store_unittest.o \
	key_value_store_unittest.o \
	l2tp_ipsec_driver_unittest.o \
	link_monitor_unittest.o \
	manager_unittest.o \
	memory_log_unittest.o \
	metrics_unittest.o \
	mock_adaptors.o \
	mock_ares.o \
	mock_arp_client.o \
	mock_async_connection.o \
	mock_cellular.o \
	mock_cellular_operator_info.o \
	mock_cellular_service.o \
	mock_connection.o \
	mock_control.o \
	mock_dbus_manager.o \
	mock_dbus_objectmanager_proxy.o \
	mock_dbus_properties_proxy.o \
	mock_dbus_service_proxy.o \
	mock_device.o \
	mock_device_info.o \
	mock_dhcp_config.o \
	mock_dhcp_provider.o \
	mock_dhcp_proxy.o \
	mock_dns_client.o \
	mock_ethernet.o \
	mock_event_dispatcher.o \
	mock_glib.o \
	mock_http_request.o \
	mock_ipconfig.o \
	mock_link_monitor.o \
	mock_log.o \
	mock_log_unittest.o \
	mock_manager.o \
	mock_metrics.o \
	mock_minijail.o \
	mock_mm1_bearer_proxy.o \
	mock_mm1_modem_modemcdma_proxy.o \
	mock_mm1_modem_modem3gpp_proxy.o \
	mock_mm1_modem_proxy.o \
	mock_mm1_modem_simple_proxy.o \
	mock_mm1_sim_proxy.o \
	mock_modem.o \
	mock_modem_cdma_proxy.o \
	mock_modem_gobi_proxy.o \
	mock_modem_gsm_card_proxy.o \
	mock_modem_gsm_network_proxy.o \
	mock_modem_info.o \
	mock_modem_manager_proxy.o \
	mock_modem_proxy.o \
	mock_modem_simple_proxy.o \
	mock_nss.o \
	mock_openvpn_driver.o \
	mock_openvpn_management_server.o \
	mock_portal_detector.o \
	mock_power_manager.o \
	mock_power_manager_proxy.o \
	mock_process_killer.o \
	mock_profile.o \
	mock_property_store.o \
	mock_proxy_factory.o \
	mock_resolver.o \
	mock_routing_table.o \
	mock_rtnl_handler.o \
	mock_service.o \
	mock_sockets.o \
	mock_store.o \
	mock_supplicant_bss_proxy.o \
	mock_supplicant_interface_proxy.o \
	mock_supplicant_process_proxy.o \
	mock_time.o \
	mock_vpn.o \
	mock_vpn_driver.o \
	mock_vpn_provider.o \
	mock_vpn_service.o \
	mock_wifi.o \
	mock_wifi_service.o \
	mock_wimax.o \
	mock_wimax_device_proxy.o \
	mock_wimax_manager_proxy.o \
	mock_wimax_network_proxy.o \
	mock_wimax_provider.o \
	mock_wimax_service.o \
	modem_1_unittest.o \
	modem_info_unittest.o \
	modem_manager_unittest.o \
	modem_unittest.o \
	nice_mock_control.o \
	nss_unittest.o \
	openvpn_driver_unittest.o \
	openvpn_management_server_unittest.o \
	portal_detector_unittest.o \
	power_manager_unittest.o \
	process_killer_unittest.o \
	profile_dbus_property_exporter_unittest.o \
	profile_unittest.o \
	property_accessor_unittest.o \
	property_store_inspector.o \
	property_store_unittest.o \
	resolver_unittest.o \
	routing_table_unittest.o \
	rpc_task_unittest.o \
	rtnl_handler_unittest.o \
	rtnl_listener_unittest.o \
	rtnl_message_unittest.o \
	scope_logger_unittest.o \
	service_under_test.o \
	service_unittest.o \
	shill_unittest.o \
	shims/certificates_unittest.o \
	shims/environment_unittest.o \
	static_ip_parameters_unittest.o \
	technology_unittest.o \
	testrunner.o \
	vpn_driver_unittest.o \
	vpn_provider_unittest.o \
	vpn_service_unittest.o \
	vpn_unittest.o \
	wifi_endpoint_unittest.o \
	wifi_service_unittest.o \
	wifi_unittest.o \
	wimax_provider_unittest.o \
	wimax_service_unittest.o \
	wimax_unittest.o \
	)

NSS_GET_CERT_OBJS = $(BUILD_SHIMS_DIR)/certificates.o
NSS_GET_CERT_MAIN_OBJ = $(BUILD_SHIMS_DIR)/nss_get_cert.o
NSS_GET_CERT_BIN = $(BUILD_SHIMS_DIR)/nss-get-cert

OPENVPN_SCRIPT_OBJS = $(addprefix $(BUILD_SHIMS_DIR)/, \
	environment.o \
	task_proxy.o \
	)
OPENVPN_SCRIPT_MAIN_OBJ = $(BUILD_SHIMS_DIR)/openvpn_script.o
OPENVPN_SCRIPT_BIN = $(BUILD_SHIMS_DIR)/openvpn-script

PPPD_PLUGIN_OBJS = $(addprefix $(BUILD_SHIMS_DIR)/, \
	c_ppp.pic.o \
	environment.pic.o \
	ppp.pic.o \
	pppd_plugin.pic.o \
	task_proxy.pic.o \
	)
PPPD_PLUGIN_SO = $(BUILD_SHIMS_DIR)/shill-pppd-plugin.so

SET_APN_HELPER_MAIN_OBJ = $(BUILD_SHIMS_DIR)/set_apn_helper.o
SET_APN_HELPER_BIN = $(BUILD_SHIMS_DIR)/set-apn-helper

WPA_SUPPLICANT_CONF = $(BUILD_SHIMS_DIR)/wpa_supplicant.conf

OBJS = \
	$(NSS_GET_CERT_MAIN_OBJ) \
	$(NSS_GET_CERT_OBJS) \
	$(OPENVPN_SCRIPT_MAIN_OBJ) \
	$(OPENVPN_SCRIPT_OBJS) \
	$(PPPD_PLUGIN_OBJS) \
	$(SET_APN_HELPER_MAIN_OBJ) \
	$(SHILL_MAIN_OBJ) \
	$(SHILL_OBJS) \
	$(TEST_OBJS)

.PHONY: all clean shims

all: $(SHILL_BIN) $(TEST_BIN) shims

shims: \
	$(NSS_GET_CERT_BIN) \
	$(OPENVPN_SCRIPT_BIN) \
	$(PPPD_PLUGIN_SO) \
	$(SET_APN_HELPER_BIN) \
	$(WPA_SUPPLICANT_CONF)

$(BUILD_DBUS_BINDINGS_SHIMS_DIR)/flimflam-task.xml: \
	$(BUILD_DBUS_BINDINGS_DIR)/flimflam-task.xml
	cp $< $@

$(BUILD_DBUS_BINDINGS_DIR)/%.xml: $(DBUS_BINDINGS_DIR)/%.xml
	cp $< $@

$(DBUS_PROXY_BINDINGS): %.h: %.xml
	$(DBUSXX_XML2CPP) $< --proxy=$@ --sync --async

$(DBUS_ADAPTOR_BINDINGS): %.h: %.xml
	$(DBUSXX_XML2CPP) $< --adaptor=$@

$(BUILDDIR)/%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDE_DIRS) -MMD -c $< -o $@

$(BUILDDIR)/%.o: %.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(INCLUDE_DIRS) -MMD -c $< -o $@

$(BUILDDIR)/%.pic.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC $(INCLUDE_DIRS) -MMD -c $< -o $@

$(BUILDDIR)/%.pic.o: %.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -fPIC $(INCLUDE_DIRS) -MMD -c $< -o $@

$(SHILL_OBJS): $(DBUS_ADAPTOR_BINDINGS) $(DBUS_PROXY_BINDINGS)

$(SHILL_LIB): $(SHILL_OBJS)
	$(AR) rcs $@ $^

$(SHILL_BIN): $(SHILL_MAIN_OBJ) $(SHILL_LIB)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(SHILL_LIBS) -o $@

$(NSS_GET_CERT_BIN): $(NSS_GET_CERT_MAIN_OBJ) $(NSS_GET_CERT_OBJS) $(SHILL_LIB)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(NSS_GET_CERT_LIBS) -o $@

$(OPENVPN_SCRIPT_OBJS): $(DBUS_PROXY_BINDINGS)
$(OPENVPN_SCRIPT_MAIN_OBJ): $(DBUS_PROXY_BINDINGS)

$(OPENVPN_SCRIPT_BIN): $(OPENVPN_SCRIPT_MAIN_OBJ) $(OPENVPN_SCRIPT_OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(OPENVPN_SCRIPT_LIBS) -o $@

$(PPPD_PLUGIN_OBJS): $(DBUS_PROXY_BINDINGS)

$(PPPD_PLUGIN_SO): $(PPPD_PLUGIN_OBJS)
	$(CXX) $(LDFLAGS) -shared $^ $(PPPD_PLUGIN_LIBS) -o $@

$(SET_APN_HELPER_BIN): $(SET_APN_HELPER_MAIN_OBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(SET_APN_HELPER_LIBS) -o $@

$(WPA_SUPPLICANT_CONF): shims/wpa_supplicant.conf.in
	sed s,@libdir@,$(LIBDIR), $^ > $@

$(TEST_BIN): CPPFLAGS += -DUNIT_TEST -DSYSROOT=\"$(SYSROOT)\"
$(TEST_BIN): \
	$(TEST_OBJS) \
	$(NSS_GET_CERT_OBJS) \
	$(OPENVPN_SCRIPT_OBJS) \
	$(SHILL_LIB)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(TEST_LIBS) -o $@

clean:
	rm -rf \
		$(BUILDDIR) \
		$(SHILL_BIN) \
		$(TEST_BIN)

-include $(OBJS:.o=.d)
