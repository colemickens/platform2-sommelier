// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PORTIER_PORTIERD_H_
#define PORTIER_PORTIERD_H_

#include <memory>

#include <base/callback.h>
#include <base/macros.h>
#include <brillo/daemons/dbus_daemon.h>
#include <dbus/bus.h>
#include <dbus/exported_object.h>

namespace portier {

// The Portier daemon which listens to the system D-Bus for method
// calls.
class Portierd : public brillo::DBusServiceDaemon {
 public:
  static std::unique_ptr<Portierd> Create();
  ~Portierd() override = default;

 protected:
  // Initializes D-Bus methods and configures the RTNLHandler instance
  // to listen for changes on the kernel's networking tables.  Called
  // automatically by the Daemon class.
  int OnInit() override;

  // Other callbacks.
  int OnEventLoopStarted() override;
  void OnShutdown(int* exit_code) override;
  bool OnRestart() override;

 private:
  Portierd();

  // Initializes the internal NDProxy logic.
  bool Init();

  // Portier D-Bus methods.
  std::unique_ptr<dbus::Response> BindInterface(dbus::MethodCall* method_call);
  std::unique_ptr<dbus::Response> ReleaseInterface(
      dbus::MethodCall* method_call);
  std::unique_ptr<dbus::Response> CreateProxyGroup(
      dbus::MethodCall* method_call);
  std::unique_ptr<dbus::Response> ReleaseProxyGroup(
      dbus::MethodCall* method_call);
  std::unique_ptr<dbus::Response> AddToGroup(dbus::MethodCall* method_call);
  std::unique_ptr<dbus::Response> RemoveFromGroup(
      dbus::MethodCall* method_call);
  std::unique_ptr<dbus::Response> SetUpstream(dbus::MethodCall* method_call);
  std::unique_ptr<dbus::Response> UnsetUpstream(dbus::MethodCall* method_call);

  // D-Bus exported object handler, used to export D-Bus method.
  dbus::ExportedObject* exported_object_;
};

}  // namespace portier

#endif  // PORTIER_PORTIERD_H_
