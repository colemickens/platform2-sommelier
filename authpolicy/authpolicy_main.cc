// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include <base/at_exit.h>
#include <base/sys_info.h>
#include <brillo/syslog_logging.h>
#include <brillo/daemons/dbus_daemon.h>
#include <chromeos/dbus/service_constants.h>
#include <install_attributes/libinstallattributes.h>

#include "authpolicy/authpolicy.h"

namespace {

const char kObjectServicePath[] = "/org/chromium/AuthPolicy/ObjectManager";
const char kChromeOSReleaseTrack[] = "CHROMEOS_RELEASE_TRACK";
const char kBetaChannel[] = "beta-channel";
const char kStableChannel[] = "stable-channel";

}  // namespace

namespace authpolicy {

class Daemon : public brillo::DBusServiceDaemon {
 public:
  Daemon() : DBusServiceDaemon(kAuthPolicyServiceName, kObjectServicePath) {}

 protected:
  void RegisterDBusObjectsAsync(AsyncEventSequencer* sequencer) override {
    auth_policy_.reset(new AuthPolicy(object_manager_.get()));
    auth_policy_->RegisterAsync(
        sequencer->GetHandler("AuthPolicy.RegisterAsync() failed.", true));
  }

  void OnShutdown(int* return_code) override {
    DBusServiceDaemon::OnShutdown(return_code);
    auth_policy_.reset();
  }

 private:
  std::unique_ptr<AuthPolicy> auth_policy_;

  DISALLOW_COPY_AND_ASSIGN(Daemon);
};

}  // namespace authpolicy

int main(int argc, const char* const* argv) {
  brillo::OpenLog("authpolicyd", true);
  brillo::InitLog(brillo::kLogToSyslog);

  // Disable on beta and stable (for now).
  // TODO(ljusten): Reenable after launch reviews, see crbug.com/668119.
  {
    // base::SysInfo internally creates a singleton that adds itself to the
    // current AtExitManager, so that it is destroyed when the manager goes out
    // of scope. Daemon creates its own, so it has to be destroyed before the
    // daemon is run.
    base::AtExitManager at_exit_manager;
    std::string channel;
    if (!base::SysInfo::GetLsbReleaseValue(kChromeOSReleaseTrack, &channel)) {
      LOG(ERROR) << "Failed to retrieve release track from sys info.";
      // Exit with "success" to prevent respawn by upstart.
      exit(0);
    }
    if (channel == kBetaChannel || channel == kStableChannel) {
      LOG(ERROR) << "Not allowed to run on '" << kBetaChannel << "' and '"
                 << kStableChannel << "'.";
      // Exit with "success" to prevent respawn by upstart.
      exit(0);
    }
  }

  // Safety check to ensure that authpolicyd cannot run after the device has
  // been locked to a mode other than enterprise_ad.  (The lifetime management
  // of authpolicyd happens through upstart, this check only serves as a second
  // line of defense.)
  InstallAttributesReader install_attributes_reader;
  if (install_attributes_reader.IsLocked()) {
    const std::string& mode =
        install_attributes_reader.GetAttribute(
            InstallAttributesReader::kAttrMode);
    if (mode != InstallAttributesReader::kDeviceModeEnterpriseAD) {
      LOG(ERROR) << "OOBE completed but device not in AD management mode.";
      // Exit with "success" to prevent respawn by upstart.
      exit(0);
    }
  }

  // Run daemon.
  LOG(INFO) << "authpolicyd starting";
  authpolicy::Daemon daemon;
  int res = daemon.Run();
  LOG(INFO) << "authpolicyd stopping with exit code " << res;

  return res;
}
