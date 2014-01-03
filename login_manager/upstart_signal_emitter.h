// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_UPSTART_SIGNAL_EMITTER_H_
#define LOGIN_MANAGER_UPSTART_SIGNAL_EMITTER_H_

#include <string>
#include <vector>

#include <glib.h>

namespace login_manager {

// Simple mockable class for emitting Upstart signals.
class UpstartSignalEmitter {
 public:
  UpstartSignalEmitter() {}
  virtual ~UpstartSignalEmitter() {}

  // Emits an upstart signal.  |args_keyvals| will be provided as
  // environment variables to any upstart jobs kicked off as a result
  // of the signal. Each element of |args_keyvals| is a string of the format
  // "key=value".
  //
  // If the emission fails and |error| is non-NULL, records an error message in
  // it.  Returns true if the emission is successful and false otherwise.
  virtual bool EmitSignal(const std::string& signal_name,
                          const std::vector<std::string>& args_keyvals,
                          GError** error);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_UPSTART_SIGNAL_EMITTER_H_
