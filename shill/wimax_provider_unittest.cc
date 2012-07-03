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
#include "shill/mock_dbus_manager.h"
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
      : wimax_manager_proxy_(new MockWiMaxManagerProxy()),
        network_proxy_(new MockWiMaxNetworkProxy()),
        proxy_factory_(this),
        manager_(&control_, NULL, &metrics_, NULL),
        device_info_(&control_, NULL, &metrics_, &manager_),
        dbus_manager_(new MockDBusManager()),
        provider_(&control_, NULL, &metrics_, &manager_) {
    manager_.dbus_manager_.reset(dbus_manager_);  // Transfers ownership.
  }

  virtual ~WiMaxProviderTest() {}

 protected:
  class TestProxyFactory : public ProxyFactory {
   public:
    explicit TestProxyFactory(WiMaxProviderTest *test) : test_(test) {}

    virtual WiMaxManagerProxyInterface *CreateWiMaxManagerProxy() {
      return test_->wimax_manager_proxy_.release();
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

  scoped_ptr<MockWiMaxManagerProxy> wimax_manager_proxy_;
  scoped_ptr<MockWiMaxNetworkProxy> network_proxy_;
  TestProxyFactory proxy_factory_;
  NiceMockControl control_;
  MockMetrics metrics_;
  MockManager manager_;
  MockDeviceInfo device_info_;
  MockDBusManager *dbus_manager_;
  WiMaxProvider provider_;
};

TEST_F(WiMaxProviderTest, StartStop) {
  EXPECT_TRUE(provider_.on_wimax_manager_appear_.IsCancelled());
  EXPECT_FALSE(provider_.wimax_manager_proxy_.get());
  EXPECT_CALL(*dbus_manager_,
              WatchName(wimax_manager::kWiMaxManagerServiceName, _, _));
  provider_.Start();
  EXPECT_FALSE(provider_.on_wimax_manager_appear_.IsCancelled());
  EXPECT_FALSE(provider_.wimax_manager_proxy_.get());
  provider_.Start();
  EXPECT_CALL(*wimax_manager_proxy_, set_devices_changed_callback(_)).Times(1);
  EXPECT_CALL(*wimax_manager_proxy_, Devices(_))
      .WillOnce(Return(RpcIdentifiers()));
  provider_.OnWiMaxManagerAppear(":0.11");
  EXPECT_TRUE(provider_.wimax_manager_proxy_.get());
  provider_.pending_devices_[GetTestLinkName(2)] = GetTestPath(2);
  provider_.Stop();
  EXPECT_TRUE(provider_.on_wimax_manager_appear_.IsCancelled());
  EXPECT_FALSE(provider_.wimax_manager_proxy_.get());
  EXPECT_TRUE(provider_.pending_devices_.empty());
  provider_.Stop();
}

TEST_F(WiMaxProviderTest, ConnectDisconnectWiMaxManager) {
  EXPECT_FALSE(provider_.wimax_manager_proxy_.get());
  EXPECT_CALL(*wimax_manager_proxy_, set_devices_changed_callback(_)).Times(1);
  EXPECT_CALL(*wimax_manager_proxy_, Devices(_))
      .WillOnce(Return(RpcIdentifiers()));
  provider_.ConnectToWiMaxManager();
  EXPECT_TRUE(provider_.wimax_manager_proxy_.get());
  provider_.pending_devices_[GetTestLinkName(2)] = GetTestPath(2);
  provider_.DisconnectFromWiMaxManager();
  EXPECT_FALSE(provider_.wimax_manager_proxy_.get());
  EXPECT_TRUE(provider_.pending_devices_.empty());
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
    scoped_refptr<MockWiMax> device(
        new MockWiMax(&control_, NULL, &metrics_, &manager_,
                      GetTestLinkName(i), "", i, GetTestPath(i)));
    EXPECT_CALL(*device, OnDeviceVanished()).Times((i == 0 || i == 3) ? 0 : 1);
    provider_.devices_[GetTestLinkName(i)] = device;
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

TEST_F(WiMaxProviderTest, RetrieveNetworkInfo) {
  static const char kName[] = "Default Network";
  const uint32 kIdentifier = 0xabcdef;
  static const char kNetworkId[] = "00abcdef";
  EXPECT_CALL(*network_proxy_, Name(_)).WillOnce(Return(kName));
  EXPECT_CALL(*network_proxy_, Identifier(_)).WillOnce(Return(kIdentifier));
  provider_.RetrieveNetworkInfo(GetTestNetworkPath(kIdentifier));
  EXPECT_EQ(1, provider_.networks_.size());
  EXPECT_TRUE(ContainsKey(provider_.networks_,
                          GetTestNetworkPath(kIdentifier)));
  EXPECT_EQ(kName,
            provider_.networks_[GetTestNetworkPath(kIdentifier)].name);
  EXPECT_EQ(kNetworkId,
            provider_.networks_[GetTestNetworkPath(kIdentifier)].id);
  provider_.RetrieveNetworkInfo(GetTestNetworkPath(kIdentifier));
  EXPECT_EQ(1, provider_.networks_.size());
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
  provider_.services_[service->GetStorageIdentifier()] = service;
  EXPECT_EQ(service.get(),
            provider_.FindService(
                WiMaxService::CreateStorageIdentifier(kNetworkId,
                                                      kName)).get());
  EXPECT_FALSE(provider_.FindService("some_storage_id"));
}

TEST_F(WiMaxProviderTest, StartLiveServices) {
  const uint32 kIdentifier = 0x1234567;
  static const char kNetworkId[] = "01234567";
  static const char kName[] = "Some WiMAX Provider";
  vector<scoped_refptr<MockWiMaxService> > services(4);
  for (size_t i = 0; i < services.size(); i++) {
    services[i] = new MockWiMaxService(&control_, NULL, &metrics_, &manager_);
    if (i == 0) {
      services[0]->set_network_id("deadbeef");
    } else {
      services[i]->set_network_id(kNetworkId);
    }
    // Make services[3] the default service.
    if (i == 3) {
      services[i]->set_friendly_name(kName);
    } else {
      services[i]->set_friendly_name(
          base::StringPrintf("Configured %d", static_cast<int>(i)));
    }
    services[i]->InitStorageIdentifier();
    provider_.services_[services[i]->GetStorageIdentifier()] = services[i];
  }
  WiMaxProvider::NetworkInfo info;
  info.id = kNetworkId;
  info.name = kName;
  provider_.networks_[GetTestNetworkPath(kIdentifier)] = info;
  network_proxy_.reset();
  EXPECT_CALL(*services[0], IsStarted()).Times(0);
  EXPECT_CALL(*services[1], IsStarted()).WillOnce(Return(true));
  EXPECT_CALL(*services[1], Start(_)).Times(0);
  EXPECT_CALL(*services[2], IsStarted()).WillOnce(Return(false));
  EXPECT_CALL(*services[2], Start(_)).WillOnce(Return(true));
  EXPECT_CALL(*services[3], IsStarted()).WillOnce(Return(false));
  EXPECT_CALL(*services[3], Start(_)).WillOnce(Return(false));
  EXPECT_CALL(manager_, RegisterService(_)).Times(0);
  provider_.StartLiveServices();
  EXPECT_FALSE(services[0]->is_default());
  EXPECT_FALSE(services[1]->is_default());
  EXPECT_FALSE(services[2]->is_default());
  EXPECT_TRUE(services[3]->is_default());
}

TEST_F(WiMaxProviderTest, DestroyAllServices) {
  vector<scoped_refptr<MockWiMaxService> > services(2);
  for (size_t i = 0; i < services.size(); i++) {
    services[i] = new MockWiMaxService(&control_, NULL, &metrics_, &manager_);
    provider_.services_[services[i]->GetStorageIdentifier()] = services[i];
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
    if (i == 0) {
      EXPECT_CALL(*services[i], IsStarted()).WillOnce(Return(false));
      EXPECT_CALL(*services[i], GetNetworkObjectPath()).Times(0);
      EXPECT_CALL(*services[i], Stop()).Times(0);
    } else {
      EXPECT_CALL(*services[i], IsStarted()).WillOnce(Return(true));
      EXPECT_CALL(*services[i], GetNetworkObjectPath())
          .WillOnce(Return(GetTestNetworkPath(100 + i)));
    }
    provider_.services_[services[i]->GetStorageIdentifier()] = services[i];
  }
  services[3]->set_is_default(true);
  EXPECT_CALL(*services[1], Stop()).Times(0);
  EXPECT_CALL(*services[2], Stop());
  EXPECT_CALL(*services[3], Stop());
  EXPECT_CALL(manager_, DeregisterService(_));
  provider_.networks_[GetTestNetworkPath(777)].id = "01234567";
  provider_.networks_[GetTestNetworkPath(101)].id = "12345678";
  provider_.StopDeadServices();
  EXPECT_EQ(3, provider_.services_.size());
  EXPECT_FALSE(ContainsKey(provider_.services_,
                           services[3]->GetStorageIdentifier()));
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
  service0->InitStorageIdentifier();

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

  provider_.services_[service0->GetStorageIdentifier()] = service0;
  provider_.services_[service1->GetStorageIdentifier()] = service1;

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

  provider_.networks_["/org/chromium/foo"].id = "foo";
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
  provider_.services_[service0->GetStorageIdentifier()] = service0;
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
  EXPECT_FALSE(service->is_default());

  // Service already exists -- it was just created.
  EXPECT_CALL(manager_, RegisterService(_)).Times(0);
  WiMaxServiceRefPtr service1 = provider_.GetUniqueService(kNetworkId, kName1);
  ASSERT_TRUE(service1);
  EXPECT_EQ(service.get(), service1.get());
  EXPECT_EQ(2, provider_.services_.size());
  EXPECT_FALSE(service->is_default());
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
  WiMaxServiceRefPtr service = provider_.services_.begin()->second;
  EXPECT_EQ("wimax_network_01234567", service->GetStorageIdentifier());
  provider_.CreateServicesFromProfile(profile);
  ASSERT_EQ(1, provider_.services_.size());
  EXPECT_EQ(service.get(), provider_.services_.begin()->second);
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

TEST_F(WiMaxProviderTest, OnServiceUnloaded) {
  scoped_refptr<MockWiMaxService> service(
      new MockWiMaxService(&control_, NULL, &metrics_, &manager_));
  EXPECT_FALSE(service->is_default());
  scoped_refptr<MockWiMaxService> service_default(
      new MockWiMaxService(&control_, NULL, &metrics_, &manager_));
  service_default->set_is_default(true);
  provider_.services_[service->GetStorageIdentifier()] = service;
  provider_.services_[service_default->GetStorageIdentifier()] =
      service_default;
  EXPECT_CALL(manager_, DeregisterService(_)).Times(0);
  EXPECT_FALSE(provider_.OnServiceUnloaded(service_default));
  EXPECT_EQ(2, provider_.services_.size());
  EXPECT_TRUE(provider_.OnServiceUnloaded(service));
  EXPECT_EQ(1, provider_.services_.size());
  EXPECT_EQ(service_default.get(), provider_.services_.begin()->second.get());
}

}  // namespace shill
