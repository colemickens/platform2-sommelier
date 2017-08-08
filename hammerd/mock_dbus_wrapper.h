// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HAMMERD_MOCK_DBUS_WRAPPER_H_
#define HAMMERD_MOCK_DBUS_WRAPPER_H_

#include <string>

#include <gmock/gmock.h>

#include "hammerd/dbus_wrapper.h"

namespace hammerd {

class MockDBusWrapper : public DBusWrapperInterface {
 public:
  MockDBusWrapper() = default;

  MOCK_METHOD1(SendSignal, void(dbus::Signal* signal));
  MOCK_METHOD1(SendSignal, void(const std::string& signal_name));
};

}  // namespace hammerd
#endif  // HAMMERD_MOCK_DBUS_WRAPPER_H_
