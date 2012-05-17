// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wimax_provider.h"

#include <base/stringprintf.h>
#include <gtest/gtest.h>

#include "shill/mock_dbus_properties_proxy.h"
#include "shill/mock_device_info.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/mock_wimax.h"
#include "shill/mock_wimax_manager_proxy.h"
#include "shill/nice_mock_control.h"
#include "shill/proxy_factory.h"

using std::string;
using std::vector;
using testing::_;
using testing::Return;

namespace shill {

namespace {
const char kWiMaxDevicePathPrefix[] = "/org/chromium/WiMaxManager/Device/";

string GetTestLinkName(int index) {
  return StringPrintf("wm%d", index);
}

string GetTestPath(int index) {
  return kWiMaxDevicePathPrefix + GetTestLinkName(index);
}
}  // namespace

class WiMaxProviderTest : public testing::Test {
 public:
  WiMaxProviderTest()
      : manager_proxy_(new MockWiMaxManagerProxy()),
        properties_proxy_(new MockDBusPropertiesProxy()),
        proxy_factory_(this),
        manager_(&control_, NULL, &metrics_, NULL),
        device_info_(&control_, NULL, &metrics_, &manager_),
        provider_(&control_, NULL, &metrics_, &manager_) {}

  virtual ~WiMaxProviderTest() {}

 protected:
  class TestProxyFactory : public ProxyFactory {
   public:
    explicit TestProxyFactory(WiMaxProviderTest *test) : test_(test) {}

    virtual DBusPropertiesProxyInterface *CreateDBusPropertiesProxy(
        const string &/*path*/, const string &/*service*/) {
      return test_->properties_proxy_.release();
    }

    virtual WiMaxManagerProxyInterface *CreateWiMaxManagerProxy() {
      return test_->manager_proxy_.release();
    }

   private:
    WiMaxProviderTest *test_;

    DISALLOW_COPY_AND_ASSIGN(TestProxyFactory);
  };

  virtual void SetUp() {
    provider_.proxy_factory_ = &proxy_factory_;
  }

  virtual void TearDown() {
    provider_.devices_.clear();
    provider_.proxy_factory_ = NULL;
  }

