// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/manager.h"

#include <string>

#include <dbus/bus.h>
#include <dbus/mock_bus.h>
#include <dbus/object_path.h>

#include "peerd/constants.h"
#include "peerd/mock_avahi_client.h"
#include "peerd/mock_peer_manager.h"
#include "peerd/mock_published_peer.h"
#include "peerd/service.h"

using chromeos::dbus_utils::DBusObject;
using dbus::MockBus;
using dbus::ObjectPath;
using peerd::constants::kSerbusServiceId;
using std::string;
using std::unique_ptr;
using testing::AnyNumber;
using testing::Mock;

namespace peerd {

class ManagerTest : public testing::Test {
 public:
  void SetUp() override {
    unique_ptr<DBusObject> dbus_object{new DBusObject{
        nullptr,
        mock_bus_,
        ObjectPath{"/manager/object/path"}}};
    // Ignore threading concerns.
    EXPECT_CALL(*mock_bus_, AssertOnOriginThread()).Times(AnyNumber());
    EXPECT_CALL(*mock_bus_, AssertOnDBusThread()).Times(AnyNumber());
    peer_ = new MockPublishedPeer{mock_bus_, ObjectPath{"/peer/prefix"}};
    peer_manager_ = new MockPeerManager();
    avahi_client_ = new MockAvahiClient(mock_bus_, peer_manager_);
    manager_.reset(new Manager{std::move(dbus_object),
                               unique_ptr<PublishedPeer>{peer_},
                               unique_ptr<PeerManagerInterface>{peer_manager_},
                               unique_ptr<AvahiClient>{avahi_client_},
                               ""});
  }

  std::vector<string> GetMonitoredTechnologies() {
    return manager_->dbus_adaptor_.GetMonitoredTechnologies();
  }

  const std::map<std::string, chromeos::Any> kNoOptions;
  const std::vector<std::string> kOnlyMdns{technologies::kMDNSText};
  scoped_refptr<MockBus> mock_bus_{new MockBus{dbus::Bus::Options{}}};
  unique_ptr<Manager> manager_;
  MockPublishedPeer* peer_;  // manager_ owns this object.
  MockPeerManager* peer_manager_;  // manager_ owns this object.
  MockAvahiClient* avahi_client_;  // manager_ owns this object.
};

TEST_F(ManagerTest, ShouldRejectSerbusServiceId) {
  chromeos::ErrorPtr error;
  string service_token;
  EXPECT_FALSE(manager_->ExposeServiceImpl(&error, "", kSerbusServiceId, {}, {},
               &service_token));
  EXPECT_TRUE(service_token.empty());
  EXPECT_NE(nullptr, error.get());
}

TEST_F(ManagerTest, StartMonitoring_ShouldRejectEmptyTechnologies) {
  chromeos::ErrorPtr error;
  string token;
  EXPECT_FALSE(manager_->StartMonitoring(&error, {}, kNoOptions, &token));
  EXPECT_NE(nullptr, error.get());
}

TEST_F(ManagerTest, StartMonitoring_ShouldRejectInvalidTechnologies) {
  chromeos::ErrorPtr error;
  string token;
  EXPECT_FALSE(manager_->StartMonitoring(
        &error, {"hot air balloon"}, kNoOptions, &token));
  EXPECT_NE(nullptr, error.get());
  EXPECT_EQ(GetMonitoredTechnologies(), std::vector<string>{});
}

TEST_F(ManagerTest, StartMonitoring_ShouldAcceptMDNS) {
  chromeos::ErrorPtr error;
  EXPECT_CALL(*avahi_client_, StartMonitoring());
  string token;
  EXPECT_TRUE(manager_->StartMonitoring(
        &error, {technologies::kMDNSText}, kNoOptions, &token));
  EXPECT_TRUE(!token.empty());
  EXPECT_EQ(nullptr, error.get());
  EXPECT_EQ(GetMonitoredTechnologies(), kOnlyMdns);
}

TEST_F(ManagerTest, StopMonitoring_HandlesInvalidToken) {
  chromeos::ErrorPtr error;
  EXPECT_FALSE(manager_->StopMonitoring(&error, "invalid_token"));
  EXPECT_NE(nullptr, error.get());
}

TEST_F(ManagerTest, StopMonitoring_HandlesSingleSubscription) {
  chromeos::ErrorPtr error;
  EXPECT_CALL(*avahi_client_, StartMonitoring());
  string token;
  EXPECT_TRUE(manager_->StartMonitoring(
      &error, {technologies::kMDNSText}, kNoOptions, &token));
  EXPECT_TRUE(!token.empty());
  EXPECT_EQ(nullptr, error.get());
  EXPECT_CALL(*avahi_client_, StopMonitoring());
  manager_->StopMonitoring(&error, token);
  EXPECT_FALSE(error.get());
  // Unsubscribing a second time is invalid.
  manager_->StopMonitoring(&error, token);
  EXPECT_NE(nullptr, error.get());
}

TEST_F(ManagerTest, StopMonitoring_HandlesMultipleSubscriptions) {
  chromeos::ErrorPtr error;
  // We don't bother to figure out if we're already monitoring on a technology.
  EXPECT_CALL(*avahi_client_, StartMonitoring()).Times(2);
  string token1;
  string token2;
  EXPECT_TRUE(manager_->StartMonitoring(
      &error, {technologies::kMDNSText}, kNoOptions, &token1));
  EXPECT_TRUE(manager_->StartMonitoring(
      &error, {technologies::kMDNSText}, kNoOptions, &token2));
  EXPECT_TRUE(!token1.empty());
  EXPECT_TRUE(!token2.empty());
  EXPECT_EQ(nullptr, error.get());
  EXPECT_EQ(GetMonitoredTechnologies(), kOnlyMdns);
  Mock::VerifyAndClearExpectations(avahi_client_);
  // Now, remove one subscription, we should keep monitoring.
  EXPECT_CALL(*avahi_client_, StopMonitoring()).Times(0);
  manager_->StopMonitoring(&error, token1);
  EXPECT_EQ(nullptr, error.get());
  Mock::VerifyAndClearExpectations(avahi_client_);
  EXPECT_CALL(*avahi_client_, StopMonitoring());
  manager_->StopMonitoring(&error, token2);
}

}  // namespace peerd
