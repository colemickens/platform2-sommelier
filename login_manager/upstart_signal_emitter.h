// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_UPSTART_SIGNAL_EMITTER_H_
#define LOGIN_MANAGER_UPSTART_SIGNAL_EMITTER_H_

#include <string>

#include <glib.h>

namespace login_manager {

// Simple mockable class for emitting Upstart signals.
class UpstartSignalEmitter {
 public:
  UpstartSignalEmitter() {}
  virtual ~UpstartSignalEmitter() {}

  // Emits an upstart signal.  |args_str| will be appended to the "initctl emit"
  // command; it can be used to add arguments to the signal.
  //
  // If the emission fails and |error| is non-NULL, records an error message in
  // it.  Returns true if the emission is successful and false otherwise.
  virtual bool EmitSignal(const std::string& signal_name,
                          const std::string& args_str,
                          GError** error);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_UPSTART_SIGNAL_EMITTER_H_
