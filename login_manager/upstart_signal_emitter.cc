// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/upstart_signal_emitter.h"

#include <string>
#include <vector>

#include <base/logging.h>
#include <dbus/message.h>
#include <dbus/object_proxy.h>

namespace login_manager {

const char UpstartSignalEmitter::kServiceName[] = "com.ubuntu.Upstart";
const char UpstartSignalEmitter::kPath[] = "/com/ubuntu/Upstart";
const char UpstartSignalEmitter::kInterface[] = "com.ubuntu.Upstart0_6";
const char UpstartSignalEmitter::kMethodName[] = "EmitEvent";

UpstartSignalEmitter::UpstartSignalEmitter(dbus::ObjectProxy* proxy)
    : upstart_dbus_proxy_(proxy) {
}

UpstartSignalEmitter::~UpstartSignalEmitter() {}

scoped_ptr<dbus::Response> UpstartSignalEmitter::EmitSignal(
    const std::string& signal_name,
    const std::vector<std::string>& args_keyvals) {
  DLOG(INFO) << "Emitting " << signal_name << " Upstart signal";

  dbus::MethodCall method_call(UpstartSignalEmitter::kInterface,
                               UpstartSignalEmitter::kMethodName);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(signal_name);
  writer.AppendArrayOfStrings(args_keyvals);
  writer.AppendBool(true);

  return scoped_ptr<dbus::Response>(upstart_dbus_proxy_->CallMethodAndBlock(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
}

}  // namespace login_manager
