// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/published_peer.h"

#include <string>

#include <chromeos/errors/error.h>
#include <dbus/mock_bus.h>
#include <dbus/mock_exported_object.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "peerd/mock_service_publisher.h"
#include "peerd/service.h"
#include "peerd/test_util.h"

using chromeos::ErrorPtr;
using dbus::MockBus;
using dbus::ObjectPath;
using std::string;
using std::unique_ptr;
using testing::AnyNumber;
using testing::Invoke;
using testing::Return;
using testing::StartsWith;
using testing::_;

namespace {

const char kPath[] = "/some/path/ending/with";
const char kServicePathPrefix[] = "/some/path/ending/with/services/";
const char kServicePath[] = "/some/path/ending/with/services/1";

}  // namespace

namespace peerd {

class PublishedPeerTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Ignore threading concerns.
    EXPECT_CALL(*bus_, AssertOnOriginThread()).Times(AnyNumber());
    EXPECT_CALL(*bus_, AssertOnDBusThread()).Times(AnyNumber());
    // Just return one object to represent the service(s) we'll create.
    EXPECT_CALL(*bus_,
                GetExportedObject(Property(&ObjectPath::value,
                                           StartsWith(kServicePathPrefix))))
        .WillRepeatedly(Return(service_object_.get()));
    // Silence annoying logspam from the service.
    EXPECT_CALL(*service_object_, ExportMethod(_, _, _, _))
        .WillRepeatedly(Invoke(&test_util::HandleMethodExport));
    EXPECT_CALL(*service_object_, Unregister()).Times(AnyNumber());
  }

  scoped_refptr<MockBus> bus_{new MockBus{dbus::Bus::Options{}}};
  scoped_refptr<dbus::MockExportedObject> service_object_{
      new dbus::MockExportedObject{bus_.get(), ObjectPath{kServicePath}}};
  PublishedPeer peer_{bus_, nullptr, ObjectPath{kPath}};
};

TEST_F(PublishedPeerTest, ShouldNotifyExistingPublishersOnServiceAdded) {
  unique_ptr<MockServicePublisher> publisher(new MockServicePublisher());
  ErrorPtr error;
  peer_.RegisterServicePublisher(publisher->weak_ptr_factory_.GetWeakPtr());
  EXPECT_CALL(*publisher, OnServiceUpdated(&error, _)).WillOnce(Return(true));
  EXPECT_TRUE(peer_.AddService(
        &error, "some-service",
        Service::IpAddresses(), Service::ServiceInfo()));
  EXPECT_EQ(nullptr, error.get());
}

TEST_F(PublishedPeerTest, ShouldNotifyNewPublisherAboutExistingServices) {
  ErrorPtr error;
  EXPECT_TRUE(peer_.AddService(
        &error, "some-service",
        Service::IpAddresses(), Service::ServiceInfo()));
  EXPECT_EQ(nullptr, error.get());
  unique_ptr<MockServicePublisher> publisher(new MockServicePublisher());
  EXPECT_CALL(*publisher, OnServiceUpdated(nullptr, _)).WillOnce(Return(true));
  peer_.RegisterServicePublisher(publisher->weak_ptr_factory_.GetWeakPtr());
}

TEST_F(PublishedPeerTest, ShouldPrunePublisherList) {
  ErrorPtr error;
  EXPECT_TRUE(peer_.AddService(
        &error, "some-service",
        Service::IpAddresses(), Service::ServiceInfo()));
  EXPECT_EQ(nullptr, error.get());
  unique_ptr<MockServicePublisher> publisher(new MockServicePublisher());
  unique_ptr<MockServicePublisher> publisher2(new MockServicePublisher());
  EXPECT_CALL(*publisher, OnServiceUpdated(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*publisher2, OnServiceUpdated(_, _))
      .Times(2)
      .WillRepeatedly(Return(true));
  peer_.RegisterServicePublisher(publisher->weak_ptr_factory_.GetWeakPtr());
  peer_.RegisterServicePublisher(publisher2->weak_ptr_factory_.GetWeakPtr());
  publisher.reset();
  // At this point, we should notice that |publisher| has been deleted.
  EXPECT_TRUE(peer_.AddService(
        &error, "another-service",
        Service::IpAddresses(), Service::ServiceInfo()));
  EXPECT_EQ(nullptr, error.get());
}

}  // namespace peerd
