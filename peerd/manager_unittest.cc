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

using brillo::dbus_utils::DBusObject;
using dbus::MockBus;
using dbus::ObjectPath;
using peerd::constants::kSerbusServiceId;
using std::string;
using std::unique_ptr;
using testing::AnyNumber;
using testing::Mock;
using testing::Return;
using testing::_;

namespace peerd {
namespace {

// Chrome doesn't bother mocking out their objects completely.
class EspeciallyMockedBus : public dbus::MockBus {
 public:
  using dbus::MockBus::MockBus;

  MOCK_METHOD2(GetServiceOwner,
               void(const std::string& service_name,
                    const GetServiceOwnerCallback& callback));

  MOCK_METHOD2(ListenForServiceOwnerChange,
               void(const std::string& service_name,
                    const GetServiceOwnerCallback& callback));

  MOCK_METHOD2(UnlistenForServiceOwnerChange,
               void(const std::string& service_name,
                    const GetServiceOwnerCallback& callback));

 protected:
  ~EspeciallyMockedBus() override = default;
};

}  // namespace

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
    manager_.reset(new Manager{scoped_refptr<dbus::Bus>{mock_bus_.get()},
                               std::move(dbus_object),
                               unique_ptr<PublishedPeer>{peer_},
                               unique_ptr<PeerManagerInterface>{peer_manager_},
                               unique_ptr<AvahiClient>{avahi_client_},
                               ""});
  }

  std::vector<string> GetMonitoredTechnologies() {
    return manager_->dbus_adaptor_.GetMonitoredTechnologies();
  }

  const std::map<std::string, brillo::Any> kNoOptions;
  const std::vector<std::string> kOnlyMdns{technologies::kMDNSText};
  scoped_refptr<EspeciallyMockedBus>
      mock_bus_{new EspeciallyMockedBus{dbus::Bus::Options{}}};
  unique_ptr<Manager> manager_;
  MockPublishedPeer* peer_;  // manager_ owns this object.
  MockPeerManager* peer_manager_;  // manager_ owns this object.
  MockAvahiClient* avahi_client_;  // manager_ owns this object.
};

TEST_F(ManagerTest, ShouldRejectSerbusServiceId) {
  brillo::ErrorPtr error;
  dbus::MethodCall method_call{"org.chromium.peerd.Manager", "ExposeService"};
  EXPECT_FALSE(manager_->ExposeService(
      &error, &method_call, kSerbusServiceId, {}, {}));
  EXPECT_NE(nullptr, error.get());
}

TEST_F(ManagerTest, StartMonitoring_ShouldRejectEmptyTechnologies) {
  brillo::ErrorPtr error;
  string token;
  EXPECT_FALSE(manager_->StartMonitoring(&error, {}, kNoOptions, &token));
  EXPECT_NE(nullptr, error.get());
}

TEST_F(ManagerTest, StartMonitoring_ShouldRejectInvalidTechnologies) {
  brillo::ErrorPtr error;
  string token;
  EXPECT_FALSE(manager_->StartMonitoring(
      &error, {"hot air balloon"}, kNoOptions, &token));
  EXPECT_NE(nullptr, error.get());
  EXPECT_EQ(GetMonitoredTechnologies(), std::vector<string>{});
}

TEST_F(ManagerTest, StartMonitoring_ShouldAcceptMDNS) {
  brillo::ErrorPtr error;
  EXPECT_CALL(*avahi_client_, StartMonitoring());
  string token;
  EXPECT_TRUE(manager_->StartMonitoring(
      &error, {technologies::kMDNSText}, kNoOptions, &token));
  EXPECT_TRUE(!token.empty());
  EXPECT_EQ(nullptr, error.get());
  EXPECT_EQ(GetMonitoredTechnologies(), kOnlyMdns);
}

TEST_F(ManagerTest, StopMonitoring_HandlesInvalidToken) {
  brillo::ErrorPtr error;
  EXPECT_FALSE(manager_->StopMonitoring(&error, "invalid_token"));
  EXPECT_NE(nullptr, error.get());
}

TEST_F(ManagerTest, StopMonitoring_HandlesSingleSubscription) {
  brillo::ErrorPtr error;
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
  brillo::ErrorPtr error;
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

TEST_F(ManagerTest, ShouldRemoveServicesOnRemoteDeath) {
  const string kServiceId{"a_service_id"};
  const string kDBusSender{"a.dbus.connection.id"};
  EXPECT_CALL(*peer_, AddPublishedService(_, kServiceId, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_bus_, ListenForServiceOwnerChange(kDBusSender, _));
  EXPECT_CALL(*mock_bus_, GetServiceOwner(kDBusSender, _));
  dbus::MethodCall method_call{"org.chromium.peerd.Manager", "ExposeService"};
  method_call.SetSender(kDBusSender);
  EXPECT_TRUE(manager_->ExposeService(
      nullptr, &method_call, kServiceId, {}, {}));
  Mock::VerifyAndClearExpectations(peer_);
  Mock::VerifyAndClearExpectations(mock_bus_.get());
  EXPECT_CALL(*peer_, RemoveService(_, kServiceId));
  EXPECT_CALL(*mock_bus_, UnlistenForServiceOwnerChange(kDBusSender, _));
  manager_->OnDBusServiceDeath(kServiceId);
}

}  // namespace peerd
