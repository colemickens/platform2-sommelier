// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/peer.h"

#include <memory>
#include <string>

#include <base/time/time.h>
#include <brillo/dbus/mock_dbus_object.h>
#include <brillo/errors/error.h>
#include <dbus/mock_bus.h>
#include <dbus/mock_exported_object.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "peerd/service.h"
#include "peerd/test_util.h"

using brillo::ErrorPtr;
using brillo::dbus_utils::DBusObject;
using brillo::dbus_utils::ExportedObjectManager;
using dbus::Bus;
using dbus::MockBus;
using dbus::MockExportedObject;
using dbus::ObjectPath;
using peerd::Peer;
using peerd::errors::peer::kDuplicateServiceID;
using peerd::errors::peer::kInvalidTime;
using peerd::errors::peer::kInvalidUUID;
using peerd::test_util::MakeMockCompletionAction;
using std::string;
using std::unique_ptr;
using testing::AnyNumber;
using testing::Invoke;
using testing::Property;
using testing::Return;
using testing::StartsWith;
using testing::_;

namespace {

const char kPeerPath[] = "/some/path/ending/with";
const char kServicePathPrefix[] = "/some/path/ending/with/services/";
const char kServicePath[] = "/some/path/ending/with/services/1";

const char kUUID[] = "123e4567-e89b-12d3-a456-426655440000";

}  // namespace

namespace peerd {

class PeerTest : public ::testing::Test {
 public:
  virtual void SetUp() {
    // Ignore threading concerns.
    EXPECT_CALL(*mock_bus_, AssertOnOriginThread()).Times(AnyNumber());
    EXPECT_CALL(*mock_bus_, AssertOnDBusThread()).Times(AnyNumber());
    // Return appropriate objects when asked.
    EXPECT_CALL(*mock_bus_,
                GetExportedObject(Property(&ObjectPath::value,
                                           kPeerPath)))
        .WillRepeatedly(Return(peer_object_.get()));
    EXPECT_CALL(*mock_bus_,
                GetExportedObject(Property(&ObjectPath::value,
                                           StartsWith(kServicePathPrefix))))
        .WillRepeatedly(Return(service_object_.get()));
    // Just immediately call callbacks on ExportMethod calls.
    EXPECT_CALL(*peer_object_, ExportMethod(_, _, _, _))
        .WillRepeatedly(Invoke(&test_util::HandleMethodExport));
    EXPECT_CALL(*service_object_, ExportMethod(_, _, _, _))
        .WillRepeatedly(Invoke(&test_util::HandleMethodExport));
    // Ignore Unregister calls.
    EXPECT_CALL(*peer_object_, Unregister()).Times(AnyNumber());
    EXPECT_CALL(*service_object_, Unregister()).Times(AnyNumber());
  }

  unique_ptr<Peer> MakePeer() {
    unique_ptr<Peer> peer{new Peer{mock_bus_, nullptr, ObjectPath{kPeerPath}}};
    brillo::ErrorPtr error;
    EXPECT_TRUE(peer->RegisterAsync(
        &error,
        kUUID,
        base::Time::UnixEpoch() + base::TimeDelta::FromDays(1),
        MakeMockCompletionAction()));
    EXPECT_EQ(nullptr, error.get());
    return peer;
  }

  void AssertBadFactoryArgs(const string& uuid, const string& error_code) {
    brillo::ErrorPtr error;
    unique_ptr<Peer> peer{
        new Peer{mock_bus_, nullptr, ObjectPath{kPeerPath}}};
    EXPECT_FALSE(peer->RegisterAsync(
        &error,
        uuid,
        base::Time::UnixEpoch(),
        MakeMockCompletionAction()));
    ASSERT_NE(nullptr, error.get());
    EXPECT_TRUE(error->HasError(peerd::kPeerdErrorDomain, error_code));
  }

  void TestBadUUID(const string& uuid) {
    AssertBadFactoryArgs(uuid, kInvalidUUID);
  }

  scoped_refptr<MockBus> mock_bus_{new MockBus{Bus::Options{}}};
  scoped_refptr<dbus::MockExportedObject> peer_object_{
      new MockExportedObject{mock_bus_.get(), ObjectPath{kPeerPath}}};
  scoped_refptr<dbus::MockExportedObject> service_object_{
      new MockExportedObject{mock_bus_.get(), ObjectPath{kServicePath}}};
};

TEST_F(PeerTest, ShouldRejectObviouslyNotUUID) {
  TestBadUUID("definitely a bad UUID");
}

TEST_F(PeerTest, ShouldRejectWrongUUIDLen) {
  TestBadUUID(string(kUUID, strlen(kUUID)- 1));
}

TEST_F(PeerTest, ShouldRejectBadDashInUUID) {
  string bad_uuid(kUUID);
  bad_uuid[23] = 'a';  // Index 23 is the final -.
  TestBadUUID(bad_uuid);
}

TEST_F(PeerTest, ShouldRejectBadCharacterInUUID) {
  string bad_uuid(kUUID);
  bad_uuid[bad_uuid.length() - 1] = '-';
  TestBadUUID(bad_uuid);
}

TEST_F(PeerTest, ShouldRegisterWithDBus) {
  auto peer = MakePeer();
  EXPECT_EQ(peer->GetUUID(), kUUID);
}

TEST_F(PeerTest, ShouldRejectStaleUpdate) {
  auto peer = MakePeer();
  brillo::ErrorPtr error;
  EXPECT_FALSE(peer->SetLastSeen(&error, base::Time::UnixEpoch()));
  ASSERT_NE(nullptr, error.get());
  EXPECT_TRUE(error->HasError(kPeerdErrorDomain, kInvalidTime));
}

TEST_F(PeerTest, ShouldRejectDuplicateServiceID) {
  auto peer = MakePeer();
  brillo::ErrorPtr error;
  const string service_id{"a-service"};
  EXPECT_TRUE(peer->AddService(&error, service_id, {}, {}, {}));
  EXPECT_FALSE(peer->AddService(&error, service_id, {}, {}, {}));
  EXPECT_TRUE(error->HasError(kPeerdErrorDomain, kDuplicateServiceID));
}

}  // namespace peerd
