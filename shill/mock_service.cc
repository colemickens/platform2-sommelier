// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_service.h"

#include <string>

#include <base/memory/ref_counted.h>
#include <base/stringprintf.h>
#include <gmock/gmock.h>

#include "shill/refptr_types.h"
#include "shill/store_interface.h"
#include "shill/technology.h"

using std::string;
using testing::_;
using testing::Return;

namespace shill {

class ControlInterface;
class EventDispatcher;
class Manager;

MockService::MockService(ControlInterface *control_interface,
                         EventDispatcher *dispatcher,
                         Manager *manager)
    : Service(control_interface, dispatcher, manager, Technology::kUnknown) {
  const string &id = UniqueName();
  EXPECT_CALL(*this, GetRpcIdentifier()).WillRepeatedly(Return(id));
  EXPECT_CALL(*this, GetStorageIdentifier()).WillRepeatedly(Return(id));
  ON_CALL(*this, state()).WillByDefault(Return(kStateUnknown));
  ON_CALL(*this, failure()).WillByDefault(Return(kFailureUnknown));
  ON_CALL(*this, TechnologyIs(_)).WillByDefault(Return(false));
}

MockService::~MockService() {}

bool MockService::FauxSave(StoreInterface *store) {
  return store->SetString(UniqueName(), "dummy", "dummy");
}

}  // namespace shill
