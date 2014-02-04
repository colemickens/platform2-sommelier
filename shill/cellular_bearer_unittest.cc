// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_bearer.h"

#include <ModemManager/ModemManager.h>

#include "shill/mock_dbus_properties_proxy.h"
#include "shill/mock_proxy_factory.h"
#include "shill/testing.h"

using std::string;
using std::vector;
using testing::Return;
using testing::ReturnNull;
using testing::_;

namespace shill {

namespace {

const char kBearerDBusPath[] = "/org/freedesktop/ModemManager/Bearer/0";
const char kBearerDBusService[] = "org.freedesktop.ModemManager";
const char kDataInterface[] = "/dev/ppp0";
const char kIPv4Address[] = "10.0.0.1";
const char kIPv4Gateway[] = "10.0.0.254";
const int kIPv4SubnetPrefix = 8;
const char *const kIPv4DNS[] = { "10.0.0.2", "8.8.4.4", "8.8.8.8" };
const char kIPv6Address[] = "0:0:0:0:0:ffff:a00:1";
const char kIPv6Gateway[] = "0:0:0:0:0:ffff:a00:fe";
const int kIPv6SubnetPrefix = 16;
const char *const kIPv6DNS[] = {
  "0:0:0:0:0:ffff:a00:fe", "0:0:0:0:0:ffff:808:404", "0:0:0:0:0:ffff:808:808"
};

}  // namespace

class CellularBearerTest : public testing::Test {
 public:
  CellularBearerTest()
      : proxy_factory_(new MockProxyFactory()),
        bearer_(proxy_factory_.get(), kBearerDBusPath, kBearerDBusService) {}

 protected:
  void VerifyDefaultProperties() {
    EXPECT_EQ(kBearerDBusPath, bearer_.dbus_path());
    EXPECT_EQ(kBearerDBusService, bearer_.dbus_service());
    EXPECT_FALSE(bearer_.connected());
    EXPECT_EQ("", bearer_.data_interface());
    EXPECT_EQ(IPConfig::kMethodUnknown, bearer_.ipv4_config_method());
    EXPECT_TRUE(bearer_.ipv4_config_properties() == NULL);
    EXPECT_EQ(IPConfig::kMethodUnknown, bearer_.ipv6_config_method());
    EXPECT_TRUE(bearer_.ipv6_config_properties() == NULL);
  }

  static DBusPropertiesMap ConstructIPv4ConfigProperties(
      MMBearerIpMethod ipconfig_method) {
    DBusPropertiesMap ipconfig_properties;
    ipconfig_properties["method"].writer().append_uint32(ipconfig_method);
    if (ipconfig_method == MM_BEARER_IP_METHOD_STATIC) {
      ipconfig_properties["address"].writer().append_string(kIPv4Address);
      ipconfig_properties["gateway"].writer().append_string(kIPv4Gateway);
      ipconfig_properties["prefix"].writer().append_uint32(kIPv4SubnetPrefix);
      ipconfig_properties["dns1"].writer().append_string(kIPv4DNS[0]);
      ipconfig_properties["dns2"].writer().append_string(kIPv4DNS[1]);
      ipconfig_properties["dns3"].writer().append_string(kIPv4DNS[2]);
    }
    return ipconfig_properties;
  }

  static DBusPropertiesMap ConstructIPv6ConfigProperties(
      MMBearerIpMethod ipconfig_method) {
    DBusPropertiesMap ipconfig_properties;
    ipconfig_properties["method"].writer().append_uint32(ipconfig_method);
    if (ipconfig_method == MM_BEARER_IP_METHOD_STATIC) {
      ipconfig_properties["address"].writer().append_string(kIPv6Address);
      ipconfig_properties["gateway"].writer().append_string(kIPv6Gateway);
      ipconfig_properties["prefix"].writer().append_uint32(kIPv6SubnetPrefix);
      ipconfig_properties["dns1"].writer().append_string(kIPv6DNS[0]);
      ipconfig_properties["dns2"].writer().append_string(kIPv6DNS[1]);
      ipconfig_properties["dns3"].writer().append_string(kIPv6DNS[2]);
    }
    return ipconfig_properties;
  }

  static DBusPropertiesMap ConstructBearerProperties(
      bool connected, const string &data_interface,
      MMBearerIpMethod ipv4_config_method,
      MMBearerIpMethod ipv6_config_method) {
    DBusPropertiesMap properties;
    properties[MM_BEARER_PROPERTY_CONNECTED].writer().append_bool(connected);
    properties[MM_BEARER_PROPERTY_INTERFACE].writer().append_string(
        data_interface.c_str());

    DBus::MessageIter writer;
    writer = properties[MM_BEARER_PROPERTY_IP4CONFIG].writer();
    writer << ConstructIPv4ConfigProperties(ipv4_config_method);
    writer = properties[MM_BEARER_PROPERTY_IP6CONFIG].writer();
    writer << ConstructIPv6ConfigProperties(ipv6_config_method);
    return properties;
  }

