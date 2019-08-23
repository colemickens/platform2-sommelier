// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/static_ip_parameters.h"

#include <base/strings/string_number_conversions.h>
#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>

#include "shill/ipconfig.h"
#include "shill/mock_store.h"
#include "shill/property_store.h"

using std::string;
using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SetArgPointee;
using testing::StrictMock;
using testing::Test;

namespace shill {

namespace {

const char kAddress[] = "10.0.0.1";
const char kGateway[] = "10.0.0.254";
const int32_t kMtu = 512;

const char kNameServer0[] = "10.0.1.253";
const char kNameServer1[] = "10.0.1.252";
const char kNameServers[] = "10.0.1.253,10.0.1.252";

const char kSearchDomains[] = "example.com,chromium.org";
const char kSearchDomain0[] = "example.com";
const char kSearchDomain1[] = "chromium.org";

const char kPeerAddress[] = "10.0.0.2";
const int32_t kPrefixLen = 24;

const char kExcludedRoutes[] = "192.168.1.0/24,192.168.2.0/24";
const char kExcludedRoute0[] = "192.168.1.0/24";
const char kExcludedRoute1[] = "192.168.2.0/24";

const char kIncludedRoutes[] = "0.0.0.0/0";
const IPConfig::Route kIncludedRoute0("0.0.0.0", 0, "10.0.0.254");

}  // namespace

class StaticIPParametersTest : public Test {
 public:
  StaticIPParametersTest() = default;

