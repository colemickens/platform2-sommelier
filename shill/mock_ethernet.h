// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_ETHERNET_H_
#define SHILL_MOCK_ETHERNET_H_

#include <string>

#include <gmock/gmock.h>

#include "shill/refptr_types.h"
#include "shill/ethernet.h"

namespace shill {

class ControlInterface;
class Error;
class EventDispatcher;

class MockEthernet : public Ethernet {
 public:
  MockEthernet(ControlInterface *control_interface,
               EventDispatcher *dispatcher,
               Metrics *metrics,
               Manager *manager,
               const std::string &link_name,
               const std::string &address,
               int interface_index);
  virtual ~MockEthernet();

  MOCK_METHOD2(Start, void(Error *error,
                           const EnabledStateChangedCallback &callback));
  MOCK_METHOD2(Stop, void(Error *error,
                          const EnabledStateChangedCallback &callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockEthernet);
};

}  // namespace shill

#endif  // SHILL_MOCK_ETHERNET_H_
