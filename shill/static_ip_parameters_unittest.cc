// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/static_ip_parameters.h"

#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>

#include "shill/ipconfig.h"
#include "shill/mock_store.h"
#include "shill/property_store.h"
#include "shill/property_store_inspector.h"

using std::string;
using std::vector;
using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrictMock;
using testing::Test;

namespace shill {

namespace {
static const char kAddress[] = "10.0.0.1";
static const char kGateway[] = "10.0.0.254";
static const int32 kMtu = 512;
static const char kNameServer0[] = "10.0.1.253";
static const char kNameServer1[] = "10.0.1.252";
static const char kNameServers[] = "10.0.1.253,10.0.1.252";
static const char kPeerAddress[] = "10.0.0.2";
static const int32 kPrefixLen = 24;
} // namespace {}

class StaticIpParametersTest : public Test {
 public:
  StaticIpParametersTest() {}

  void PopulateParams();
  void PopulateProps();
  void ExpectEmpty() {
    EXPECT_TRUE(props_.address.empty());
    EXPECT_TRUE(props_.gateway.empty());
    EXPECT_FALSE(props_.mtu);
    EXPECT_TRUE(props_.dns_servers.empty());
    EXPECT_TRUE(props_.peer_address.empty());
    EXPECT_FALSE(props_.subnet_prefix);
  }
  void ExpectPopulated() {
    EXPECT_EQ(kAddress, props_.address);
    EXPECT_EQ(kGateway, props_.gateway);
    EXPECT_EQ(kMtu, props_.mtu);
    EXPECT_EQ(2, props_.dns_servers.size());
    EXPECT_EQ(kNameServer0, props_.dns_servers[0]);
    EXPECT_EQ(kNameServer1, props_.dns_servers[1]);
    EXPECT_EQ(kPeerAddress, props_.peer_address);
    EXPECT_EQ(kPrefixLen, props_.subnet_prefix);
  }
  void Populate() {
    props_.address = kAddress;
    props_.gateway = kGateway;
    props_.mtu = kMtu;
    props_.dns_servers.push_back(kNameServer0);
    props_.dns_servers.push_back(kNameServer1);
    props_.peer_address = kPeerAddress;
    props_.subnet_prefix = kPrefixLen;
  }
 protected:
  StaticIPParameters static_params_;
  IPConfig::Properties props_;
};

TEST_F(StaticIpParametersTest, InitState) {
  EXPECT_TRUE(props_.address.empty());
  EXPECT_TRUE(props_.gateway.empty());
  EXPECT_FALSE(props_.mtu);
  EXPECT_TRUE(props_.dns_servers.empty());
  EXPECT_TRUE(props_.peer_address.empty());
  EXPECT_FALSE(props_.subnet_prefix);

  // Applying an empty set of parameters on an empty set of properties should
  // be a no-op.
  static_params_.ApplyTo(&props_);
  ExpectEmpty();
}

TEST_F(StaticIpParametersTest, ApplyEmptyParameters) {
  Populate();
  static_params_.ApplyTo(&props_);
  ExpectPopulated();
}

TEST_F(StaticIpParametersTest, ControlInterface) {
  PropertyStore store;
  static_params_.PlumbPropertyStore(&store);
  Error error;
  store.SetStringProperty("StaticIP.Address", kAddress, &error);
  store.SetStringProperty("StaticIP.Gateway", kGateway, &error);
  store.SetInt32Property("StaticIP.Mtu", kMtu, &error);
  store.SetStringProperty("StaticIP.NameServers", kNameServers, &error);
  store.SetStringProperty("StaticIP.PeerAddress", kPeerAddress, &error);
  store.SetInt32Property("StaticIP.Prefixlen", kPrefixLen, &error);
  static_params_.ApplyTo(&props_);
  ExpectPopulated();

  EXPECT_TRUE(static_params_.ContainsAddress());
  store.ClearProperty("StaticIP.Address", &error);
  EXPECT_FALSE(static_params_.ContainsAddress());
  store.ClearProperty("StaticIP.Mtu", &error);
  IPConfig::Properties props;
  const string kTestAddress("test_address");
  props.address = kTestAddress;
  const int32 kTestMtu = 256;
  props.mtu = kTestMtu;
  static_params_.ApplyTo(&props);
  EXPECT_EQ(kTestAddress, props.address);
  EXPECT_EQ(kTestMtu, props.mtu);

  PropertyStoreInspector inspector(&store);
  EXPECT_FALSE(inspector.ContainsStringProperty("StaticIP.Address"));
  string string_value;
  EXPECT_TRUE(inspector.GetStringProperty("StaticIP.Gateway",
                                          &string_value, &error));
  EXPECT_EQ(kGateway, string_value);
  EXPECT_FALSE(inspector.ContainsInt32Property("StaticIP.Mtu"));
  EXPECT_TRUE(inspector.GetStringProperty("StaticIP.NameServers",
                                          &string_value, &error));
  EXPECT_EQ(kNameServers, string_value);
  EXPECT_TRUE(inspector.GetStringProperty("StaticIP.PeerAddress",
                                          &string_value, &error));
  EXPECT_EQ(kPeerAddress, string_value);
  int32 int_value;
  EXPECT_TRUE(inspector.GetInt32Property("StaticIP.Prefixlen",
                                         &int_value, &error));
  EXPECT_EQ(kPrefixLen, int_value);
}

TEST_F(StaticIpParametersTest, Profile) {
  StrictMock<MockStore> store;
  const string kID = "storage_id";
  EXPECT_CALL(store, GetString(kID, "StaticIP.Address", _))
      .WillOnce(DoAll(SetArgumentPointee<2>(string(kAddress)), Return(true)));
  EXPECT_CALL(store, GetString(kID, "StaticIP.Gateway", _))
      .WillOnce(DoAll(SetArgumentPointee<2>(string(kGateway)), Return(true)));
  EXPECT_CALL(store, GetInt(kID, "StaticIP.Mtu", _))
      .WillOnce(DoAll(SetArgumentPointee<2>(kMtu), Return(true)));
  EXPECT_CALL(store, GetString(kID, "StaticIP.NameServers", _))
      .WillOnce(DoAll(SetArgumentPointee<2>(string(kNameServers)),
                      Return(true)));
  EXPECT_CALL(store, GetString(kID, "StaticIP.PeerAddress", _))
      .WillOnce(DoAll(SetArgumentPointee<2>(string(kPeerAddress)),
                      Return(true)));
  EXPECT_CALL(store, GetInt(kID, "StaticIP.Prefixlen", _))
      .WillOnce(DoAll(SetArgumentPointee<2>(kPrefixLen), Return(true)));
  static_params_.Load(&store, kID);
  static_params_.ApplyTo(&props_);
  ExpectPopulated();

  EXPECT_CALL(store, SetString(kID, "StaticIP.Address", kAddress))
      .WillOnce(Return(true));
  EXPECT_CALL(store, SetString(kID, "StaticIP.Gateway", kGateway))
      .WillOnce(Return(true));
  EXPECT_CALL(store, SetInt(kID, "StaticIP.Mtu", kMtu))
      .WillOnce(Return(true));
  EXPECT_CALL(store, SetString(kID, "StaticIP.NameServers", kNameServers))
      .WillOnce(Return(true));
  EXPECT_CALL(store, SetString(kID, "StaticIP.PeerAddress", kPeerAddress))
      .WillOnce(Return(true));
  EXPECT_CALL(store, SetInt(kID, "StaticIP.Prefixlen", kPrefixLen))
      .WillOnce(Return(true));
  static_params_.Save(&store, kID);
}

}  // namespace shill
