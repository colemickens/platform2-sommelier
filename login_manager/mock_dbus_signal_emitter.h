// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_MOCK_DBUS_SIGNAL_EMITTER_H_
#define LOGIN_MANAGER_MOCK_DBUS_SIGNAL_EMITTER_H_

#include "login_manager/dbus_signal_emitter.h"

#include <gmock/gmock.h>

namespace login_manager {

// Mock implementation of DBusSignalEmitter that always reports success.
class MockDBusSignalEmitter : public DBusSignalEmitterInterface {
 public:
  MockDBusSignalEmitter();
  virtual ~MockDBusSignalEmitter();

  MOCK_METHOD1(EmitSignal, void(const char*));
  MOCK_METHOD2(EmitSignalWithStringArgs,
               void(const char*, const std::vector<std::string>&));
  MOCK_METHOD2(EmitSignalWithBoolean, void(const char*, bool));
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_MOCK_DBUS_SIGNAL_EMITTER_H_
