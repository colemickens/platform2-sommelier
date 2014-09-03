// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_UPSTART_SIGNAL_EMITTER_H_
#define LOGIN_MANAGER_UPSTART_SIGNAL_EMITTER_H_

#include <string>
#include <vector>

#include <base/macros.h>
#include <base/memory/scoped_ptr.h>

namespace dbus {
class ObjectProxy;
class Response;
}

namespace login_manager {
// Simple mockable class for emitting Upstart signals.
class UpstartSignalEmitter {
 public:
  static const char kServiceName[];
  static const char kPath[];
  static const char kInterface[];
  static const char kMethodName[];

  explicit UpstartSignalEmitter(dbus::ObjectProxy* proxy);
  virtual ~UpstartSignalEmitter();

  // Emits an upstart signal.  |args_keyvals| will be provided as
  // environment variables to any upstart jobs kicked off as a result
  // of the signal. Each element of |args_keyvals| is a string of the format
  // "key=value".
  //
  // Returns NULL if emitting the signal fails.
  // Caller takes ownership of this Response*.
  virtual scoped_ptr<dbus::Response> EmitSignal(
      const std::string& signal_name,
      const std::vector<std::string>& args_keyvals);

 private:
  dbus::ObjectProxy* upstart_dbus_proxy_;  // Weak, owned by caller.
  DISALLOW_COPY_AND_ASSIGN(UpstartSignalEmitter);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_UPSTART_SIGNAL_EMITTER_H_
