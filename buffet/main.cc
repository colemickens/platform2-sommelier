// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <base/files/file_path.h>
#include <chromeos/dbus/async_event_sequencer.h>
#include <chromeos/dbus/exported_object_manager.h>
#include <chromeos/daemons/dbus_daemon.h>
#include <chromeos/flag_helper.h>
#include <chromeos/syslog_logging.h>

#include "buffet/dbus_constants.h"
#include "buffet/manager.h"

using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::DBusServiceDaemon;
using buffet::dbus_constants::kServiceName;
using buffet::dbus_constants::kRootServicePath;

namespace buffet {

class Daemon : public DBusServiceDaemon {
 public:
  Daemon(const base::FilePath& config_path,
         const base::FilePath& state_path)
      : DBusServiceDaemon(kServiceName, kRootServicePath),
        config_path_{config_path},
        state_path_{state_path} {}

 protected:
  void RegisterDBusObjectsAsync(AsyncEventSequencer* sequencer) override {
    manager_.reset(new buffet::Manager(object_manager_->AsWeakPtr()));
    manager_->RegisterAsync(
        config_path_,
        state_path_,
        sequencer->GetHandler("Manager.RegisterAsync() failed.", true));
  }

 private:
  std::unique_ptr<buffet::Manager> manager_;
  const base::FilePath config_path_;
  const base::FilePath state_path_;

  DISALLOW_COPY_AND_ASSIGN(Daemon);
};

}  // namespace buffet

namespace {

const char kDefaultConfigFilePath[] = "/etc/buffet/buffet.conf";
const char kDefaultStateFilePath[] = "/var/lib/buffet/device_reg_info";

}  // namespace

int main(int argc, char* argv[]) {
  DEFINE_string(config_path, kDefaultConfigFilePath,
                "Path to file containing config information.");
  DEFINE_string(state_path, kDefaultStateFilePath,
                "Path to file containing state information.");
  if (FLAGS_config_path.empty())
    FLAGS_config_path = kDefaultConfigFilePath;
  if (FLAGS_state_path.empty())
    FLAGS_state_path = kDefaultStateFilePath;
  chromeos::FlagHelper::Init(argc, argv, "Privet protocol handler daemon");
  chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogHeader);
  buffet::Daemon daemon{base::FilePath{FLAGS_config_path},
                        base::FilePath{FLAGS_state_path}};
  return daemon.Run();
}