  scoped_ptr<MockWiMaxManagerProxy> manager_proxy_;
  scoped_ptr<MockDBusPropertiesProxy> properties_proxy_;
  TestProxyFactory proxy_factory_;
  NiceMockControl control_;
  MockMetrics metrics_;
  MockManager manager_;
  MockDeviceInfo device_info_;
  WiMaxProvider provider_;
};

TEST_F(WiMaxProviderTest, StartStop) {
  EXPECT_FALSE(provider_.manager_proxy_.get());
  EXPECT_FALSE(provider_.properties_proxy_.get());
  vector<RpcIdentifier> devices;
  EXPECT_CALL(*properties_proxy_, set_properties_changed_callback(_)).Times(1);
  EXPECT_CALL(*manager_proxy_, Devices(_)).WillOnce(Return(devices));
  provider_.Start();
  EXPECT_TRUE(provider_.manager_proxy_.get());
  EXPECT_TRUE(provider_.properties_proxy_.get());
  provider_.Start();
  EXPECT_TRUE(provider_.manager_proxy_.get());
  EXPECT_TRUE(provider_.properties_proxy_.get());
  provider_.pending_devices_[GetTestLinkName(2)] = GetTestPath(2);
  provider_.Stop();
  EXPECT_FALSE(provider_.manager_proxy_.get());
  EXPECT_FALSE(provider_.properties_proxy_.get());
  EXPECT_TRUE(provider_.pending_devices_.empty());
  provider_.Stop();
}

TEST_F(WiMaxProviderTest, UpdateDevices) {
  EXPECT_CALL(manager_, device_info()).WillRepeatedly(Return(&device_info_));

  provider_.pending_devices_[GetTestLinkName(1)] = GetTestPath(1);
  WiMaxProvider::RpcIdentifiers live_devices;
  live_devices.push_back(GetTestPath(2));
  live_devices.push_back(GetTestPath(3));
  EXPECT_CALL(device_info_, GetIndex(GetTestLinkName(2))).WillOnce(Return(-1));
  EXPECT_CALL(device_info_, GetIndex(GetTestLinkName(3))).WillOnce(Return(-1));
  provider_.UpdateDevices(live_devices);
  ASSERT_EQ(2, provider_.pending_devices_.size());
  EXPECT_EQ(GetTestPath(2), provider_.pending_devices_[GetTestLinkName(2)]);
  EXPECT_EQ(GetTestPath(3), provider_.pending_devices_[GetTestLinkName(3)]);
}

TEST_F(WiMaxProviderTest, OnDeviceInfoAvailable) {
  EXPECT_CALL(manager_, device_info()).WillRepeatedly(Return(&device_info_));

  provider_.pending_devices_[GetTestLinkName(1)] = GetTestPath(1);
  EXPECT_CALL(device_info_, GetIndex(GetTestLinkName(1))).WillOnce(Return(1));
  EXPECT_CALL(device_info_, GetMACAddress(1, _)).WillOnce(Return(true));
  EXPECT_CALL(device_info_, RegisterDevice(_));
  provider_.OnDeviceInfoAvailable(GetTestLinkName(1));
  EXPECT_TRUE(provider_.pending_devices_.empty());
  ASSERT_EQ(1, provider_.devices_.size());
  ASSERT_TRUE(ContainsKey(provider_.devices_, GetTestLinkName(1)));
  EXPECT_EQ(GetTestLinkName(1),
            provider_.devices_[GetTestLinkName(1)]->link_name());
}

TEST_F(WiMaxProviderTest, OnPropertiesChanged) {
  EXPECT_CALL(manager_, device_info()).WillRepeatedly(Return(&device_info_));

  vector<DBus::Path> paths;
  paths.push_back(GetTestPath(1));
  DBusPropertiesMap props;
  DBus::MessageIter writer = props["Devices"].writer();
  writer << paths;
  EXPECT_CALL(device_info_, GetIndex(GetTestLinkName(1))).WillOnce(Return(-1));
  provider_.OnPropertiesChanged(string(), props, vector<string>());
  ASSERT_EQ(1, provider_.pending_devices_.size());
  EXPECT_EQ(GetTestPath(1), provider_.pending_devices_[GetTestLinkName(1)]);
}

TEST_F(WiMaxProviderTest, CreateDevice) {
  EXPECT_CALL(manager_, device_info()).WillRepeatedly(Return(&device_info_));

  EXPECT_CALL(device_info_, GetIndex(GetTestLinkName(1))).WillOnce(Return(-1));
  provider_.CreateDevice(GetTestLinkName(1), GetTestPath(1));
  EXPECT_TRUE(provider_.devices_.empty());
  ASSERT_EQ(1, provider_.pending_devices_.size());
  EXPECT_EQ(GetTestPath(1), provider_.pending_devices_[GetTestLinkName(1)]);

  EXPECT_CALL(device_info_, GetIndex(GetTestLinkName(1))).WillOnce(Return(1));
  EXPECT_CALL(device_info_, GetMACAddress(1, _)).WillOnce(Return(true));
  EXPECT_CALL(device_info_, RegisterDevice(_));
  provider_.CreateDevice(GetTestLinkName(1), GetTestPath(1));
  EXPECT_TRUE(provider_.pending_devices_.empty());
  ASSERT_EQ(1, provider_.devices_.size());
  ASSERT_TRUE(ContainsKey(provider_.devices_, GetTestLinkName(1)));
  EXPECT_EQ(GetTestLinkName(1),
            provider_.devices_[GetTestLinkName(1)]->link_name());

  WiMax *device = provider_.devices_[GetTestLinkName(1)].get();
  provider_.CreateDevice(GetTestLinkName(1), GetTestPath(1));
  EXPECT_EQ(device, provider_.devices_[GetTestLinkName(1)].get());
}

TEST_F(WiMaxProviderTest, DestroyDeadDevices) {
  for (int i = 0; i < 4; i++) {
    provider_.devices_[GetTestLinkName(i)] =
      new MockWiMax(&control_, NULL, &metrics_, &manager_,
                    GetTestLinkName(i), "", i, GetTestPath(i));
  }
  for (int i = 4; i < 8; i++) {
    provider_.pending_devices_[GetTestLinkName(i)] = GetTestPath(i);
  }
  WiMaxProvider::RpcIdentifiers live_devices;
  live_devices.push_back(GetTestPath(0));
  live_devices.push_back(GetTestPath(3));
  live_devices.push_back(GetTestPath(4));
  live_devices.push_back(GetTestPath(7));
  live_devices.push_back(GetTestPath(123));
  EXPECT_CALL(manager_, device_info())
      .Times(2)
      .WillRepeatedly(Return(&device_info_));
  EXPECT_CALL(device_info_, DeregisterDevice(_)).Times(2);
  provider_.DestroyDeadDevices(live_devices);
  ASSERT_EQ(2, provider_.devices_.size());
  EXPECT_TRUE(ContainsKey(provider_.devices_, GetTestLinkName(0)));
  EXPECT_TRUE(ContainsKey(provider_.devices_, GetTestLinkName(3)));
  EXPECT_EQ(2, provider_.pending_devices_.size());
  EXPECT_TRUE(ContainsKey(provider_.pending_devices_, GetTestLinkName(4)));
  EXPECT_TRUE(ContainsKey(provider_.pending_devices_, GetTestLinkName(7)));
}

TEST_F(WiMaxProviderTest, GetLinkName) {
  EXPECT_EQ("", provider_.GetLinkName("/random/path"));
  EXPECT_EQ(GetTestLinkName(1), provider_.GetLinkName(GetTestPath(1)));
}

}  // namespace shill
