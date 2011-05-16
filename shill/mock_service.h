// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_SERVICE_
#define SHILL_MOCK_SERVICE_

#include <base/memory/ref_counted.h>
#include <gmock/gmock.h>

#include "shill/service.h"

namespace shill {

class ControlInterface;
class EventDispatcher;

using ::testing::_;
using ::testing::Return;

class MockService : public Service {
 public:
  // A constructor for the Service object
  MockService(ControlInterface *control_interface,
              EventDispatcher *dispatcher,
              Device *interface)
      : Service(control_interface, dispatcher, interface) { }
  virtual ~MockService() {}

  MOCK_METHOD0(Connect, void(void));
  MOCK_METHOD0(Disconnect, void(void));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockService);
};

}  // namespace shill

#endif  // SHILL_MOCK_SERVICE_
