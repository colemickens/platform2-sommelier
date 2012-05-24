// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wimax_provider.h"

#include <base/file_util.h>
#include <base/scoped_temp_dir.h>
#include <base/stringprintf.h>
#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>

#include "shill/glib.h"
#include "shill/key_file_store.h"
#include "shill/mock_device_info.h"
#include "shill/mock_profile.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/mock_wimax.h"
#include "shill/mock_wimax_manager_proxy.h"
#include "shill/mock_wimax_network_proxy.h"
#include "shill/mock_wimax_service.h"
#include "shill/nice_mock_control.h"
#include "shill/proxy_factory.h"
#include "shill/wimax_service.h"

using std::string;
using std::vector;
using testing::_;
using testing::Return;

namespace shill {

namespace {

string GetTestLinkName(int index) {
  return StringPrintf("wm%d", index);
}

string GetTestPath(int index) {
  return wimax_manager::kDeviceObjectPathPrefix + GetTestLinkName(index);
}

string GetTestNetworkPath(uint32 identifier) {
  return base::StringPrintf("%s%08x",
                            wimax_manager::kNetworkObjectPathPrefix,
                            identifier);
}

}  // namespace

class WiMaxProviderTest : public testing::Test {
 public:
  WiMaxProviderTest()
      : manager_proxy_(new MockWiMaxManagerProxy()),
        network_proxy_(new MockWiMaxNetworkProxy()),
        proxy_factory_(this),
        manager_(&control_, NULL, &metrics_, NULL),
        device_info_(&control_, NULL, &metrics_, &manager_),
        provider_(&control_, NULL, &metrics_, &manager_) {}

  virtual ~WiMaxProviderTest() {}

 protected:
  class TestProxyFactory : public ProxyFactory {
   public:
    explicit TestProxyFactory(WiMaxProviderTest *test) : test_(test) {}

    virtual WiMaxManagerProxyInterface *CreateWiMaxManagerProxy() {
      return test_->manager_proxy_.release();
    }

    virtual WiMaxNetworkProxyInterface *CreateWiMaxNetworkProxy(
        const string &/*path*/) {
      return test_->network_proxy_.release();
    }

   private:
    WiMaxProviderTest *test_;

    DISALLOW_COPY_AND_ASSIGN(TestProxyFactory);
  };

  virtual void SetUp() {
    provider_.proxy_factory_ = &proxy_factory_;
  }

  virtual void TearDown() {
    provider_.proxy_factory_ = NULL;
  }

