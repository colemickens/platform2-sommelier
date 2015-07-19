// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <base/files/file_path.h>
#include <chromeos/dbus/async_event_sequencer.h>
#include <chromeos/dbus/exported_object_manager.h>
#include <chromeos/daemons/dbus_daemon.h>
#include <chromeos/flag_helper.h>
#include <chromeos/strings/string_utils.h>
#include <chromeos/syslog_logging.h>

#include "buffet/dbus_constants.h"
#include "buffet/manager.h"

using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::DBusServiceDaemon;
using buffet::kServiceName;
using buffet::kRootServicePath;

namespace buffet {

class Daemon final : public DBusServiceDaemon {
 public:
  explicit Daemon(const weave::Device::Options& options)
      : DBusServiceDaemon(kServiceName, kRootServicePath), options_{options} {}

 protected:
  void RegisterDBusObjectsAsync(AsyncEventSequencer* sequencer) override {
    manager_.reset(new buffet::Manager(object_manager_->AsWeakPtr()));
    manager_->Start(options_, sequencer);
  }

  void OnShutdown(int* return_code) override { manager_->Stop(); }

 private:
  weave::Device::Options options_;
  std::unique_ptr<buffet::Manager> manager_;
  DISALLOW_COPY_AND_ASSIGN(Daemon);
};

}  // namespace buffet

namespace {

const char kDefaultConfigFilePath[] = "/etc/buffet/buffet.conf";
const char kDefaultStateFilePath[] = "/var/lib/buffet/device_reg_info";

}  // namespace

int main(int argc, char* argv[]) {
  DEFINE_bool(log_to_stderr, false, "log trace messages to stderr as well");
  DEFINE_string(config_path, kDefaultConfigFilePath,
                "Path to file containing config information.");
  DEFINE_string(state_path, kDefaultStateFilePath,
                "Path to file containing state information.");
  DEFINE_bool(enable_xmpp, true,
              "Connect to GCD via a persistent XMPP connection.");
  DEFINE_bool(disable_privet, false, "disable Privet protocol");
  DEFINE_bool(enable_ping, false, "enable test HTTP handler at /privet/ping");
  DEFINE_string(device_whitelist, "",
                "Comma separated list of network interfaces to monitor for "
                "connectivity (an empty list enables all interfaces).");

  DEFINE_bool(disable_security, false,
              "disable Privet security for tests. For test only.");
  DEFINE_string(test_privet_ssid, "",
                "Fixed SSID for WiFi bootstrapping. For test only.");
  DEFINE_string(test_definitions_path, "",
                "Path to directory containing additional command "
                "and state definitions. For test only.");

  chromeos::FlagHelper::Init(argc, argv, "Privet protocol handler daemon");
  if (FLAGS_config_path.empty())
    FLAGS_config_path = kDefaultConfigFilePath;
  if (FLAGS_state_path.empty())
    FLAGS_state_path = kDefaultStateFilePath;
  int flags = chromeos::kLogToSyslog | chromeos::kLogHeader;
  if (FLAGS_log_to_stderr)
    flags |= chromeos::kLogToStderr;
  chromeos::InitLog(flags);

  auto device_whitelist =
      chromeos::string_utils::Split(FLAGS_device_whitelist, ",", true, true);

  weave::Device::Options options;
  options.config_path = base::FilePath{FLAGS_config_path};
  options.state_path = base::FilePath{FLAGS_state_path};
  options.definitions_path = base::FilePath{"/etc/buffet"};
  options.test_definitions_path = base::FilePath{FLAGS_test_definitions_path};
  options.xmpp_enabled = FLAGS_enable_xmpp;
  options.device_whitelist.insert(device_whitelist.begin(),
                                  device_whitelist.end());
  options.disable_privet = FLAGS_disable_privet;
  options.disable_security = FLAGS_disable_security;
  options.enable_ping = FLAGS_enable_ping;
  options.test_privet_ssid = FLAGS_test_privet_ssid;

  buffet::Daemon daemon{options};
  return daemon.Run();
}
