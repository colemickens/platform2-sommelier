// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_SERVICE_
#define SHILL_MOCK_SERVICE_

#include <base/memory/ref_counted.h>
#include <gmock/gmock.h>

#include "shill/refptr_types.h"
#include "shill/service.h"

namespace shill {

class ControlInterface;
class EventDispatcher;
class Manager;

class MockService : public Service {
 public:
  // A constructor for the Service object
  MockService(ControlInterface *control_interface,
              EventDispatcher *dispatcher,
              Manager *manager);
  virtual ~MockService();

  MOCK_METHOD0(Connect, void());
  MOCK_METHOD0(Disconnect, void());
  MOCK_METHOD0(CalculateState, std::string());
  MOCK_METHOD0(GetDeviceRpcId, std::string());
  MOCK_CONST_METHOD0(GetRpcIdentifier, std::string());
  MOCK_METHOD1(GetStorageIdentifier, std::string(const std::string &));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockService);
};

}  // namespace shill

#endif  // SHILL_MOCK_SERVICE_
