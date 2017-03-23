// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include <base/at_exit.h>
#include <base/memory/ptr_util.h>
#include <base/sys_info.h>
#include <brillo/syslog_logging.h>
#include <brillo/daemons/dbus_daemon.h>
#include <install_attributes/libinstallattributes.h>

#include "authpolicy/authpolicy.h"
#include "authpolicy/constants.h"
#include "authpolicy/path_service.h"
#include "authpolicy/platform_helper.h"

namespace {

const char kObjectServicePath[] = "/org/chromium/AuthPolicy/ObjectManager";
const char kChromeOSReleaseTrack[] = "CHROMEOS_RELEASE_TRACK";
const char kBetaChannel[] = "beta-channel";
const char kStableChannel[] = "stable-channel";
const char kAuthPolicydUser[] = "authpolicyd";
const char kAuthPolicydExecUser[] = "authpolicyd-exec";

const int kExitCodeStartupFailure = 175;  // This number is hex AF.

}  // namespace

namespace authpolicy {

class Daemon : public brillo::DBusServiceDaemon {
 public:
  explicit Daemon(bool expect_config)
      : DBusServiceDaemon(kAuthPolicyServiceName, kObjectServicePath),
        expect_config_(expect_config) {}

 protected:
  void RegisterDBusObjectsAsync(AsyncEventSequencer* sequencer) override {
    auth_policy_ = base::MakeUnique<AuthPolicy>(
        AuthPolicy::GetDBusObject(object_manager_.get()),
        base::MakeUnique<AuthPolicyMetrics>(),
        base::MakeUnique<PathService>());
    auth_policy_->RegisterAsync(
        sequencer->GetHandler("AuthPolicy.RegisterAsync() failed.", true));
    ErrorType error = auth_policy_->Initialize(expect_config_);
    if (error != ERROR_NONE) {
      LOG(ERROR) << "SambaInterface failed to initialize with error code "
                 << error;
      exit(kExitCodeStartupFailure);
    }
  }

  void OnShutdown(int* return_code) override {
    DBusServiceDaemon::OnShutdown(return_code);
    auth_policy_.reset();
  }

 private:
  bool expect_config_;
  std::unique_ptr<AuthPolicy> auth_policy_;

  DISALLOW_COPY_AND_ASSIGN(Daemon);
};

}  // namespace authpolicy

int main(int /* argc */, char* /* argv */ []) {
  brillo::OpenLog("authpolicyd", true);
  brillo::InitLog(brillo::kLogToSyslog);

  // Verify we're running as authpolicyd user.
  uid_t authpolicyd_uid = authpolicy::GetUserId(kAuthPolicydUser);
  if (authpolicyd_uid != authpolicy::GetEffectiveUserId()) {
    LOG(ERROR) << "Failed to verify effective UID (must run as authpolicyd).";
    exit(kExitCodeStartupFailure);
  }

  // Make it possible to switch to authpolicyd-exec without caps and drop caps.
  uid_t authpolicyd_exec_uid = authpolicy::GetUserId(kAuthPolicydExecUser);
  if (!authpolicy::SetSavedUserAndDropCaps(authpolicyd_exec_uid)) {
    LOG(ERROR) << "Failed to establish user ids and drop caps.";
    exit(kExitCodeStartupFailure);
  }

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
      exit(kExitCodeStartupFailure);
    }
    if (channel == kBetaChannel || channel == kStableChannel) {
      LOG(ERROR) << "Not allowed to run on '" << kBetaChannel << "' and '"
                 << kStableChannel << "'.";
      exit(kExitCodeStartupFailure);
    }
  }

  // Safety check to ensure that authpolicyd cannot run after the device has
  // been locked to a mode other than enterprise_ad.  (The lifetime management
  // of authpolicyd happens through upstart, this check only serves as a second
  // line of defense.)
  bool expect_config = false;
  InstallAttributesReader install_attributes_reader;
  if (install_attributes_reader.IsLocked()) {
    const std::string& mode =
        install_attributes_reader.GetAttribute(
            InstallAttributesReader::kAttrMode);
    if (mode != InstallAttributesReader::kDeviceModeEnterpriseAD) {
      LOG(ERROR) << "OOBE completed but device not in Active Directory "
                    "management mode.";
      exit(kExitCodeStartupFailure);
    } else {
      LOG(INFO) << "Install attributes locked to Active Directory mode.";

      // A configuration file should be present in this case.
      expect_config = true;
    }
  } else {
    LOG(INFO) << "No install attributes found.";
  }

  // Run daemon.
  LOG(INFO) << "authpolicyd starting";
  authpolicy::Daemon daemon(expect_config);
  int res = daemon.Run();
  LOG(INFO) << "authpolicyd stopping with exit code " << res;

  return res;
}
