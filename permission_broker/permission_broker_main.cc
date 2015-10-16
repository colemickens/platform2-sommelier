// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "brillo/daemons/dbus_daemon.h"
#include "chromeos/dbus/service_constants.h"
#include "permission_broker/permission_broker.h"

namespace permission_broker {

namespace Firewalld = org::chromium::Firewalld;
using brillo::dbus_utils::AsyncEventSequencer;
const char kObjectServicePath[] =
    "/org/chromium/PermissionBroker/ObjectManager";

class Daemon : public brillo::DBusServiceDaemon {
 public:
  Daemon(std::string access_group, std::string udev_run_path, int poll_interval)
      : DBusServiceDaemon(kPermissionBrokerServiceName,
                          dbus::ObjectPath{kObjectServicePath}),
        access_group_(access_group),
        udev_run_path_(udev_run_path),
        poll_interval_(poll_interval) {}

 protected:
  void RegisterDBusObjectsAsync(AsyncEventSequencer* sequencer) override {
    firewalld_object_manager_.reset(new Firewalld::ObjectManagerProxy{bus_});
    firewalld_object_manager_->SetFirewalldAddedCallback(
        base::Bind(&Daemon::OnFirewallUp, weak_ptr_factory_.GetWeakPtr()));
    firewalld_object_manager_->SetFirewalldRemovedCallback(
        base::Bind(&Daemon::OnFirewallDown, weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  void OnFirewallUp(org::chromium::FirewalldProxy* firewalld) {
    LOG(INFO) << "firewalld instance created. "
                 "Putting permission_broker object on D-Bus.";
    broker_.reset(new PermissionBroker{object_manager_.get(), firewalld,
                                       access_group_, udev_run_path_,
                                       poll_interval_});
    broker_->RegisterAsync(AsyncEventSequencer::GetDefaultCompletionAction());
  }

  void OnFirewallDown(const dbus::ObjectPath& object_path) {
    LOG(INFO) << "firewalld instance went away. "
                 "Removing permission_broker object from D-Bus.";
    broker_.reset();
  }

  std::unique_ptr<Firewalld::ObjectManagerProxy> firewalld_object_manager_;
  std::unique_ptr<PermissionBroker> broker_;
  std::string access_group_;
  std::string udev_run_path_;
  int poll_interval_;

  base::WeakPtrFactory<Daemon> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(Daemon);
};

}  // namespace permission_broker

int main(int argc, char** argv) {
  DEFINE_string(access_group, "",
                "The group which has resource access granted to it. "
                "Must not be empty.");
  DEFINE_int32(poll_interval, 100,
               "The interval at which to poll for udev events.");
  DEFINE_string(udev_run_path, "/run/udev",
                "The path to udev's run directory.");

  brillo::FlagHelper::Init(argc, argv, "Chromium OS Permission Broker");
  brillo::InitLog(brillo::kLogToSyslog);

  permission_broker::Daemon daemon(FLAGS_access_group, FLAGS_udev_run_path,
                                   FLAGS_poll_interval);
  return daemon.Run();
}
