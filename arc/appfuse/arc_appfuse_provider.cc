// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>

#include <base/logging.h>
#include <base/macros.h>
#include <base/memory/ref_counted.h>
#include <brillo/daemons/dbus_daemon.h>
#include <brillo/syslog_logging.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/bus.h>

#include "appfuse/dbus_adaptors/org.chromium.ArcAppfuseProvider.h"

namespace {

class DBusAdaptor : public org::chromium::ArcAppfuseProviderAdaptor,
                    public org::chromium::ArcAppfuseProviderInterface {
 public:
  explicit DBusAdaptor(scoped_refptr<dbus::Bus> bus)
      : org::chromium::ArcAppfuseProviderAdaptor(this),
        dbus_object_(
            nullptr,
            bus,
            dbus::ObjectPath(arc::appfuse::kArcAppfuseProviderServicePath)) {}

  ~DBusAdaptor() override = default;

  void RegisterAsync(
      const brillo::dbus_utils::AsyncEventSequencer::CompletionAction& cb) {
    RegisterWithDBusObject(&dbus_object_);
    dbus_object_.RegisterAsync(cb);
  }

  // org::chromium::ArcAppfuseProviderInterface overrides:
  bool Mount(brillo::ErrorPtr* error,
             int32_t uid,
             int32_t mount_id,
             brillo::dbus_utils::FileDescriptor* out_fd) override {
    NOTIMPLEMENTED();
    return true;
  }

  bool Unmount(brillo::ErrorPtr* error,
               int32_t uid,
               int32_t mount_id) override {
    NOTIMPLEMENTED();
    return true;
  }

  bool OpenFile(brillo::ErrorPtr* error,
                int32_t uid,
                int32_t mount_id,
                int32_t file_id,
                int32_t flags,
                brillo::dbus_utils::FileDescriptor* out_fd) override {
    NOTIMPLEMENTED();
    return true;
  }

 private:
  brillo::dbus_utils::DBusObject dbus_object_;

  DISALLOW_COPY_AND_ASSIGN(DBusAdaptor);
};

class Daemon : public brillo::DBusServiceDaemon {
 public:
  Daemon() : DBusServiceDaemon(arc::appfuse::kArcAppfuseProviderServiceName) {}
  ~Daemon() override = default;

 protected:
  void RegisterDBusObjectsAsync(
      brillo::dbus_utils::AsyncEventSequencer* sequencer) override {
    adaptor_ = std::make_unique<DBusAdaptor>(bus_);
    adaptor_->RegisterAsync(
        sequencer->GetHandler("RegisterAsync() failed.", true));
  }

 private:
  std::unique_ptr<DBusAdaptor> adaptor_;

  DISALLOW_COPY_AND_ASSIGN(Daemon);
};

}  // namespace

int main(int argc, char** argv) {
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);
  return Daemon().Run();
}
