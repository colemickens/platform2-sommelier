// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_DBUS_SIGNAL_EMITTER_H_
#define LOGIN_MANAGER_DBUS_SIGNAL_EMITTER_H_

#include <string>
#include <vector>

#include <base/compiler_specific.h>
#include <base/macros.h>

namespace dbus {
class ExportedObject;
}

namespace login_manager {

// Used to emit D-Bus signals.
class DBusSignalEmitter {
 public:
  DBusSignalEmitter(dbus::ExportedObject* object, const std::string& interface);
  ~DBusSignalEmitter();

  void EmitSignal(const std::string& signal_name);
  void EmitSignalWithSuccessFailure(const std::string& signal_name,
                                    const bool success);
  void EmitSignalWithString(const std::string& signal_name,
                            const std::string& payload);
  void EmitSignalWithBool(const std::string& signal_name,
                          bool payload);
  void EmitSignalWithBoolAndString(const std::string& signal_name,
                                   bool payload1,
                                   const std::string& payload2);

 private:
  dbus::ExportedObject* object_;  // Weak, owned by caller.
  const std::string interface_;
  DISALLOW_COPY_AND_ASSIGN(DBusSignalEmitter);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_DBUS_SIGNAL_EMITTER_H_
