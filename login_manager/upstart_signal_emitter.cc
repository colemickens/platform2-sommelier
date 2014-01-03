// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/upstart_signal_emitter.h"

#include <string>
#include <vector>

#include <base/logging.h>
#include <chromeos/dbus/dbus.h>

namespace login_manager {

const char kDestination[] = "com.ubuntu.Upstart";
const char kPath[] = "/com/ubuntu/Upstart";
const char kInterface[] = "com.ubuntu.Upstart0_6";
const char kMethodName[] = "EmitEvent";

bool UpstartSignalEmitter::EmitSignal(
    const std::string& signal_name,
    const std::vector<std::string>& args_keyvals,
    GError** error) {
  DLOG(INFO) << "Emitting " << signal_name << " Upstart signal";

  chromeos::dbus::Proxy proxy(chromeos::dbus::GetSystemBusConnection(),
                              kDestination, kPath, kInterface);
  if (!proxy) {
    LOG(ERROR) << "No proxy; can't call " << kInterface << "." << kMethodName;
    return false;
  }
  char** env = g_new(char*, args_keyvals.size() + 1);  // NULL-terminated.
  for (size_t i = 0; i < args_keyvals.size(); ++i) {
    env[i] = g_strdup(args_keyvals[i].c_str());
  }
  env[args_keyvals.size()] = NULL;
  bool success = ::dbus_g_proxy_call(proxy.gproxy(), kMethodName, error,
                                     G_TYPE_STRING, signal_name.c_str(),
                                     G_TYPE_STRV, env,
                                     G_TYPE_BOOLEAN, true,
                                     G_TYPE_INVALID,
                                     G_TYPE_INVALID);
  if (*error) {
    LOG(ERROR) << kInterface << "." << kPath << " failed: "
               << (*error)->message;
  }
  g_strfreev(env);  // Free the elements as well.
  return success;
}

}  // namespace login_manager
