// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_WIMAX_H_
#define SHILL_MOCK_WIMAX_H_

#include <string>

#include <gmock/gmock.h>

#include "shill/wimax.h"

namespace shill {

class ControlInterface;
class Error;
class EventDispatcher;

class MockWiMax : public WiMax {
 public:
  MockWiMax(ControlInterface *control,
            EventDispatcher *dispatcher,
            Metrics *metrics,
            Manager *manager,
            const std::string &link_name,
            const std::string &address,
            int interface_index,
            const RpcIdentifier &path);
  virtual ~MockWiMax();

  MOCK_METHOD2(Start, void(Error *error,
                           const EnabledStateChangedCallback &callback));
  MOCK_METHOD2(Stop, void(Error *error,
                          const EnabledStateChangedCallback &callback));
  MOCK_METHOD2(ConnectTo, void(const WiMaxServiceRefPtr &service,
                               Error *error));
  MOCK_METHOD2(DisconnectFrom, void(const WiMaxServiceRefPtr &service,
                                    Error *error));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWiMax);
};

}  // namespace shill

#endif  // SHILL_MOCK_WIMAX_H_