  void VerifyStaticIPv4ConfigMethodAndProperties() {
    EXPECT_EQ(IPConfig::kMethodStatic, bearer_.ipv4_config_method());
    const IPConfig::Properties *ipv4_config_properties =
        bearer_.ipv4_config_properties();
    ASSERT_TRUE(ipv4_config_properties != NULL);
    EXPECT_EQ(IPAddress::kFamilyIPv4, ipv4_config_properties->address_family);
    EXPECT_EQ(kIPv4Address, ipv4_config_properties->address);
    EXPECT_EQ(kIPv4Gateway, ipv4_config_properties->gateway);
    EXPECT_EQ(kIPv4SubnetPrefix, ipv4_config_properties->subnet_prefix);
    ASSERT_EQ(3, ipv4_config_properties->dns_servers.size());
    EXPECT_EQ(kIPv4DNS[0], ipv4_config_properties->dns_servers[0]);
    EXPECT_EQ(kIPv4DNS[1], ipv4_config_properties->dns_servers[1]);
    EXPECT_EQ(kIPv4DNS[2], ipv4_config_properties->dns_servers[2]);
  }

  void VerifyStaticIPv6ConfigMethodAndProperties() {
    EXPECT_EQ(IPConfig::kMethodStatic, bearer_.ipv6_config_method());
    const IPConfig::Properties *ipv6_config_properties =
        bearer_.ipv6_config_properties();
    ASSERT_TRUE(ipv6_config_properties != NULL);
    EXPECT_EQ(IPAddress::kFamilyIPv6, ipv6_config_properties->address_family);
    EXPECT_EQ(kIPv6Address, ipv6_config_properties->address);
    EXPECT_EQ(kIPv6Gateway, ipv6_config_properties->gateway);
    EXPECT_EQ(kIPv6SubnetPrefix, ipv6_config_properties->subnet_prefix);
    ASSERT_EQ(3, ipv6_config_properties->dns_servers.size());
    EXPECT_EQ(kIPv6DNS[0], ipv6_config_properties->dns_servers[0]);
    EXPECT_EQ(kIPv6DNS[1], ipv6_config_properties->dns_servers[1]);
    EXPECT_EQ(kIPv6DNS[2], ipv6_config_properties->dns_servers[2]);
  }

