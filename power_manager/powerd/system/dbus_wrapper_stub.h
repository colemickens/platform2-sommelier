// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_DBUS_WRAPPER_STUB_H_
#define POWER_MANAGER_POWERD_SYSTEM_DBUS_WRAPPER_STUB_H_

#include <string>

#include <base/memory/scoped_vector.h>
#include <google/protobuf/message_lite.h>

#include "power_manager/powerd/system/dbus_wrapper.h"

namespace power_manager {
namespace system {

// Stub implementation of DBusWrapperInterface for testing.
class DBusWrapperStub : public DBusWrapperInterface {
 public:
  // Information about a signal that was sent.
  struct SignalInfo {
    std::string signal_name;
    std::string protobuf_type;
    std::string serialized_data;
  };

  DBusWrapperStub();
  virtual ~DBusWrapperStub();

  bool service_published() const { return service_published_; }
  size_t num_sent_signals() const { return sent_signals_.size(); }

  // Copies the signal at position |index| in |sent_signals_| (that is, the
  // |index|th-sent signal) to |protobuf|, which should be a concrete protocol
  // buffer.  false is returned if the index is out-of-range, the D-Bus signal
  // name doesn't match |expected_signal_name|, or the type of protocol buffer
  // that was attached to the signal doesn't match |protobuf|'s type.
  // |protobuf| can be NULL, in which case only the signal name is checked.
  bool GetSentSignal(size_t index,
                     const std::string& expected_signal_name,
                     google::protobuf::MessageLite* protobuf);

  // Clears |sent_signals_|.
  void ClearSentSignals();

  // DBusWrapperInterface overrides:
  dbus::Bus* GetBus() override;
  dbus::ObjectProxy* GetObjectProxy(
      const std::string& service_name,
      const std::string& object_path) override;
  void RegisterForServiceAvailability(
      dbus::ObjectProxy* proxy,
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) override;
  void RegisterForSignal(dbus::ObjectProxy* proxy,
                         const std::string& interface_name,
                         const std::string& signal_name,
                         dbus::ObjectProxy::SignalCallback callback) override;
  void ExportMethod(const std::string& method_name,
                    dbus::ExportedObject::MethodCallCallback callback) override;
  bool PublishService() override;
  void EmitSignal(dbus::Signal* signal) override;
  void EmitBareSignal(const std::string& signal_name) override;
  void EmitSignalWithProtocolBuffer(
      const std::string& signal_name,
      const google::protobuf::MessageLite& protobuf) override;
  std::unique_ptr<dbus::Response> CallMethodSync(
      dbus::ObjectProxy* proxy,
      dbus::MethodCall* method_call,
      base::TimeDelta timeout) override;
  void CallMethodAsync(
      dbus::ObjectProxy* proxy,
      dbus::MethodCall* method_call,
      base::TimeDelta timeout,
      dbus::ObjectProxy::ResponseCallback callback) override;

 private:
  // Has PublishService() been called?
  bool service_published_;

  ScopedVector<SignalInfo> sent_signals_;

  DISALLOW_COPY_AND_ASSIGN(DBusWrapperStub);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_DBUS_WRAPPER_STUB_H_
