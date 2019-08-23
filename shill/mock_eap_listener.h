// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_EAP_LISTENER_H_
#define SHILL_MOCK_EAP_LISTENER_H_

#include "shill/eap_listener.h"

#include <gmock/gmock.h>

namespace shill {

class MockEapListener : public EapListener {
 public:
  MockEapListener();
  ~MockEapListener() override;

  MOCK_METHOD0(Start, bool());
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD1(set_request_received_callback,
               void(const EapListener::EapRequestReceivedCallback& callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockEapListener);
};

}  // namespace shill

#endif  // SHILL_MOCK_EAP_LISTENER_H_