  scoped_ptr<MockProxyFactory> proxy_factory_;
  CellularBearer bearer_;
};

TEST_F(CellularBearerTest, Constructor) {
  VerifyDefaultProperties();
}

TEST_F(CellularBearerTest, Init) {
  // Ownership of |properties_proxy| is transferred to |bearer_| via
  // |proxy_factory_|.
  scoped_ptr<MockDBusPropertiesProxy> properties_proxy(
      new MockDBusPropertiesProxy);
  EXPECT_CALL(*proxy_factory_.get(),
              CreateDBusPropertiesProxy(kBearerDBusPath, kBearerDBusService))
      .WillOnce(ReturnAndReleasePointee(&properties_proxy));
  EXPECT_CALL(*properties_proxy.get(), set_properties_changed_callback(_))
      .Times(1);
  EXPECT_CALL(*properties_proxy.get(), GetAll(MM_DBUS_INTERFACE_BEARER))
      .WillOnce(Return(ConstructBearerProperties(true, kDataInterface,
                                                 MM_BEARER_IP_METHOD_STATIC,
                                                 MM_BEARER_IP_METHOD_STATIC)));
  bearer_.Init();
  EXPECT_TRUE(bearer_.connected());
  EXPECT_EQ(kDataInterface, bearer_.data_interface());
  VerifyStaticIPv4ConfigMethodAndProperties();
  VerifyStaticIPv6ConfigMethodAndProperties();
}

TEST_F(CellularBearerTest, InitAndCreateDBusPropertiesProxyFails) {
  EXPECT_CALL(*proxy_factory_.get(),
              CreateDBusPropertiesProxy(kBearerDBusPath, kBearerDBusService))
      .WillOnce(ReturnNull());
  bearer_.Init();
  VerifyDefaultProperties();
}

TEST_F(CellularBearerTest, OnDBusPropertiesChanged) {
  DBusPropertiesMap properties;

  // If interface is not MM_DBUS_INTERFACE_BEARER, no updates should be done.
  bearer_.OnDBusPropertiesChanged("", properties, vector<string>());
  VerifyDefaultProperties();

  properties[MM_BEARER_PROPERTY_CONNECTED].writer().append_bool(true);
  bearer_.OnDBusPropertiesChanged("", properties, vector<string>());
  VerifyDefaultProperties();

  // Update 'interface' property.
  properties.clear();
  properties[MM_BEARER_PROPERTY_INTERFACE].writer().append_string(
      kDataInterface);
  bearer_.OnDBusPropertiesChanged(MM_DBUS_INTERFACE_BEARER, properties,
                                  vector<string>());
  EXPECT_EQ(kDataInterface, bearer_.data_interface());

  // Update 'connected' property.
  properties.clear();
  properties[MM_BEARER_PROPERTY_CONNECTED].writer().append_bool(true);
  bearer_.OnDBusPropertiesChanged(MM_DBUS_INTERFACE_BEARER, properties,
                                  vector<string>());
  EXPECT_TRUE(bearer_.connected());
  // 'interface' property remains unchanged.
  EXPECT_EQ(kDataInterface, bearer_.data_interface());

  DBus::MessageIter writer;

  // Update 'ip4config' property.
  properties.clear();
  writer = properties[MM_BEARER_PROPERTY_IP4CONFIG].writer();
  writer << ConstructIPv4ConfigProperties(MM_BEARER_IP_METHOD_UNKNOWN);
  bearer_.OnDBusPropertiesChanged(MM_DBUS_INTERFACE_BEARER, properties,
                                  vector<string>());
  EXPECT_EQ(IPConfig::kMethodUnknown, bearer_.ipv4_config_method());

  properties.clear();
  writer = properties[MM_BEARER_PROPERTY_IP4CONFIG].writer();
  writer << ConstructIPv4ConfigProperties(MM_BEARER_IP_METHOD_PPP);
  bearer_.OnDBusPropertiesChanged(MM_DBUS_INTERFACE_BEARER, properties,
                                  vector<string>());
  EXPECT_EQ(IPConfig::kMethodPPP, bearer_.ipv4_config_method());

  properties.clear();
  writer = properties[MM_BEARER_PROPERTY_IP4CONFIG].writer();
  writer << ConstructIPv4ConfigProperties(MM_BEARER_IP_METHOD_STATIC);
  bearer_.OnDBusPropertiesChanged(MM_DBUS_INTERFACE_BEARER, properties,
                                  vector<string>());
  EXPECT_EQ(IPConfig::kMethodStatic, bearer_.ipv4_config_method());
  VerifyStaticIPv4ConfigMethodAndProperties();

  properties.clear();
  writer = properties[MM_BEARER_PROPERTY_IP4CONFIG].writer();
  writer << ConstructIPv4ConfigProperties(MM_BEARER_IP_METHOD_DHCP);
  bearer_.OnDBusPropertiesChanged(MM_DBUS_INTERFACE_BEARER, properties,
                                  vector<string>());
  EXPECT_EQ(IPConfig::kMethodDHCP, bearer_.ipv4_config_method());

  // Update 'ip6config' property.
  properties.clear();
  writer = properties[MM_BEARER_PROPERTY_IP6CONFIG].writer();
  writer << ConstructIPv6ConfigProperties(MM_BEARER_IP_METHOD_UNKNOWN);
  bearer_.OnDBusPropertiesChanged(MM_DBUS_INTERFACE_BEARER, properties,
                                  vector<string>());
  EXPECT_EQ(IPConfig::kMethodUnknown, bearer_.ipv6_config_method());

  properties.clear();
  writer = properties[MM_BEARER_PROPERTY_IP6CONFIG].writer();
  writer << ConstructIPv6ConfigProperties(MM_BEARER_IP_METHOD_PPP);
  bearer_.OnDBusPropertiesChanged(MM_DBUS_INTERFACE_BEARER, properties,
                                  vector<string>());
  EXPECT_EQ(IPConfig::kMethodPPP, bearer_.ipv6_config_method());

  properties.clear();
  writer = properties[MM_BEARER_PROPERTY_IP6CONFIG].writer();
  writer << ConstructIPv6ConfigProperties(MM_BEARER_IP_METHOD_STATIC);
  bearer_.OnDBusPropertiesChanged(MM_DBUS_INTERFACE_BEARER, properties,
                                  vector<string>());
  EXPECT_EQ(IPConfig::kMethodStatic, bearer_.ipv6_config_method());
  VerifyStaticIPv6ConfigMethodAndProperties();

  properties.clear();
  writer = properties[MM_BEARER_PROPERTY_IP6CONFIG].writer();
  writer << ConstructIPv6ConfigProperties(MM_BEARER_IP_METHOD_DHCP);
  bearer_.OnDBusPropertiesChanged(MM_DBUS_INTERFACE_BEARER, properties,
                                  vector<string>());
  EXPECT_EQ(IPConfig::kMethodDHCP, bearer_.ipv6_config_method());
}

}  // namespace shill
