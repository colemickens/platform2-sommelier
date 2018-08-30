// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/peer_manager_impl.h"

#include <base/time/time.h>
#include <dbus/mock_bus.h>
#include <dbus/mock_exported_object.h>
#include <dbus/object_path.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "peerd/technologies.h"
#include "peerd/test_util.h"

using base::Time;
using dbus::ObjectPath;
using dbus::MockExportedObject;
using peerd::technologies::kBT;
using testing::AnyNumber;
using testing::Invoke;
using testing::Mock;
using testing::Property;
using testing::Return;
using testing::_;

namespace {

const char kPeerPath[] = "/org/chromium/peerd/peers/1";
const char kServicePath[] = "/org/chromium/peerd/peers/1/services/1";

const char kPeerId[] = "123e4567-e89b-12d3-a456-426655440000";
const time_t kPeerLastSeen = 1;
const char kServiceId[] = "a-service-id";
const char kBadServiceId[] = "#bad_service_id";
const time_t kServiceLastSeen = 2;

}  // namespace

namespace peerd {

class PeerManagerImplTest : public testing::Test {
 public:
  void SetUp() override {
    // Ignore threading concerns.
    EXPECT_CALL(*bus_, AssertOnOriginThread()).Times(AnyNumber());
    EXPECT_CALL(*bus_, AssertOnDBusThread()).Times(AnyNumber());
    // By default, don't let anyone have anyone export anything.
    EXPECT_CALL(*bus_, GetExportedObject(_)) .Times(0);
    EXPECT_CALL(*peer_object_, Unregister()).Times(0);
    EXPECT_CALL(*service_object_, Unregister()).Times(0);
    // Don't worry about signals.
    EXPECT_CALL(*peer_object_, SendSignal(_)).Times(AnyNumber());
    EXPECT_CALL(*service_object_, SendSignal(_)).Times(AnyNumber());
  }

  void TearDown() override {
    Mock::VerifyAndClearExpectations(bus_.get());
    Mock::VerifyAndClearExpectations(peer_object_.get());
    Mock::VerifyAndClearExpectations(service_object_.get());
    if (peer_exposed_) {
      ExpectPeerRemoved();
    }
    if (service_exposed_) {
      ExpectServiceRemoved();
    }
  }

  void ExpectPeerExposed() {
    peer_exposed_ = true;
    EXPECT_CALL(*bus_, GetExportedObject(Property(&ObjectPath::value,
                                                  kPeerPath)))
        .WillOnce(Return(peer_object_.get()));
    EXPECT_CALL(*peer_object_, ExportMethod(_, _, _, _))
        .WillRepeatedly(Invoke(&test_util::HandleMethodExport));
  }

  void ExpectPeerRemoved() {
    Mock::VerifyAndClearExpectations(bus_.get());
    Mock::VerifyAndClearExpectations(peer_object_.get());
    peer_exposed_ = false;
    EXPECT_CALL(*peer_object_, Unregister()).Times(1);
  }

  void ExpectServiceExposed() {
    service_exposed_ = true;
    EXPECT_CALL(*bus_, GetExportedObject(Property(&ObjectPath::value,
                                                  kServicePath)))
        .WillOnce(Return(service_object_.get()));
    EXPECT_CALL(*service_object_, ExportMethod(_, _, _, _))
        .WillRepeatedly(Invoke(&test_util::HandleMethodExport));
  }

  void ExpectServiceRemoved() {
    Mock::VerifyAndClearExpectations(bus_.get());
    Mock::VerifyAndClearExpectations(service_object_.get());
    service_exposed_ = false;
    EXPECT_CALL(*service_object_, Unregister()).Times(1);
  }

  scoped_refptr<dbus::MockBus> bus_{new dbus::MockBus{dbus::Bus::Options{}}};
  scoped_refptr<MockExportedObject> peer_object_{
      new MockExportedObject{bus_.get(), ObjectPath{kPeerPath}}};
  scoped_refptr<MockExportedObject> service_object_{
      new MockExportedObject{bus_.get(), ObjectPath{kServicePath}}};
  bool peer_exposed_{false};
  bool service_exposed_{false};
  PeerManagerImpl manager_{bus_, nullptr};
};

TEST_F(PeerManagerImplTest, CanDiscoverPeer) {
  ExpectPeerExposed();
  manager_.OnPeerDiscovered(kPeerId,
                            Time::FromTimeT(kPeerLastSeen), kBT);
}

TEST_F(PeerManagerImplTest, CanDiscoverServiceOnPeer) {
  ExpectPeerExposed();
  manager_.OnPeerDiscovered(kPeerId,
                            Time::FromTimeT(kPeerLastSeen), kBT);
  ExpectServiceExposed();
  manager_.OnServiceDiscovered(kPeerId, kServiceId, {}, {},
                               Time::FromTimeT(kServiceLastSeen), kBT);
}

TEST_F(PeerManagerImplTest, CannotDiscoverServiceWithoutPeer) {
  manager_.OnServiceDiscovered(kPeerId, kServiceId, {}, {},
                               Time::FromTimeT(kServiceLastSeen), kBT);
}

TEST_F(PeerManagerImplTest, CanForgetPeer) {
  ExpectPeerExposed();
  manager_.OnPeerDiscovered(kPeerId,
                            Time::FromTimeT(kPeerLastSeen), kBT);
  ExpectPeerRemoved();
  manager_.OnPeerRemoved(kPeerId, kBT);
}

TEST_F(PeerManagerImplTest, CanShutdownLoneTechnology) {
  ExpectPeerExposed();
  manager_.OnPeerDiscovered(kPeerId,
                            Time::FromTimeT(kPeerLastSeen), kBT);
  ExpectServiceExposed();
  manager_.OnServiceDiscovered(kPeerId, kServiceId, {}, {},
                               Time::FromTimeT(kServiceLastSeen), kBT);
  ExpectServiceRemoved();
  ExpectPeerRemoved();
  manager_.OnTechnologyShutdown(kBT);
}

TEST_F(PeerManagerImplTest, WillNotExposeCorruptService) {
  ExpectPeerExposed();
  manager_.OnPeerDiscovered(kPeerId,
                            Time::FromTimeT(kPeerLastSeen), kBT);
  manager_.OnServiceDiscovered(kPeerId, kBadServiceId, {}, {},
                               Time::FromTimeT(kServiceLastSeen), kBT);
}

}  // namespace peerd
