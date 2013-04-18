// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/service_property_change_test.h"

#include <string>

#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "shill/error.h"
#include "shill/mock_adaptors.h"
#include "shill/service.h"

using std::string;
using testing::_;
using testing::AnyNumber;
using testing::Mock;

namespace shill {

// Some of these tests are duplicative, as we also have broader tests
// for specific setters. However, it's convenient to have all the property
// change notifications documented (and tested) in one place.

void TestCommonPropertyChanges(ServiceRefPtr service,
                               ServiceMockAdaptor *adaptor) {
  Error error;

  EXPECT_EQ(Service::kStateIdle, service->state());
  EXPECT_CALL(*adaptor, EmitStringChanged(flimflam::kStateProperty, _));
  service->SetState(Service::kStateConnected);
  Mock::VerifyAndClearExpectations(adaptor);

  // TODO(quiche): Once crosbug.com/34528 is resolved, add a test
  // that service->SetConnection emits kIPConfigProperty changed.

  bool connectable = service->connectable();
  EXPECT_CALL(*adaptor, EmitBoolChanged(flimflam::kConnectableProperty, _));
  service->SetConnectable(!connectable);
  Mock::VerifyAndClearExpectations(adaptor);

  EXPECT_EQ(string(), service->guid());
  EXPECT_CALL(*adaptor, EmitStringChanged(flimflam::kGuidProperty, _));
  service->SetGuid("some garbage", &error);
  Mock::VerifyAndClearExpectations(adaptor);

  EXPECT_FALSE(service->favorite());
  EXPECT_CALL(*adaptor, EmitBoolChanged(flimflam::kAutoConnectProperty, _))
      .Times(AnyNumber());
  EXPECT_CALL(*adaptor, EmitBoolChanged(flimflam::kFavoriteProperty, _));
  service->MakeFavorite();
  Mock::VerifyAndClearExpectations(adaptor);

  EXPECT_EQ(0, service->priority());
  EXPECT_CALL(*adaptor, EmitIntChanged(flimflam::kPriorityProperty, _));
  service->SetPriority(1, &error);
  Mock::VerifyAndClearExpectations(adaptor);

  EXPECT_EQ(string(), service->GetProxyConfig(&error));
  EXPECT_CALL(*adaptor, EmitStringChanged(flimflam::kProxyConfigProperty, _));
  service->SetProxyConfig("some garbage", &error);
  Mock::VerifyAndClearExpectations(adaptor);

  uint8 strength = service->strength();
  EXPECT_CALL(*adaptor,
              EmitUint8Changed(flimflam::kSignalStrengthProperty, _));
  service->SetStrength(strength+1);
  Mock::VerifyAndClearExpectations(adaptor);

  EXPECT_EQ(string(), service->error_details());
  EXPECT_CALL(*adaptor, EmitStringChanged(kErrorDetailsProperty, _));
  service->SetErrorDetails("some garbage");
  Mock::VerifyAndClearExpectations(adaptor);

  EXPECT_EQ(Service::kFailureUnknown, service->failure());
  EXPECT_EQ(Service::ConnectFailureToString(Service::kFailureUnknown),
            service->error());
  EXPECT_CALL(*adaptor, EmitStringChanged(flimflam::kStateProperty, _));
  EXPECT_CALL(*adaptor, EmitStringChanged(flimflam::kErrorProperty, _));
  service->SetFailure(Service::kFailureAAA);
  Mock::VerifyAndClearExpectations(adaptor);

  EXPECT_NE(Service::ConnectFailureToString(Service::kFailureUnknown),
            service->error());
  EXPECT_CALL(*adaptor, EmitStringChanged(flimflam::kStateProperty, _));
  EXPECT_CALL(*adaptor, EmitStringChanged(kErrorDetailsProperty, _));
  EXPECT_CALL(*adaptor, EmitStringChanged(flimflam::kErrorProperty, _));
  service->SetState(Service::kStateConnected);
  Mock::VerifyAndClearExpectations(adaptor);

  EXPECT_EQ(Service::ConnectFailureToString(Service::kFailureUnknown),
            service->error());
  EXPECT_CALL(*adaptor, EmitStringChanged(flimflam::kStateProperty, _));
  EXPECT_CALL(*adaptor, EmitStringChanged(flimflam::kErrorProperty, _));
  service->SetFailureSilent(Service::kFailureAAA);
  Mock::VerifyAndClearExpectations(adaptor);
}

void TestAutoConnectPropertyChange(ServiceRefPtr service,
                                   ServiceMockAdaptor *adaptor) {
  bool auto_connect = service->auto_connect();
  EXPECT_CALL(*adaptor, EmitBoolChanged(flimflam::kAutoConnectProperty, _));
  service->SetAutoConnect(!auto_connect);
  Mock::VerifyAndClearExpectations(adaptor);
}

void TestNamePropertyChange(ServiceRefPtr service,
                            ServiceMockAdaptor *adaptor) {
  Error error;
  string name = service->GetNameProperty(&error);
  EXPECT_CALL(*adaptor, EmitStringChanged(flimflam::kNameProperty, _));
  service->SetNameProperty(name + " and some new stuff", &error);
  Mock::VerifyAndClearExpectations(adaptor);
}

} // namespace shill
