// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_DBUS_WRAPPER_H_
#define POWER_MANAGER_POWERD_SYSTEM_DBUS_WRAPPER_H_

#include <string>

#include <base/compiler_specific.h>
#include <base/macros.h>

namespace dbus {
class ExportedObject;
}  // namespace dbus

namespace google {
namespace protobuf {
class MessageLite;
}  // namespace protobuf
}  // namespace google

namespace power_manager {
namespace system {

// Interface for sending D-Bus messages.  A stub implementation can be
// instantiated by tests to verify behavior without actually communicating with
// D-Bus.
// TODO(derat): Just have an EmitSignal() method that takes a dbus::Signal.
class DBusWrapperInterface {
 public:
  virtual ~DBusWrapperInterface() {}

  // Emits a signal named |signal_name| without any arguments.
  virtual void EmitBareSignal(const std::string& signal_name) = 0;

  // Emits a signal named |signal_name| and containing a serialized copy of
  // |protobuf| as a single byte array argument.
  virtual void EmitSignalWithProtocolBuffer(
      const std::string& signal_name,
      const google::protobuf::MessageLite& protobuf) = 0;
};

// DBusWrapper implementation that actually sends messages to D-Bus.
class DBusWrapper : public DBusWrapperInterface {
 public:
  DBusWrapper();
  virtual ~DBusWrapper();

  // |object| and |interface| are used when sending signals.
  void Init(dbus::ExportedObject* object, const std::string& interface);

  // DBusWrapperInterface overrides:
  void EmitBareSignal(const std::string& signal_name) override;
  void EmitSignalWithProtocolBuffer(
      const std::string& signal_name,
      const google::protobuf::MessageLite& protobuf) override;

 private:
  dbus::ExportedObject* object_;
  std::string interface_;

  DISALLOW_COPY_AND_ASSIGN(DBusWrapper);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_DBUS_WRAPPER_H_