  scoped_ptr<MockWiMaxManagerProxy> manager_proxy_;
  scoped_ptr<MockWiMaxNetworkProxy> network_proxy_;
  TestProxyFactory proxy_factory_;
  NiceMockControl control_;
  MockMetrics metrics_;
  MockManager manager_;
  MockDeviceInfo device_info_;
  WiMaxProvider provider_;
};

TEST_F(WiMaxProviderTest, StartStop) {
  EXPECT_FALSE(provider_.manager_proxy_.get());
  EXPECT_CALL(*manager_proxy_, set_devices_changed_callback(_)).Times(1);
  EXPECT_CALL(*manager_proxy_, Devices(_)).WillOnce(Return(RpcIdentifiers()));
  provider_.Start();
  EXPECT_TRUE(provider_.manager_proxy_.get());
  provider_.Start();
  EXPECT_TRUE(provider_.manager_proxy_.get());
  provider_.pending_devices_[GetTestLinkName(2)] = GetTestPath(2);
  provider_.Stop();
  EXPECT_FALSE(provider_.manager_proxy_.get());
  EXPECT_TRUE(provider_.pending_devices_.empty());
  provider_.Stop();
}

TEST_F(WiMaxProviderTest, OnDevicesChanged) {
  EXPECT_CALL(manager_, device_info()).WillRepeatedly(Return(&device_info_));

  provider_.pending_devices_[GetTestLinkName(1)] = GetTestPath(1);
  RpcIdentifiers live_devices;
  live_devices.push_back(GetTestPath(2));
  live_devices.push_back(GetTestPath(3));
  EXPECT_CALL(device_info_, GetIndex(GetTestLinkName(2))).WillOnce(Return(-1));
  EXPECT_CALL(device_info_, GetIndex(GetTestLinkName(3))).WillOnce(Return(-1));
  provider_.OnDevicesChanged(live_devices);
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
  RpcIdentifiers live_devices;
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

TEST_F(WiMaxProviderTest, FindService) {
  EXPECT_FALSE(provider_.FindService("some_storage_id"));
  scoped_refptr<MockWiMaxService> service(
      new MockWiMaxService(&control_, NULL, &metrics_, &manager_));
  static const char kName[] = "WiMAX Network";
  static const char kNetworkId[] = "76543210";
  service->set_friendly_name(kName);
  service->set_network_id(kNetworkId);
  service->InitStorageIdentifier();
  provider_.services_.push_back(
      new MockWiMaxService(&control_, NULL, &metrics_, &manager_));
  provider_.services_.push_back(service);
  provider_.services_.push_back(
      new MockWiMaxService(&control_, NULL, &metrics_, &manager_));
  EXPECT_EQ(service.get(),
            provider_.FindService(
                WiMaxService::CreateStorageIdentifier(kNetworkId,
                                                      kName)).get());
  EXPECT_FALSE(provider_.FindService("some_storage_id"));
}

TEST_F(WiMaxProviderTest, StartLiveServicesForNetwork) {
  const uint32 kIdentifier = 0x1234567;
  static const char kNetworkId[] = "01234567";
  static const char kName[] = "Some WiMAX Provider";
  vector<scoped_refptr<MockWiMaxService> > services(4);
  for (size_t i = 0; i < services.size(); i++) {
    services[i] = new MockWiMaxService(&control_, NULL, &metrics_, &manager_);
    services[i]->set_network_id(kNetworkId);
    provider_.services_.push_back(services[i]);
  }
  services[0]->set_network_id("deadbeef");
  // Make services[3] the default service.
  services[3]->set_friendly_name(kName);
  services[3]->InitStorageIdentifier();
  EXPECT_CALL(*network_proxy_, Name(_)).WillOnce(Return(kName));
  EXPECT_CALL(*network_proxy_, Identifier(_)).WillOnce(Return(kIdentifier));
  EXPECT_CALL(*services[0], IsStarted()).Times(0);
  EXPECT_CALL(*services[1], IsStarted()).WillOnce(Return(true));
  EXPECT_CALL(*services[1], Start(_)).Times(0);
  EXPECT_CALL(*services[2], IsStarted()).WillOnce(Return(false));
  EXPECT_CALL(*services[2], Start(_)).WillOnce(Return(true));
  EXPECT_CALL(*services[3], IsStarted()).WillOnce(Return(false));
  EXPECT_CALL(*services[3], Start(_)).WillOnce(Return(false));
  EXPECT_CALL(manager_, RegisterService(_)).Times(0);
  provider_.StartLiveServicesForNetwork(GetTestNetworkPath(kIdentifier));
}

TEST_F(WiMaxProviderTest, DestroyAllServices) {
  vector<scoped_refptr<MockWiMaxService> > services(2);
  for (size_t i = 0; i < services.size(); i++) {
    services[i] = new MockWiMaxService(&control_, NULL, &metrics_, &manager_);
    provider_.services_.push_back(services[i]);
    EXPECT_CALL(*services[i], Stop());
  }
  EXPECT_CALL(manager_, DeregisterService(_)).Times(services.size());
  provider_.DestroyAllServices();
  EXPECT_TRUE(provider_.services_.empty());
}

TEST_F(WiMaxProviderTest, StopDeadServices) {
  vector<scoped_refptr<MockWiMaxService> > services(4);
  for (size_t i = 0; i < services.size(); i++) {
    services[i] = new MockWiMaxService(&control_, NULL, &metrics_, &manager_);
    if (i > 0) {
      EXPECT_CALL(*services[i], IsStarted()).WillOnce(Return(true));
      EXPECT_CALL(*services[i], GetNetworkObjectPath())
          .WillOnce(Return(GetTestNetworkPath(100 + i)));
    } else {
      EXPECT_CALL(*services[i], IsStarted()).WillOnce(Return(false));
      EXPECT_CALL(*services[i], GetNetworkObjectPath()).Times(0);
      EXPECT_CALL(*services[i], Stop()).Times(0);
    }
    provider_.services_.push_back(services[i]);
  }

  EXPECT_CALL(*services[1], Stop()).Times(0);
  EXPECT_CALL(*services[2], Stop());
  EXPECT_CALL(*services[3], Stop());
  provider_.networks_.insert(GetTestNetworkPath(777));
  provider_.networks_.insert(GetTestNetworkPath(101));
  provider_.StopDeadServices();

  EXPECT_EQ(4, provider_.services_.size());
}

TEST_F(WiMaxProviderTest, OnNetworksChanged) {
  static const char kName[] = "Default Network";
  const uint32 kIdentifier = 0xabcdef;
  static const char kNetworkId[] = "00abcdef";

  // Started service to be stopped.
  scoped_refptr<MockWiMaxService> service0(
      new MockWiMaxService(&control_, NULL, &metrics_, &manager_));
  EXPECT_CALL(*service0, IsStarted()).WillOnce(Return(true));
  EXPECT_CALL(*service0, GetNetworkObjectPath())
      .WillOnce(Return(GetTestNetworkPath(100)));
  EXPECT_CALL(*service0, Start(_)).Times(0);
  EXPECT_CALL(*service0, Stop()).Times(1);
  service0->set_network_id("1234");

  // Stopped service to be started.
  scoped_refptr<MockWiMaxService> service1(
      new MockWiMaxService(&control_, NULL, &metrics_, &manager_));
  EXPECT_CALL(*service1, IsStarted()).Times(2).WillRepeatedly(Return(false));
  EXPECT_CALL(*service1, Start(_)).WillOnce(Return(true));
  EXPECT_CALL(*service1, Stop()).Times(0);
  service1->set_network_id(kNetworkId);
  service1->set_friendly_name(kName);
  service1->InitStorageIdentifier();
  EXPECT_CALL(*network_proxy_, Name(_)).WillOnce(Return(kName));
  EXPECT_CALL(*network_proxy_, Identifier(_)).WillOnce(Return(kIdentifier));

  provider_.services_.push_back(service0);
  provider_.services_.push_back(service1);

  for (int i = 0; i < 3; i++) {
    scoped_refptr<MockWiMax> device(
        new MockWiMax(&control_, NULL, &metrics_, &manager_,
                      GetTestLinkName(i), "", i, GetTestPath(i)));
    provider_.devices_[GetTestLinkName(i)] = device;
    if (i > 0) {
      device->networks_.insert(GetTestNetworkPath(101));
    }
  }
  EXPECT_CALL(manager_, RegisterService(_)).Times(0);
  EXPECT_CALL(manager_, DeregisterService(_)).Times(0);

  provider_.networks_.insert("foo");
  provider_.OnNetworksChanged();
  EXPECT_EQ(1, provider_.networks_.size());
  EXPECT_TRUE(ContainsKey(provider_.networks_, GetTestNetworkPath(101)));
}

TEST_F(WiMaxProviderTest, GetUniqueService) {
  EXPECT_TRUE(provider_.services_.empty());

  static const char kName0[] = "Test WiMAX Network";
  static const char kName1[] = "Unknown Network";
  static const char kNetworkId[] = "12340000";

  // Service already exists.
  scoped_refptr<MockWiMaxService> service0(
      new MockWiMaxService(&control_, NULL, &metrics_, &manager_));
  service0->set_network_id(kNetworkId);
  service0->set_friendly_name(kName0);
  service0->InitStorageIdentifier();
  provider_.services_.push_back(service0);
  EXPECT_CALL(manager_, RegisterService(_)).Times(0);
  WiMaxServiceRefPtr service = provider_.GetUniqueService(kNetworkId, kName0);
  ASSERT_TRUE(service);
  EXPECT_EQ(service0.get(), service.get());
  EXPECT_EQ(1, provider_.services_.size());

  // Create a new service.
  EXPECT_CALL(manager_, RegisterService(_));
  service = provider_.GetUniqueService(kNetworkId, kName1);
  ASSERT_TRUE(service);
  EXPECT_NE(service0.get(), service.get());
  EXPECT_EQ(2, provider_.services_.size());
  EXPECT_EQ(WiMaxService::CreateStorageIdentifier(kNetworkId, kName1),
            service->GetStorageIdentifier());

  // Service already exists -- it was just created.
  EXPECT_CALL(manager_, RegisterService(_)).Times(0);
  WiMaxServiceRefPtr service1 = provider_.GetUniqueService(kNetworkId, kName1);
  ASSERT_TRUE(service1);
  EXPECT_EQ(service.get(), service1.get());
  EXPECT_EQ(2, provider_.services_.size());
}

TEST_F(WiMaxProviderTest, GetDefaultService) {
  EXPECT_TRUE(provider_.services_.empty());

  static const char kName[] = "Default Network";
  const uint32 kIdentifier = 0xabcdef;
  static const char kNetworkId[] = "00abcdef";

  // Ensure that a service is created and registered if it doesn't exist
  // already.
  EXPECT_CALL(*network_proxy_, Name(_)).WillOnce(Return(kName));
  EXPECT_CALL(*network_proxy_, Identifier(_)).WillOnce(Return(kIdentifier));
  EXPECT_CALL(manager_, RegisterService(_));
  WiMaxServiceRefPtr service =
      provider_.GetDefaultService(GetTestNetworkPath(kIdentifier));
  ASSERT_TRUE(service);
  EXPECT_EQ(1, provider_.services_.size());
  EXPECT_EQ(WiMaxService::CreateStorageIdentifier(kNetworkId, kName),
            service->GetStorageIdentifier());

  // Ensure that no duplicate service is created or registered.
  ASSERT_FALSE(network_proxy_.get());
  network_proxy_.reset(new MockWiMaxNetworkProxy());
  EXPECT_CALL(*network_proxy_, Name(_)).WillOnce(Return(kName));
  EXPECT_CALL(*network_proxy_, Identifier(_)).WillOnce(Return(kIdentifier));
  EXPECT_CALL(manager_, RegisterService(_)).Times(0);
  WiMaxServiceRefPtr service2 =
      provider_.GetDefaultService(GetTestNetworkPath(kIdentifier));
  ASSERT_TRUE(service2);
  EXPECT_EQ(1, provider_.services_.size());
  EXPECT_EQ(service.get(), service2.get());
}

TEST_F(WiMaxProviderTest, CreateServicesFromProfile) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath test_profile = temp_dir.path().Append("test-profile");
  GLib glib;
  KeyFileStore store(&glib);
  store.set_path(test_profile);
  static const char contents[] =
      "[no_type]\n"
      "Name=No Type Entry\n"
      "\n"
      "[no_wimax]\n"
      "Type=vpn\n"
      "\n"
      "[wimax_network_01234567]\n"
      "Name=network\n"
      "Type=wimax\n"
      "NetworkId=01234567\n"
      "\n"
      "[no_network_id]\n"
      "Type=wimax\n"
      "\n"
      "[no_name]\n"
      "Type=wimax\n"
      "NetworkId=76543210\n";
  EXPECT_EQ(strlen(contents),
            file_util::WriteFile(test_profile, contents, strlen(contents)));
  ASSERT_TRUE(store.Open());
  scoped_refptr<MockProfile> profile(new MockProfile(&control_, &manager_));
  EXPECT_CALL(*profile, GetConstStorage())
      .Times(2)
      .WillRepeatedly(Return(&store));
  EXPECT_CALL(manager_, RegisterService(_));
  EXPECT_CALL(*profile, ConfigureService(_)).WillOnce(Return(true));
  provider_.CreateServicesFromProfile(profile);
  ASSERT_EQ(1, provider_.services_.size());
  WiMaxServiceRefPtr service = provider_.services_[0];
  EXPECT_EQ("wimax_network_01234567", service->GetStorageIdentifier());
  provider_.CreateServicesFromProfile(profile);
  ASSERT_EQ(1, provider_.services_.size());
  EXPECT_EQ(service.get(), provider_.services_[0].get());
}

TEST_F(WiMaxProviderTest, GetService) {
  KeyValueStore args;
  Error e;

  args.SetString(flimflam::kTypeProperty, flimflam::kTypeWimax);

  // No network id property.
  WiMaxServiceRefPtr service = provider_.GetService(args, &e);
  EXPECT_EQ(Error::kInvalidArguments, e.type());
  EXPECT_FALSE(service);

  // No name property.
  static const char kNetworkId[] = "1234abcd";
  args.SetString(WiMaxService::kNetworkIdProperty, kNetworkId);
  e.Reset();
  service = provider_.GetService(args, &e);
  EXPECT_EQ(Error::kInvalidArguments, e.type());
  EXPECT_FALSE(service);

  // Service created and configured.
  static const char kName[] = "Test WiMAX Network";
  args.SetString(flimflam::kNameProperty, kName);
  static const char kIdentity[] = "joe";
  args.SetString(flimflam::kEapIdentityProperty, kIdentity);
  EXPECT_CALL(manager_, RegisterService(_));
  e.Reset();
  service = provider_.GetService(args, &e);
  EXPECT_TRUE(e.IsSuccess());
  ASSERT_TRUE(service);
  EXPECT_EQ(kIdentity, service->eap().identity);
}

TEST_F(WiMaxProviderTest, SelectCarrier) {
  scoped_refptr<MockWiMaxService> service(
      new MockWiMaxService(&control_, NULL, &metrics_, &manager_));
  EXPECT_FALSE(provider_.SelectCarrier(service));
  scoped_refptr<MockWiMax> device(
      new MockWiMax(&control_, NULL, &metrics_, &manager_,
                    GetTestLinkName(1), "", 1, GetTestPath(1)));
  provider_.devices_[GetTestLinkName(1)] = device;
  WiMaxRefPtr carrier = provider_.SelectCarrier(service);
  EXPECT_EQ(device.get(), carrier.get());
}

}  // namespace shill
