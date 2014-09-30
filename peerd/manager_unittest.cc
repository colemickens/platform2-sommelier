// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/manager.h"

#include <string>

#include <dbus/bus.h>
#include <dbus/mock_bus.h>
#include <dbus/object_path.h>

#include "peerd/mock_avahi_client.h"
#include "peerd/mock_peer.h"
#include "peerd/service.h"

using chromeos::dbus_utils::DBusObject;
using dbus::MockBus;
using peerd::constants::kSerbusServiceId;
using std::unique_ptr;
using testing::AnyNumber;

namespace peerd {

class ManagerTest : public testing::Test {
 public:
  void SetUp() override {
    unique_ptr<DBusObject> dbus_object{new DBusObject{
        nullptr,
        mock_bus_,
        dbus::ObjectPath{"/manager/object/path"}}};
    // Ignore threading concerns.
    EXPECT_CALL(*mock_bus_, AssertOnOriginThread()).Times(AnyNumber());
    EXPECT_CALL(*mock_bus_, AssertOnDBusThread()).Times(AnyNumber());
    peer_ = new MockPeer("/peer/prefix", "wish-this-was-a-uuid");
    avahi_client_ = new MockAvahiClient();
    manager_.reset(new Manager{std::move(dbus_object),
                               unique_ptr<Peer>{peer_},
                               unique_ptr<AvahiClient>{avahi_client_}});
  }

  scoped_refptr<MockBus> mock_bus_{new MockBus{dbus::Bus::Options{}}};
  unique_ptr<Manager> manager_;
  MockAvahiClient* avahi_client_;  // manager_ owns this object.
  MockPeer* peer_;  // manager_ owns this object.
};

TEST_F(ManagerTest, ShouldRejectSerbusServiceId) {
  chromeos::ErrorPtr error;
  std::string service_token = manager_->ExposeService(
      &error, kSerbusServiceId, {});
  EXPECT_TRUE(service_token.empty());
  EXPECT_TRUE(error->GetFirstError() != nullptr);
}

}  // namespace peerd