  void ExpectEmptyIPConfig() {
    EXPECT_TRUE(ipconfig_props_.address.empty());
    EXPECT_TRUE(ipconfig_props_.gateway.empty());
    EXPECT_EQ(IPConfig::kUndefinedMTU, ipconfig_props_.mtu);
    EXPECT_TRUE(ipconfig_props_.dns_servers.empty());
    EXPECT_TRUE(ipconfig_props_.domain_search.empty());
    EXPECT_TRUE(ipconfig_props_.peer_address.empty());
    EXPECT_FALSE(ipconfig_props_.subnet_prefix);
    EXPECT_TRUE(ipconfig_props_.exclusion_list.empty());
    EXPECT_TRUE(ipconfig_props_.routes.empty());
  }
  // Modify an IP address string in some predictable way.  There's no need
  // for the output string to be valid from a networking perspective.
  string VersionedAddress(const string& address, int version) {
    string returned_address = address;
    CHECK(returned_address.length());
    returned_address[returned_address.length() - 1] += version;
    return returned_address;
  }
  void ExpectPopulatedIPConfigWithVersion(int version) {
    EXPECT_EQ(VersionedAddress(kAddress, version), ipconfig_props_.address);
    EXPECT_EQ(VersionedAddress(kGateway, version), ipconfig_props_.gateway);
    EXPECT_EQ(kMtu + version, ipconfig_props_.mtu);

    EXPECT_EQ(2, ipconfig_props_.dns_servers.size());
    EXPECT_EQ(VersionedAddress(kNameServer0, version),
              ipconfig_props_.dns_servers[0]);
    EXPECT_EQ(VersionedAddress(kNameServer1, version),
              ipconfig_props_.dns_servers[1]);

    // VersionedAddress() increments the final character of each domain
    // name.
    EXPECT_EQ(2, ipconfig_props_.domain_search.size());
    EXPECT_EQ(VersionedAddress(kSearchDomain0, version),
              ipconfig_props_.domain_search[0]);
    EXPECT_EQ(VersionedAddress(kSearchDomain1, version),
              ipconfig_props_.domain_search[1]);

    EXPECT_EQ(VersionedAddress(kPeerAddress, version),
              ipconfig_props_.peer_address);
    EXPECT_EQ(kPrefixLen + version, ipconfig_props_.subnet_prefix);

    EXPECT_EQ(2, ipconfig_props_.exclusion_list.size());
    EXPECT_EQ(VersionedAddress(kExcludedRoute0, version),
              ipconfig_props_.exclusion_list[0]);
    EXPECT_EQ(VersionedAddress(kExcludedRoute1, version),
              ipconfig_props_.exclusion_list[1]);

    // VersionedAddress() increments the final digit of the prefix on
    // the IncludedRoutes property, and the final digit of the IP address
    // on the Gateway property.
    EXPECT_EQ(1, ipconfig_props_.routes.size());
    EXPECT_EQ(kIncludedRoute0.host, ipconfig_props_.routes[0].host);
    EXPECT_EQ(kIncludedRoute0.prefix + version,
              ipconfig_props_.routes[0].prefix);
    EXPECT_EQ(VersionedAddress(kIncludedRoute0.gateway, version),
              ipconfig_props_.routes[0].gateway);
  }
  void ExpectPopulatedIPConfig() { ExpectPopulatedIPConfigWithVersion(0); }
  void ExpectPropertiesWithVersion(PropertyStore* store,
                                   const string& property_prefix,
                                   int version) {
    string string_value;
    Error unused_error;
    EXPECT_TRUE(store->GetStringProperty(property_prefix + ".Address",
                                         &string_value, &unused_error));
    EXPECT_EQ(VersionedAddress(kAddress, version), string_value);
    EXPECT_TRUE(store->GetStringProperty(property_prefix + ".Gateway",
                                         &string_value, &unused_error));
    EXPECT_EQ(VersionedAddress(kGateway, version), string_value);
    int32_t int_value;
    EXPECT_TRUE(store->GetInt32Property(property_prefix + ".Mtu", &int_value,
                                        &unused_error));
    EXPECT_EQ(kMtu + version, int_value);
    EXPECT_TRUE(store->GetStringProperty(property_prefix + ".NameServers",
                                         &string_value, &unused_error));
    EXPECT_EQ(VersionedAddress(kNameServer0, version) + "," +
                  VersionedAddress(kNameServer1, version),
              string_value);
    EXPECT_TRUE(store->GetStringProperty(property_prefix + ".SearchDomains",
                                         &string_value, &unused_error));
    EXPECT_EQ(VersionedAddress(kSearchDomain0, version) + "," +
                  VersionedAddress(kSearchDomain1, version),
              string_value);
    EXPECT_TRUE(store->GetStringProperty(property_prefix + ".PeerAddress",
                                         &string_value, &unused_error));
    EXPECT_EQ(VersionedAddress(kPeerAddress, version), string_value);
    EXPECT_TRUE(store->GetInt32Property(property_prefix + ".Prefixlen",
                                        &int_value, &unused_error));
    EXPECT_EQ(kPrefixLen + version, int_value);
    EXPECT_TRUE(store->GetStringProperty(property_prefix + ".ExcludedRoutes",
                                         &string_value, &unused_error));
    EXPECT_EQ(VersionedAddress(kExcludedRoute0, version) + "," +
                  VersionedAddress(kExcludedRoute1, version),
              string_value);
    EXPECT_TRUE(store->GetStringProperty(property_prefix + ".IncludedRoutes",
                                         &string_value, &unused_error));
    EXPECT_EQ(VersionedAddress(kIncludedRoutes, version), string_value);
  }
  void ExpectProperties(PropertyStore* store, const string& property_prefix) {
    ExpectPropertiesWithVersion(store, property_prefix, 0);
  }
  void PopulateIPConfig() {
    ipconfig_props_.address = kAddress;
    ipconfig_props_.gateway = kGateway;
    ipconfig_props_.mtu = kMtu;
    ipconfig_props_.dns_servers = {kNameServer0, kNameServer1};
    ipconfig_props_.domain_search = {kSearchDomain0, kSearchDomain1};
    ipconfig_props_.peer_address = kPeerAddress;
    ipconfig_props_.subnet_prefix = kPrefixLen;
    ipconfig_props_.exclusion_list = {kExcludedRoute0, kExcludedRoute1};
    ipconfig_props_.routes = {kIncludedRoute0};
  }
  void SetStaticPropertiesWithVersion(PropertyStore* store, int version) {
    Error error;
    store->SetStringProperty("StaticIP.Address",
                             VersionedAddress(kAddress, version), &error);
    store->SetStringProperty("StaticIP.Gateway",
                             VersionedAddress(kGateway, version), &error);
    store->SetInt32Property("StaticIP.Mtu", kMtu + version, &error);
    store->SetStringProperty("StaticIP.NameServers",
                             VersionedAddress(kNameServer0, version) + "," +
                                 VersionedAddress(kNameServer1, version),
                             &error);
    store->SetStringProperty("StaticIP.SearchDomains",
                             VersionedAddress(kSearchDomain0, version) + "," +
                                 VersionedAddress(kSearchDomain1, version),
                             &error);
    store->SetStringProperty("StaticIP.PeerAddress",
                             VersionedAddress(kPeerAddress, version), &error);
    store->SetInt32Property("StaticIP.Prefixlen", kPrefixLen + version, &error);
    store->SetStringProperty("StaticIP.ExcludedRoutes",
                             VersionedAddress(kExcludedRoute0, version) + "," +
                                 VersionedAddress(kExcludedRoute1, version),
                             &error);
    store->SetStringProperty("StaticIP.IncludedRoutes",
                             VersionedAddress(kIncludedRoutes, version),
                             &error);
  }
  void SetStaticProperties(PropertyStore* store) {
    SetStaticPropertiesWithVersion(store, 0);
  }
  void SetStaticDictPropertiesWithVersion(PropertyStore* store, int version) {
    KeyValueStore args;
    args.SetString(kAddressProperty, VersionedAddress(kAddress, version));
    args.SetString(kGatewayProperty, VersionedAddress(kGateway, version));
    args.SetInt(kMtuProperty, kMtu + version);
    args.SetStrings(kNameServersProperty,
                    {VersionedAddress(kNameServer0, version),
                     VersionedAddress(kNameServer1, version)});
    args.SetStrings(kSearchDomainsProperty,
                    {VersionedAddress(kSearchDomain0, version),
                     VersionedAddress(kSearchDomain1, version)});
    args.SetString(kPeerAddressProperty,
                   VersionedAddress(kPeerAddress, version));
    args.SetInt(kPrefixlenProperty, kPrefixLen + version);
    args.SetStrings(kExcludedRoutesProperty,
                    {VersionedAddress(kExcludedRoute0, version),
                     VersionedAddress(kExcludedRoute1, version)});
    args.SetStrings(kIncludedRoutesProperty,
                    {VersionedAddress(kIncludedRoutes, version)});

    Error error;
    store->SetKeyValueStoreProperty(kStaticIPConfigProperty, args, &error);
  }

