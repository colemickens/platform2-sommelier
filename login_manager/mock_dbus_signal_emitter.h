// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_MOCK_DBUS_SIGNAL_EMITTER_H_
#define LOGIN_MANAGER_MOCK_DBUS_SIGNAL_EMITTER_H_

#include "login_manager/dbus_signal_emitter.h"

#include <string>

#include <gmock/gmock.h>

namespace login_manager {

// Mock implementation of DBusSignalEmitter that always reports success.
class MockDBusSignalEmitter : public DBusSignalEmitterInterface {
 public:
  MockDBusSignalEmitter();
  virtual ~MockDBusSignalEmitter();

  MOCK_METHOD1(EmitSignal, void(const std::string&));
  MOCK_METHOD2(EmitSignalWithSuccessFailure, void(const std::string&, bool));
  MOCK_METHOD2(EmitSignalWithString, void(const std::string&,
                                          const std::string&));
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_MOCK_DBUS_SIGNAL_EMITTER_H_
