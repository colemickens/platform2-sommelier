// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/discovered_peer.h"

#include <string>

#include <base/time/time.h>
#include <dbus/bus.h>
#include <dbus/mock_bus.h>
#include <dbus/mock_exported_object.h>
#include <dbus/object_path.h>
#include <chromeos/dbus/async_event_sequencer.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "peerd/technologies.h"
#include "peerd/test_util.h"

using base::Time;
using chromeos::dbus_utils::AsyncEventSequencer;
using dbus::ObjectPath;
using dbus::MockExportedObject;
using peerd::technologies::tech_t;
using std::string;
using std::unique_ptr;
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
// Initial peer values.
const char kName[] = "friendly name";
const char kNote[] = "descriptive note";
const time_t kPeerLastSeen = 100;
// Updated version of those fields.
const char kNewName[] = "new friendly name";
const char kNewNote[] = "new descriptive note";
const time_t kNewPeerLastSeen = 200;
// Bad values for peer fields.
const char kBadName[] = "#evil name";
const char kBadNote[] = "#evil note";
const time_t kStalePeerLastSeen = 5;
// Service bits.
const char kServiceId[] = "a-service-id";
const char kBadServiceId[] = "#bad_service_id";
const time_t kServiceStaleLastSeen = 201;
const time_t kServiceLastSeen = 225;
const time_t kServiceNewLastSeen = 250;

tech_t kFakeTech1 = 1 << 29;
tech_t kFakeTech2 = 1 << 30;

}  // namespace

namespace peerd {

class DiscoveredPeerTest : public testing::Test {
 public:
  void SetUp() override {
    // Ignore threading concerns.
    EXPECT_CALL(*bus_, AssertOnOriginThread()).Times(AnyNumber());
    EXPECT_CALL(*bus_, AssertOnDBusThread()).Times(AnyNumber());
    // We're going to set up a DiscoveredPeer; it needs a DBusObject.
    EXPECT_CALL(*bus_, GetExportedObject(Property(&ObjectPath::value,
                                                  kPeerPath)))
        .WillOnce(Return(peer_object_.get()));
    EXPECT_CALL(*peer_object_, ExportMethod(_, _, _, _))
        .WillRepeatedly(Invoke(&test_util::HandleMethodExport));
    peer_.reset(new DiscoveredPeer{bus_, nullptr, ObjectPath{kPeerPath},
                                   kFakeTech1});
    EXPECT_TRUE(peer_->RegisterAsync(
        nullptr, kPeerId, kName, kNote, Time::FromTimeT(kPeerLastSeen),
        AsyncEventSequencer::GetDefaultCompletionAction()));
    // By default, no services should be exported.
    EXPECT_CALL(*bus_, GetExportedObject(_)) .Times(0);
    EXPECT_CALL(*service_object_, Unregister()).Times(0);
    // Don't worry about signals.
    EXPECT_CALL(*peer_object_, SendSignal(_)).Times(AnyNumber());
    EXPECT_CALL(*service_object_, SendSignal(_)).Times(AnyNumber());
  }

  void TearDown() override {
    Mock::VerifyAndClearExpectations(peer_object_.get());
    Mock::VerifyAndClearExpectations(service_object_.get());
    EXPECT_CALL(*peer_object_, Unregister()).Times(1);
    if (service_exposed_) {
      ExpectServiceRemoved();
    }
  }

  void ExpectInitialPeerValues() {
    EXPECT_EQ(kName, peer_->GetFriendlyName());
    EXPECT_EQ(kNote, peer_->GetNote());
    EXPECT_EQ(Time::FromTimeT(kPeerLastSeen), peer_->GetLastSeen());
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
    Mock::VerifyAndClearExpectations(service_object_.get());
    service_exposed_ = false;
    EXPECT_CALL(*service_object_, Unregister()).Times(1);
  }


  void ExpectNoServicesDiscovered() {
    EXPECT_TRUE(peer_->service_metadata_.empty());
    EXPECT_TRUE(peer_->services_.empty());
  }

  void ExpectServiceHasValues(const string& service_id,
                              const Service::IpAddresses& ip_addr,
                              const Service::ServiceInfo& info,
                              const base::Time& last_seen,
                              tech_t tech) {
    auto service_it = peer_->services_.find(service_id);
    auto metadata_it = peer_->service_metadata_.find(service_id);
    // Better find this peer in both collections of data.
    ASSERT_NE(service_it, peer_->services_.end());
    ASSERT_NE(metadata_it, peer_->service_metadata_.end());
    // Fields in the peer should match.
    EXPECT_EQ(ip_addr, service_it->second->GetIpAddresses());
    EXPECT_EQ(info, service_it->second->GetServiceInfo());
    // Metadata should also match.
    EXPECT_EQ(last_seen, metadata_it->second.last_seen);
    EXPECT_EQ(tech, metadata_it->second.technology);
  }

