// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/service_under_test.h"

#include <string>

#include "shill/mock_adaptors.h"

using std::string;

namespace shill {

// static
const char ServiceUnderTest::kRpcId[] = "/mock_device_rpc";
// static
const char ServiceUnderTest::kStorageId[] = "service";

ServiceUnderTest::ServiceUnderTest(ControlInterface *control_interface,
                                   EventDispatcher *dispatcher,
                                   Metrics *metrics,
                                   Manager *manager)
    : Service(control_interface, dispatcher, metrics, manager,
              Technology::kUnknown) {
}

ServiceUnderTest::~ServiceUnderTest() {}

string ServiceUnderTest::CalculateState(Error */*error*/) { return ""; }

string ServiceUnderTest::GetRpcIdentifier() const {
  return ServiceMockAdaptor::kRpcId;
}

string ServiceUnderTest::GetDeviceRpcId(Error */*error*/) { return kRpcId; }

string ServiceUnderTest::GetStorageIdentifier() const { return kStorageId; }

}  // namespace shill
