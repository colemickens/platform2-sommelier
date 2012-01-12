// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_DEVICE_
#define SHILL_MOCK_DEVICE_

#include <string>

#include <base/memory/ref_counted.h>
#include <gmock/gmock.h>

#include "shill/device.h"

namespace shill {

class MockDevice : public Device {
 public:
  MockDevice(ControlInterface *control_interface,
             EventDispatcher *dispatcher,
             Metrics *metrics,
             Manager *manager,
             const std::string &link_name,
             const std::string &address,
             int interface_index);
  virtual ~MockDevice();

  MOCK_METHOD0(Start, void());
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD1(Scan, void(Error *error));
  MOCK_CONST_METHOD1(TechnologyIs,
                     bool(const Technology::Identifier technology));
  MOCK_METHOD1(Load, bool(StoreInterface*));
  MOCK_METHOD1(Save, bool(StoreInterface*));
  MOCK_METHOD0(DisableIPv6, void());
  MOCK_METHOD0(EnableIPv6, void());
  MOCK_METHOD0(EnableIPv6Privacy, void());
  MOCK_METHOD0(DisableReversePathFilter, void());
  MOCK_METHOD0(EnableReversePathFilter, void());
  MOCK_CONST_METHOD0(technology, Technology::Identifier());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDevice);
};

}  // namespace shill

#endif  // SHILL_MOCK_DEVICE_