 protected:
  StaticIPParameters static_params_;
  IPConfig::Properties ipconfig_props_;
};

TEST_F(StaticIPParametersTest, InitState) {
  ExpectEmptyIPConfig();

  // Applying an empty set of parameters on an empty set of properties should
  // be a no-op.
  static_params_.ApplyTo(&ipconfig_props_);
  ExpectEmptyIPConfig();
}

TEST_F(StaticIPParametersTest, ApplyEmptyParameters) {
  PopulateIPConfig();
  static_params_.ApplyTo(&ipconfig_props_);
  ExpectPopulatedIPConfig();
}

TEST_F(StaticIPParametersTest, ControlInterface) {
  PropertyStore store;
  static_params_.PlumbPropertyStore(&store);
  SetStaticProperties(&store);
  static_params_.ApplyTo(&ipconfig_props_);
  ExpectPopulatedIPConfig();

  EXPECT_TRUE(static_params_.ContainsAddress());
  Error unused_error;
  store.ClearProperty("StaticIP.Address", &unused_error);
  EXPECT_FALSE(static_params_.ContainsAddress());
  store.ClearProperty("StaticIP.Mtu", &unused_error);
  IPConfig::Properties props;
  const string kTestAddress("test_address");
  props.address = kTestAddress;
  const int32_t kTestMtu = 256;
  props.mtu = kTestMtu;
  static_params_.ApplyTo(&props);
  EXPECT_EQ(kTestAddress, props.address);
  EXPECT_EQ(kTestMtu, props.mtu);

  {
    Error error;
    EXPECT_FALSE(store.GetStringProperty("StaticIP.Address", nullptr, &error));
    EXPECT_EQ(Error::kNotFound, error.type());
  }
  string string_value;
  EXPECT_TRUE(store.GetStringProperty("StaticIP.Gateway", &string_value,
                                      &unused_error));
  EXPECT_EQ(kGateway, string_value);
  {
    Error error;
    EXPECT_FALSE(store.GetInt32Property("StaticIP.Mtu", nullptr, &error));
    EXPECT_EQ(Error::kNotFound, error.type());
  }
  EXPECT_TRUE(store.GetStringProperty("StaticIP.NameServers", &string_value,
                                      &unused_error));
  EXPECT_EQ(kNameServers, string_value);
  EXPECT_TRUE(store.GetStringProperty("StaticIP.SearchDomains", &string_value,
                                      &unused_error));
  EXPECT_EQ(kSearchDomains, string_value);
  EXPECT_TRUE(store.GetStringProperty("StaticIP.PeerAddress", &string_value,
                                      &unused_error));
  EXPECT_EQ(kPeerAddress, string_value);
  int32_t int_value;
  EXPECT_TRUE(
      store.GetInt32Property("StaticIP.Prefixlen", &int_value, &unused_error));
  EXPECT_EQ(kPrefixLen, int_value);
  EXPECT_TRUE(store.GetStringProperty("StaticIP.ExcludedRoutes", &string_value,
                                      &unused_error));
  EXPECT_EQ(kExcludedRoutes, string_value);
  EXPECT_TRUE(store.GetStringProperty("StaticIP.IncludedRoutes", &string_value,
                                      &unused_error));
  EXPECT_EQ(kIncludedRoutes, string_value);
}

TEST_F(StaticIPParametersTest, Profile) {
  StrictMock<MockStore> store;
  const string kID = "storage_id";
  EXPECT_CALL(store, GetString(kID, "StaticIP.Address", _))
      .WillOnce(DoAll(SetArgPointee<2>(string(kAddress)), Return(true)));
  EXPECT_CALL(store, GetString(kID, "StaticIP.Gateway", _))
      .WillOnce(DoAll(SetArgPointee<2>(string(kGateway)), Return(true)));
  EXPECT_CALL(store, GetInt(kID, "StaticIP.Mtu", _))
      .WillOnce(DoAll(SetArgPointee<2>(kMtu), Return(true)));
  EXPECT_CALL(store, GetString(kID, "StaticIP.NameServers", _))
      .WillOnce(DoAll(SetArgPointee<2>(string(kNameServers)), Return(true)));
  EXPECT_CALL(store, GetString(kID, "StaticIP.SearchDomains", _))
      .WillOnce(DoAll(SetArgPointee<2>(string(kSearchDomains)), Return(true)));
  EXPECT_CALL(store, GetString(kID, "StaticIP.PeerAddress", _))
      .WillOnce(DoAll(SetArgPointee<2>(string(kPeerAddress)), Return(true)));
  EXPECT_CALL(store, GetInt(kID, "StaticIP.Prefixlen", _))
      .WillOnce(DoAll(SetArgPointee<2>(kPrefixLen), Return(true)));
  EXPECT_CALL(store, GetString(kID, "StaticIP.ExcludedRoutes", _))
      .WillOnce(DoAll(SetArgPointee<2>(string(kExcludedRoutes)), Return(true)));
  EXPECT_CALL(store, GetString(kID, "StaticIP.IncludedRoutes", _))
      .WillOnce(DoAll(SetArgPointee<2>(string(kIncludedRoutes)), Return(true)));
  static_params_.Load(&store, kID);
  static_params_.ApplyTo(&ipconfig_props_);
  ExpectPopulatedIPConfig();

  EXPECT_CALL(store, SetString(kID, "StaticIP.Address", kAddress))
      .WillOnce(Return(true));
  EXPECT_CALL(store, SetString(kID, "StaticIP.Gateway", kGateway))
      .WillOnce(Return(true));
  EXPECT_CALL(store, SetInt(kID, "StaticIP.Mtu", kMtu)).WillOnce(Return(true));
  EXPECT_CALL(store, SetString(kID, "StaticIP.NameServers", kNameServers))
      .WillOnce(Return(true));
  EXPECT_CALL(store, SetString(kID, "StaticIP.SearchDomains", kSearchDomains))
      .WillOnce(Return(true));
  EXPECT_CALL(store, SetString(kID, "StaticIP.PeerAddress", kPeerAddress))
      .WillOnce(Return(true));
  EXPECT_CALL(store, SetInt(kID, "StaticIP.Prefixlen", kPrefixLen))
      .WillOnce(Return(true));
  EXPECT_CALL(store, SetString(kID, "StaticIP.ExcludedRoutes", kExcludedRoutes))
      .WillOnce(Return(true));
  EXPECT_CALL(store, SetString(kID, "StaticIP.IncludedRoutes", kIncludedRoutes))
      .WillOnce(Return(true));
  static_params_.Save(&store, kID);
}

TEST_F(StaticIPParametersTest, SavedParameters) {
  // Calling RestoreTo() when no parameters are set should not crash or
  // add any entries.
  static_params_.RestoreTo(&ipconfig_props_);
  ExpectEmptyIPConfig();

  PopulateIPConfig();
  PropertyStore static_params_props;
  static_params_.PlumbPropertyStore(&static_params_props);
  SetStaticPropertiesWithVersion(&static_params_props, 1);
  static_params_.ApplyTo(&ipconfig_props_);

  // The version 0 properties in |ipconfig_props_| are now in SavedIP.*
  // properties, while the version 1 StaticIP parameters are now in
  // |ipconfig_props_|.
  ExpectPropertiesWithVersion(&static_params_props, "SavedIP", 0);
  ExpectPopulatedIPConfigWithVersion(1);

  // Clear all "StaticIP" parameters.
  static_params_.args_.Clear();

  // Another ApplyTo() call rotates the version 1 properties in
  // |ipconfig_props_| over to SavedIP.*.  Since there are no StaticIP
  // parameters, |ipconfig_props_| should remain populated with version 1
  // parameters.
  static_params_.ApplyTo(&ipconfig_props_);
  ExpectPropertiesWithVersion(&static_params_props, "SavedIP", 1);
  ExpectPopulatedIPConfigWithVersion(1);

  // Reset |ipconfig_props_| to version 0.
  PopulateIPConfig();

  // A RestoreTo() call moves the version 1 "SavedIP" parameters into
  // |ipconfig_props_|.
  SetStaticPropertiesWithVersion(&static_params_props, 2);
  static_params_.RestoreTo(&ipconfig_props_);
  ExpectPopulatedIPConfigWithVersion(1);

  // All "SavedIP" parameters should be cleared.
  EXPECT_TRUE(static_params_.saved_args_.IsEmpty());

  // Static IP parameters should be unchanged.
  ExpectPropertiesWithVersion(&static_params_props, "StaticIP", 2);
}

TEST_F(StaticIPParametersTest, SavedParametersDict) {
  // Calling RestoreTo() when no parameters are set should not crash or
  // add any entries.
  static_params_.RestoreTo(&ipconfig_props_);
  ExpectEmptyIPConfig();

  PopulateIPConfig();
  PropertyStore static_params_props;
  static_params_.PlumbPropertyStore(&static_params_props);
  SetStaticDictPropertiesWithVersion(&static_params_props, 1);
  static_params_.ApplyTo(&ipconfig_props_);

  // The version 0 properties in |ipconfig_props_| are now in SavedIP.*
  // properties, while the version 1 StaticIP parameters are now in
  // |ipconfig_props_|.
  ExpectPropertiesWithVersion(&static_params_props, "SavedIP", 0);
  ExpectPopulatedIPConfigWithVersion(1);

  // Clear all "StaticIP" parameters.
  static_params_.args_.Clear();

  // Another ApplyTo() call rotates the version 1 properties in
  // |ipconfig_props_| over to SavedIP.*.  Since there are no StaticIP
  // parameters, |ipconfig_props_| should remain populated with version 1
  // parameters.
  static_params_.ApplyTo(&ipconfig_props_);
  ExpectPropertiesWithVersion(&static_params_props, "SavedIP", 1);
  ExpectPopulatedIPConfigWithVersion(1);

  // Reset |ipconfig_props_| to version 0.
  PopulateIPConfig();

  // A RestoreTo() call moves the version 1 "SavedIP" parameters into
  // |ipconfig_props_|.
  SetStaticDictPropertiesWithVersion(&static_params_props, 2);
  static_params_.RestoreTo(&ipconfig_props_);
  ExpectPopulatedIPConfigWithVersion(1);

  // All "SavedIP" parameters should be cleared.
  EXPECT_TRUE(static_params_.saved_args_.IsEmpty());

  // Static IP parameters should be unchanged.
  ExpectPropertiesWithVersion(&static_params_props, "StaticIP", 2);
}

}  // namespace shill
