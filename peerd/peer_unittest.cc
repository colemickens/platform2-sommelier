// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/peer.h"

#include <string>

#include <chromeos/dbus/mock_dbus_object.h>
#include <dbus/mock_bus.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "peerd/test_util.h"

using chromeos::dbus_utils::DBusObject;
using chromeos::dbus_utils::MockDBusObject;
using dbus::Bus;
using dbus::MockBus;
using dbus::ObjectPath;
using peerd::Peer;
using peerd::peer_codes::kInvalidName;
using peerd::peer_codes::kInvalidNote;
using peerd::peer_codes::kInvalidUUID;
using peerd::test_util::MakeMockCompletionAction;
using peerd::test_util::MakeMockDBusObject;
using std::string;
using std::unique_ptr;
using testing::_;

namespace {

const char kUUID[] = "123e4567-e89b-12d3-a456-426655440000";
const char kValidName[] = "NAME";
const char kValidNote[] = "notes are long and very descriptive for people.";

}  // namespace

namespace peerd {

class PeerTest : public ::testing::Test {
 public:
  unique_ptr<Peer> MakePeer() {
    auto dbus_object = MakeMockDBusObject();
    chromeos::ErrorPtr error;
    EXPECT_CALL(*dbus_object, RegisterAsync(_)).Times(1);
    auto peer = Peer::MakePeerImpl(&error,
                                   std::move(dbus_object),
                                   kUUID,
                                   kValidName,
                                   kValidNote,
                                   0,
                                   MakeMockCompletionAction());
    EXPECT_NE(nullptr, peer.get());
    EXPECT_EQ(nullptr, error.get());
    return peer;
  }

  void TestBadUUID(const string& uuid) {
    auto dbus_object = MakeMockDBusObject();
    EXPECT_CALL(*dbus_object, RegisterAsync(_)).Times(0);
    chromeos::ErrorPtr error;
    auto peer = Peer::MakePeerImpl(&error,
                                   std::move(dbus_object),
                                   uuid,
                                   kValidName,
                                   kValidNote,
                                   0,
                                   MakeMockCompletionAction());
    ASSERT_NE(nullptr, error.get());
    EXPECT_TRUE(error->HasError(peerd::kPeerdErrorDomain,
                                peerd::peer_codes::kInvalidUUID));
    EXPECT_EQ(nullptr, peer.get());
  }
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

TEST_F(PeerTest, ShouldRejectBadNameInFactory) {
  auto dbus_object = MakeMockDBusObject();
  EXPECT_CALL(*dbus_object, RegisterAsync(_)).Times(0);
  chromeos::ErrorPtr error;
  auto peer = Peer::MakePeerImpl(&error,
                                 std::move(dbus_object),
                                 kUUID,
                                 "* is not allowed",
                                 kValidNote,
                                 0,
                                 MakeMockCompletionAction());
  ASSERT_NE(nullptr, error.get());
  EXPECT_TRUE(error->HasError(kPeerdErrorDomain, kInvalidName));
  EXPECT_EQ(nullptr, peer.get());
}

TEST_F(PeerTest, ShouldRejectBadNoteInFactory) {
  auto dbus_object = MakeMockDBusObject();
  EXPECT_CALL(*dbus_object, RegisterAsync(_)).Times(0);
  chromeos::ErrorPtr error;
  auto peer = Peer::MakePeerImpl(&error,
                                 std::move(dbus_object),
                                 kUUID,
                                 kValidName,
                                 "notes also may not contain *",
                                 0,
                                 MakeMockCompletionAction());
  ASSERT_NE(nullptr, error.get());
  EXPECT_TRUE(error->HasError(kPeerdErrorDomain, kInvalidNote));
  EXPECT_EQ(nullptr, peer.get());
}

TEST_F(PeerTest, ShouldRegisterWithDBus) {
  auto peer = MakePeer();
}

TEST_F(PeerTest, ShouldRejectNameTooLong) {
  auto peer = MakePeer();
  chromeos::ErrorPtr error;
  peer->SetFriendlyName(&error, string(33, 'a'));
  ASSERT_NE(nullptr, error.get());
  EXPECT_TRUE(error->HasError(kPeerdErrorDomain, kInvalidName));
}

TEST_F(PeerTest, ShouldRejectNameInvalidChars) {
  auto peer = MakePeer();
  chromeos::ErrorPtr error;
  peer->SetFriendlyName(&error, "* is not allowed");
  ASSERT_NE(nullptr, error.get());
  EXPECT_TRUE(error->HasError(kPeerdErrorDomain, kInvalidName));
}

TEST_F(PeerTest, ShouldRejectNoteTooLong) {
  auto peer = MakePeer();
  chromeos::ErrorPtr error;
  peer->SetNote(&error, string(256, 'a'));
  ASSERT_NE(nullptr, error.get());
  EXPECT_TRUE(error->HasError(kPeerdErrorDomain, kInvalidNote));
}

TEST_F(PeerTest, ShouldRejectNoteInvalidChars) {
  auto peer = MakePeer();
  chromeos::ErrorPtr error;
  peer->SetNote(&error, "* is also not allowed in notes");
  ASSERT_NE(nullptr, error.get());
  EXPECT_TRUE(error->HasError(kPeerdErrorDomain, kInvalidNote));
}

}  // namespace peerd
