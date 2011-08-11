// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/upstart_signal_emitter.h"

#include <cstdlib>

#include "base/logging.h"
#include "base/string_util.h"
#include "chromeos/dbus/error_constants.h"
#include "chromeos/dbus/service_constants.h"

using std::string;

namespace login_manager {

bool UpstartSignalEmitter::EmitSignal(const string& signal_name,
                                      const string& args_str,
                                      GError** error) {
  DLOG(INFO) << "Emitting " << signal_name << " Upstart signal";
  string command = StringPrintf("/sbin/initctl emit %s %s",
                                signal_name.c_str(), args_str.c_str());
  bool success = (system(command.c_str()) == 0);
  if (!success && error != NULL) {
    g_set_error(error,
                CHROMEOS_LOGIN_ERROR,
                CHROMEOS_LOGIN_ERROR_EMIT_FAILED,
                "Login error: %s",
                StringPrintf("Can't emit %s", signal_name.c_str()).c_str());
  }
  return success;
}

}  // namespace login_manager
