// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wimax.h"

#include <string>
#include <vector>

#include <base/file_util.h>
#include <base/scoped_temp_dir.h>
#include <base/stringprintf.h>
#include <gtest/gtest.h>

#include "shill/event_dispatcher.h"
#include "shill/glib.h"
#include "shill/key_file_store.h"
#include "shill/mock_glib.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/mock_wimax_device_proxy.h"
#include "shill/mock_wimax_network_proxy.h"
#include "shill/mock_wimax_service.h"
#include "shill/nice_mock_control.h"
#include "shill/proxy_factory.h"

using std::string;
using std::vector;
using testing::_;
using testing::Return;

namespace shill {

namespace {

const char kTestLinkName[] = "wm0";
const char kTestAddress[] = "01:23:45:67:89:ab";
const int kTestInterfaceIndex = 5;
const char kTestPath[] = "/org/chromium/WiMaxManager/Device/6";

string GetTestNetworkPath(uint32 identifier) {
  return base::StringPrintf("%s%08x",
                            wimax_manager::kNetworkObjectPathPrefix,
                            identifier);
}

}  // namespace

class WiMaxTest : public testing::Test {
 public:
  WiMaxTest()
      : proxy_(new MockWiMaxDeviceProxy()),
        network_proxy_(new MockWiMaxNetworkProxy()),
        proxy_factory_(this),
        manager_(&control_, &dispatcher_, &metrics_, &glib_),
        device_(new WiMax(&control_, &dispatcher_, &metrics_, &manager_,
                          kTestLinkName, kTestAddress, kTestInterfaceIndex,
                          kTestPath)) {}

  virtual ~WiMaxTest() {}

 protected:
  class TestProxyFactory : public ProxyFactory {
   public:
    explicit TestProxyFactory(WiMaxTest *test) : test_(test) {}

    virtual WiMaxDeviceProxyInterface *CreateWiMaxDeviceProxy(
        const string &/*path*/) {
      return test_->proxy_.release();
    }

    virtual WiMaxNetworkProxyInterface *CreateWiMaxNetworkProxy(
        const string &/*path*/) {
      return test_->network_proxy_.release();
    }

   private:
    WiMaxTest *test_;

    DISALLOW_COPY_AND_ASSIGN(TestProxyFactory);
  };

  virtual void SetUp() {
    device_->proxy_factory_ = &proxy_factory_;
  }

  virtual void TearDown() {
    device_->services_.clear();
    device_->proxy_factory_ = NULL;
  }

