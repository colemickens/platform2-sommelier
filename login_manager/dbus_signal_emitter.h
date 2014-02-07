// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_DBUS_SIGNAL_EMITTER_H_
#define LOGIN_MANAGER_DBUS_SIGNAL_EMITTER_H_

#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/compiler_specific.h>

namespace login_manager {

// Simple mockable interface for emitting DBus signals.
class DBusSignalEmitterInterface {
 public:
  // Strings for encoding boolean status in signals.
  static const char kSignalSuccess[];
  static const char kSignalFailure[];

  virtual ~DBusSignalEmitterInterface();

  // Broadcasts |signal_name| from the session manager DBus interface.
  virtual void EmitSignal(const char* signal_name) = 0;

  // Broadcasts |signal_name| from the session manager DBus interface,
  // optionally adding |payload| as args if it is not empty.
  virtual void EmitSignalWithStringArgs(
      const char* signal_name,
      const std::vector<std::string>& payload) = 0;

  // Same as above, but accepts a boolean status that'll be encoded as
  // |kSignalSuccess| and |kSignalFailure| respectively.
  virtual void EmitSignalWithBoolean(const char* signal_name, bool status) = 0;
};

// Simple mockable interface for emitting DBus signals.
class DBusSignalEmitter : public DBusSignalEmitterInterface {
 public:
  DBusSignalEmitter();
  virtual ~DBusSignalEmitter();

  virtual void EmitSignal(const char* signal_name) OVERRIDE;
  virtual void EmitSignalWithStringArgs(
      const char* signal_name,
      const std::vector<std::string>& payload) OVERRIDE;
  virtual void EmitSignalWithBoolean(const char* signal_name,
                                     bool status) OVERRIDE;

 private:
  // Does the actual work of emitting the signal.
  void EmitSignalFrom(const char* interface,
                      const char* signal_name,
                      const std::vector<std::string>& payload);
  DISALLOW_COPY_AND_ASSIGN(DBusSignalEmitter);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_DBUS_SIGNAL_EMITTER_H_