  scoped_refptr<dbus::MockBus> bus_{new dbus::MockBus{dbus::Bus::Options{}}};
  scoped_refptr<MockExportedObject> peer_object_{
      new MockExportedObject{bus_.get(), ObjectPath{kPeerPath}}};
  scoped_refptr<MockExportedObject> service_object_{
      new MockExportedObject{bus_.get(), ObjectPath{kServicePath}}};
  bool service_exposed_{false};
  unique_ptr<DiscoveredPeer> peer_;
};

TEST_F(DiscoveredPeerTest, CanUpdateFromAdvertisement) {
  peer_->UpdateFromAdvertisement(kNewName, kNewNote,
                                 Time::FromTimeT(kNewPeerLastSeen),
                                 kFakeTech1);
  EXPECT_EQ(kNewName, peer_->GetFriendlyName());
  EXPECT_EQ(kNewNote, peer_->GetNote());
  EXPECT_EQ(Time::FromTimeT(kNewPeerLastSeen), peer_->GetLastSeen());
  EXPECT_EQ(1, peer_->GetTechnologyCount());
}

TEST_F(DiscoveredPeerTest, CannotUpdateFromInvalidAdvertisement) {
  peer_->UpdateFromAdvertisement(kBadName, kNewNote,
                                 Time::FromTimeT(kNewPeerLastSeen),
                                 kFakeTech1);
  ExpectInitialPeerValues();
}

TEST_F(DiscoveredPeerTest, CannotPartiallyUpdatePeer) {
  // Only invalid note.
  peer_->UpdateFromAdvertisement(kNewName, kBadNote,
                                 Time::FromTimeT(kNewPeerLastSeen),
                                 kFakeTech1);
  ExpectInitialPeerValues();
  // Only invalid name.
  peer_->UpdateFromAdvertisement(kBadName, kNewNote,
                                 Time::FromTimeT(kNewPeerLastSeen),
                                 kFakeTech1);
  ExpectInitialPeerValues();
  // Stale advertisement.
  peer_->UpdateFromAdvertisement(kNewName, kNewNote,
                                 Time::FromTimeT(kStalePeerLastSeen),
                                 kFakeTech1);
  ExpectInitialPeerValues();
}

TEST_F(DiscoveredPeerTest, CanAddService) {
  ExpectServiceExposed();
  peer_->UpdateService(kServiceId, {}, {}, Time::FromTimeT(kServiceLastSeen),
                       kFakeTech1);
}

TEST_F(DiscoveredPeerTest, CannotPartiallyAddService) {
  peer_->UpdateService(kBadServiceId, {}, {},
                       Time::FromTimeT(kServiceLastSeen), kFakeTech1);
  ExpectNoServicesDiscovered();
}

TEST_F(DiscoveredPeerTest, CannotAddServiceForUnknownTechnology) {
  peer_->UpdateService(kServiceId, {}, {}, Time::FromTimeT(kServiceLastSeen),
                       kFakeTech2);
}

TEST_F(DiscoveredPeerTest, CanUpdateService) {
  ExpectServiceExposed();
  peer_->UpdateService(kServiceId, {}, {},
                       Time::FromTimeT(kServiceLastSeen), kFakeTech1);
  const Service::ServiceInfo valid_info{{"good_key", "valid value"}};
  peer_->UpdateService(kServiceId, {}, valid_info,
                       Time::FromTimeT(kServiceNewLastSeen), kFakeTech1);
  ExpectServiceHasValues(kServiceId, {}, valid_info,
                         Time::FromTimeT(kServiceNewLastSeen),
                         kFakeTech1);
}

TEST_F(DiscoveredPeerTest, CannotPartiallyUpdateService) {
  ExpectServiceExposed();
  peer_->UpdateService(kServiceId, {}, {},
                       Time::FromTimeT(kServiceLastSeen), kFakeTech1);
  const Service::ServiceInfo invalid_info{{"#badkey", "valid value"}};
  peer_->UpdateService(kServiceId, {}, invalid_info,
                       Time::FromTimeT(kServiceLastSeen), kFakeTech2);
  ExpectServiceHasValues(kServiceId, {}, {}, Time::FromTimeT(kServiceLastSeen),
                         kFakeTech1);
}

TEST_F(DiscoveredPeerTest, ShouldRejectStaleServiceUpdate) {
  peer_->UpdateFromAdvertisement(kName, kNote,
                                 Time::FromTimeT(kPeerLastSeen),
                                 kFakeTech2);
  ExpectServiceExposed();
  peer_->UpdateService(kServiceId, {}, {},
                       Time::FromTimeT(kServiceLastSeen), kFakeTech1);
  const Service::ServiceInfo valid_info{{"good_key", "valid value"}};
  peer_->UpdateService(kServiceId, {}, valid_info,
                       Time::FromTimeT(kServiceStaleLastSeen), kFakeTech2);
  ExpectServiceHasValues(kServiceId, {}, {},
                         Time::FromTimeT(kServiceLastSeen), kFakeTech1);
}

TEST_F(DiscoveredPeerTest, RemoveTechnologyIsRecursive) {
  ExpectServiceExposed();
  peer_->UpdateService(kServiceId, {}, {}, Time::FromTimeT(kServiceLastSeen),
                       kFakeTech1);
  ExpectServiceRemoved();
  peer_->RemoveTechnology(kFakeTech1);
}

TEST_F(DiscoveredPeerTest, HandlesMultipleTechnologies) {
  peer_->UpdateFromAdvertisement(kNewName, kNewNote,
                                 Time::FromTimeT(kNewPeerLastSeen),
                                 kFakeTech2);
  EXPECT_EQ(2, peer_->GetTechnologyCount());
  ExpectServiceExposed();
  peer_->UpdateService(kServiceId, {}, {}, Time::FromTimeT(kServiceLastSeen),
                       kFakeTech1);
  // This should not create a new service object, just touch the existing one.
  peer_->UpdateService(kServiceId, {}, {}, Time::FromTimeT(kServiceLastSeen),
                       kFakeTech2);
  peer_->RemoveTechnology(kFakeTech2);
  EXPECT_EQ(1, peer_->GetTechnologyCount());
  ExpectServiceRemoved();
  peer_->RemoveTechnologyFromService(kServiceId, kFakeTech1);
}

}  // namespace peerd