  scoped_ptr<MockWiMaxDeviceProxy> proxy_;
  scoped_ptr<MockWiMaxNetworkProxy> network_proxy_;
  TestProxyFactory proxy_factory_;
  NiceMockControl control_;
  EventDispatcher dispatcher_;
  MockMetrics metrics_;
  MockGLib glib_;
  MockManager manager_;
  WiMaxRefPtr device_;
};

TEST_F(WiMaxTest, Constructor) {
  EXPECT_EQ(kTestPath, device_->path());
  EXPECT_FALSE(device_->scanning());
}

TEST_F(WiMaxTest, TechnologyIs) {
  EXPECT_TRUE(device_->TechnologyIs(Technology::kWiMax));
  EXPECT_FALSE(device_->TechnologyIs(Technology::kEthernet));
}

TEST_F(WiMaxTest, StartStop) {
  EXPECT_FALSE(device_->proxy_.get());
  EXPECT_CALL(*proxy_, Enable(_, _, _));
  EXPECT_CALL(*proxy_, set_networks_changed_callback(_));
  EXPECT_CALL(*proxy_, Disable(_, _, _));
  device_->Start(NULL, EnabledStateChangedCallback());
  EXPECT_TRUE(device_->proxy_.get());

  device_->networks_.push_back("path");
  scoped_refptr<MockWiMaxService> service(
      new MockWiMaxService(&control_, NULL, &metrics_, &manager_, NULL));
  device_->services_.push_back(service);
  EXPECT_CALL(*service, IsStarted()).WillOnce(Return(true));
  EXPECT_CALL(*service, GetNetworkObjectPath()).WillOnce(Return("path"));
  EXPECT_CALL(*service, Stop());
  EXPECT_CALL(manager_, DeregisterService(_));
  device_->Stop(NULL, EnabledStateChangedCallback());
  EXPECT_TRUE(device_->networks_.empty());
  EXPECT_TRUE(device_->services_.empty());
}

TEST_F(WiMaxTest, FindService) {
  EXPECT_FALSE(device_->FindService("some_storage_id"));
  scoped_refptr<MockWiMaxService> service(
      new MockWiMaxService(&control_, NULL, &metrics_, &manager_, NULL));
  static const char kName[] = "WiMAX Network";
  static const char kNetworkId[] = "76543210";
  service->set_friendly_name(kName);
  service->set_network_id(kNetworkId);
  service->InitStorageIdentifier();
  device_->services_.push_back(
      new MockWiMaxService(&control_, NULL, &metrics_, &manager_, NULL));
  device_->services_.push_back(service);
  device_->services_.push_back(
      new MockWiMaxService(&control_, NULL, &metrics_, &manager_, NULL));
  EXPECT_EQ(service.get(),
            device_->FindService(
                WiMaxService::CreateStorageIdentifier(kNetworkId,
                                                      kName)).get());
  EXPECT_FALSE(device_->FindService("some_storage_id"));
}

TEST_F(WiMaxTest, StartLiveServicesForNetwork) {
  const uint32 kIdentifier = 0x1234567;
  static const char kNetworkId[] = "01234567";
  static const char kName[] = "Some WiMAX Provider";
  vector<scoped_refptr<MockWiMaxService> > services(4);
  for (size_t i = 0; i < services.size(); i++) {
    services[i] =
        new MockWiMaxService(&control_, NULL, &metrics_, &manager_, NULL);
    services[i]->set_network_id(kNetworkId);
    device_->services_.push_back(services[i]);
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
  device_->StartLiveServicesForNetwork(GetTestNetworkPath(kIdentifier));
}

TEST_F(WiMaxTest, DestroyAllServices) {
  device_->services_.push_back(
      new MockWiMaxService(&control_, NULL, &metrics_, &manager_, NULL));
  device_->services_.push_back(
      new MockWiMaxService(&control_, NULL, &metrics_, &manager_, NULL));
  EXPECT_CALL(manager_, DeregisterService(_)).Times(2);
  device_->DestroyAllServices();
  EXPECT_TRUE(device_->services_.empty());
}

TEST_F(WiMaxTest, StopDeadServices) {
  vector<scoped_refptr<MockWiMaxService> > services(4);
  for (size_t i = 0; i < services.size(); i++) {
    services[i] =
        new MockWiMaxService(&control_, NULL, &metrics_, &manager_, NULL);
    if (i > 0) {
      EXPECT_CALL(*services[i], GetNetworkObjectPath())
          .WillRepeatedly(Return(GetTestNetworkPath(100 + i)));
    } else {
      EXPECT_CALL(*services[i], GetNetworkObjectPath()).Times(0);
      EXPECT_CALL(*services[i], IsStarted()).WillRepeatedly(Return(false));
      EXPECT_CALL(*services[i], Stop()).Times(0);
    }
    device_->services_.push_back(services[i]);
  }

  device_->SelectService(services[1]);
  device_->pending_service_ = services[3];

  EXPECT_CALL(*services[1], IsStarted()).WillOnce(Return(true));
  EXPECT_CALL(*services[2], IsStarted()).WillOnce(Return(true));
  EXPECT_CALL(*services[2], Stop());
  EXPECT_CALL(*services[3], IsStarted()).WillOnce(Return(true));
  device_->networks_.push_back(GetTestNetworkPath(777));
  device_->networks_.push_back(GetTestNetworkPath(101));
  device_->networks_.push_back(GetTestNetworkPath(103));
  device_->StopDeadServices();
  EXPECT_TRUE(device_->selected_service());
  EXPECT_TRUE(device_->pending_service_);

  EXPECT_CALL(*services[1], IsStarted()).WillOnce(Return(true));
  EXPECT_CALL(*services[2], IsStarted()).WillOnce(Return(false));
  EXPECT_CALL(*services[2], Stop()).Times(0);
  EXPECT_CALL(*services[3], IsStarted()).WillOnce(Return(true));
  EXPECT_CALL(*services[3], Stop());
  device_->networks_.pop_back();
  device_->StopDeadServices();
  EXPECT_TRUE(device_->selected_service());
  EXPECT_FALSE(device_->pending_service_);

  EXPECT_CALL(*services[1], IsStarted()).WillOnce(Return(true));
  EXPECT_CALL(*services[1], Stop());
  EXPECT_CALL(*services[2], IsStarted()).WillOnce(Return(false));
  EXPECT_CALL(*services[2], Stop()).Times(0);
  EXPECT_CALL(*services[3], IsStarted()).WillOnce(Return(false));
  EXPECT_CALL(*services[3], Stop()).Times(0);
  device_->networks_.pop_back();
  device_->StopDeadServices();
  EXPECT_FALSE(device_->selected_service());

  EXPECT_EQ(4, device_->services_.size());
}

TEST_F(WiMaxTest, OnNetworksChanged) {
  static const char kName[] = "Default Network";
  const uint32 kIdentifier = 0xabcdef;
  static const char kNetworkId[] = "00abcdef";

  // Started service to be stopped.
  scoped_refptr<MockWiMaxService> service0(
      new MockWiMaxService(&control_, NULL, &metrics_, &manager_, NULL));
  EXPECT_CALL(*service0, IsStarted()).WillOnce(Return(true));
  EXPECT_CALL(*service0, GetNetworkObjectPath())
      .WillOnce(Return(GetTestNetworkPath(100)));
  EXPECT_CALL(*service0, Start(_)).Times(0);
  EXPECT_CALL(*service0, Stop()).Times(1);
  service0->set_network_id("1234");

  // Stopped service to be started.
  scoped_refptr<MockWiMaxService> service1(
      new MockWiMaxService(&control_, NULL, &metrics_, &manager_, NULL));
  EXPECT_CALL(*service1, IsStarted()).Times(2).WillRepeatedly(Return(false));
  EXPECT_CALL(*service1, Start(_)).WillOnce(Return(true));
  EXPECT_CALL(*service1, Stop()).Times(0);
  service1->set_network_id(kNetworkId);
  service1->set_friendly_name(kName);
  service1->InitStorageIdentifier();
  EXPECT_CALL(*network_proxy_, Name(_)).WillOnce(Return(kName));
  EXPECT_CALL(*network_proxy_, Identifier(_)).WillOnce(Return(kIdentifier));

  device_->services_.push_back(service0);
  device_->services_.push_back(service1);

  EXPECT_CALL(manager_, RegisterService(_)).Times(0);
  EXPECT_CALL(manager_, DeregisterService(_)).Times(0);

  RpcIdentifiers networks;
  networks.push_back(GetTestNetworkPath(101));
  device_->OnNetworksChanged(networks);
  EXPECT_TRUE(networks == device_->networks_);
}

TEST_F(WiMaxTest, GetService) {
  EXPECT_TRUE(device_->services_.empty());

  static const char kName0[] = "Test WiMAX Network";
  static const char kName1[] = "Unknown Network";
  static const char kNetworkId[] = "12340000";

  // Service already exists.
  scoped_refptr<MockWiMaxService> service0(
      new MockWiMaxService(&control_, NULL, &metrics_, &manager_, NULL));
  service0->set_network_id(kNetworkId);
  service0->set_friendly_name(kName0);
  service0->InitStorageIdentifier();
  device_->services_.push_back(service0);
  EXPECT_CALL(manager_, RegisterService(_)).Times(0);
  WiMaxServiceRefPtr service = device_->GetService(kNetworkId, kName0);
  ASSERT_TRUE(service);
  EXPECT_EQ(service0.get(), service.get());
  EXPECT_EQ(1, device_->services_.size());

  // Create a new service.
  EXPECT_CALL(manager_, RegisterService(_));
  service = device_->GetService(kNetworkId, kName1);
  ASSERT_TRUE(service);
  EXPECT_NE(service0.get(), service.get());
  EXPECT_EQ(2, device_->services_.size());
  EXPECT_EQ(WiMaxService::CreateStorageIdentifier(kNetworkId, kName1),
            service->GetStorageIdentifier());

  // Service already exists -- it was just created.
  EXPECT_CALL(manager_, RegisterService(_)).Times(0);
  WiMaxServiceRefPtr service1 = device_->GetService(kNetworkId, kName1);
  ASSERT_TRUE(service1);
  EXPECT_EQ(service.get(), service1.get());
  EXPECT_EQ(2, device_->services_.size());
}

TEST_F(WiMaxTest, GetDefaultService) {
  EXPECT_TRUE(device_->services_.empty());

  static const char kName[] = "Default Network";
  const uint32 kIdentifier = 0xabcdef;
  static const char kNetworkId[] = "00abcdef";

  // Ensure that a service is created and registered if it doesn't exist
  // already.
  EXPECT_CALL(*network_proxy_, Name(_)).WillOnce(Return(kName));
  EXPECT_CALL(*network_proxy_, Identifier(_)).WillOnce(Return(kIdentifier));
  EXPECT_CALL(manager_, RegisterService(_));
  WiMaxServiceRefPtr service =
      device_->GetDefaultService(GetTestNetworkPath(kIdentifier));
  ASSERT_TRUE(service);
  EXPECT_EQ(1, device_->services_.size());
  EXPECT_EQ(WiMaxService::CreateStorageIdentifier(kNetworkId, kName),
            service->GetStorageIdentifier());

  // Ensure that no duplicate service is created or registered.
  ASSERT_FALSE(network_proxy_.get());
  network_proxy_.reset(new MockWiMaxNetworkProxy());
  EXPECT_CALL(*network_proxy_, Name(_)).WillOnce(Return(kName));
  EXPECT_CALL(*network_proxy_, Identifier(_)).WillOnce(Return(kIdentifier));
  EXPECT_CALL(manager_, RegisterService(_)).Times(0);
  WiMaxServiceRefPtr service2 =
      device_->GetDefaultService(GetTestNetworkPath(kIdentifier));
  ASSERT_TRUE(service2);
  EXPECT_EQ(1, device_->services_.size());
  EXPECT_EQ(service.get(), service2.get());
}

TEST_F(WiMaxTest, LoadServices) {
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
  EXPECT_CALL(manager_, RegisterService(_)).Times(1);
  EXPECT_TRUE(device_->LoadServices(&store));
  EXPECT_FALSE(device_->LoadServices(&store));
  EXPECT_EQ(1, device_->services_.size());
}

}  // namespace shill
