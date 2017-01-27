// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <linux/capability.h>
#include <pwd.h>
#include <sys/capability.h>
#include <sys/prctl.h>
#include <sysexits.h>

#include <memory>

#include <base/at_exit.h>
#include <base/memory/ptr_util.h>
#include <base/sys_info.h>
#include <brillo/syslog_logging.h>
#include <brillo/daemons/dbus_daemon.h>
#include <chromeos/dbus/service_constants.h>
#include <install_attributes/libinstallattributes.h>

#include "authpolicy/authpolicy.h"
#include "authpolicy/constants.h"
#include "authpolicy/path_service.h"

namespace ac = authpolicy::constants;

uid_t ac::kAuthPolicydUid = ac::kInvalidUid;
uid_t ac::kAuthPolicyExecUid = ac::kInvalidUid;

namespace {

const char kObjectServicePath[] = "/org/chromium/AuthPolicy/ObjectManager";
const char kChromeOSReleaseTrack[] = "CHROMEOS_RELEASE_TRACK";
const char kBetaChannel[] = "beta-channel";
const char kStableChannel[] = "stable-channel";
const int kDefaultBufLen = 65536;
const char kAuthPolicydUser[] = "authpolicyd";
const char kAuthPolicydExecUser[] = "authpolicyd-exec";

// Gets a user id by name.
bool GetUserId(const char* user_name, uid_t* user_id) {
  // Load the passwd entry.
  int buf_len = sysconf(_SC_GETPW_R_SIZE_MAX);
  if (buf_len == -1)
    buf_len = kDefaultBufLen;
  struct passwd user_info, *user_infop;
  std::vector<char> buf(buf_len);
  if (getpwnam_r(user_name, &user_info, buf.data(), buf_len, &user_infop))
    return false;
  *user_id = user_info.pw_uid;
  return true;
}

// Sets authpolicy-exec as saved uid and drops caps. This way, Samba executables
// can be executed as authpolicy-exec even without keeping caps around. That's
// more secure.
bool SetSavedUserAndDropCaps() {
  // Initialize user ids.
  if (!GetUserId(kAuthPolicydUser, &ac::kAuthPolicydUid) ||
      !GetUserId(kAuthPolicydExecUser, &ac::kAuthPolicyExecUid)) {
    PLOG(ERROR) << "Failed to verify user ids";
    return false;
  }
  DCHECK_NE(ac::kAuthPolicydUid, ac::kInvalidUid);
  DCHECK_NE(ac::kAuthPolicyExecUid, ac::kInvalidUid);

  // Set authpolicyd-exec as saved uid.
  if (setresuid(ac::kAuthPolicydUid, ac::kAuthPolicydUid,
                ac::kAuthPolicyExecUid)) {
    PLOG(ERROR) << "setresuid failed";
    return false;
  }

  // Drop capabilities from bounding set.
  if (prctl(PR_CAPBSET_DROP, CAP_SETUID) ||
      prctl(PR_CAPBSET_DROP, CAP_SETPCAP)) {
    PLOG(ERROR) << "Failed to drop caps from bounding set";
    return false;
  }

  // Clear capabilities.
  cap_t caps = cap_get_proc();
  if (!caps ||
      cap_clear_flag(caps, CAP_INHERITABLE) ||
      cap_clear_flag(caps, CAP_EFFECTIVE) ||
      cap_clear_flag(caps, CAP_PERMITTED) ||
      cap_set_proc(caps)) {
    PLOG(ERROR) << "Clearing caps failed";
    return false;
  }

  return true;
}

}  // namespace

namespace authpolicy {

class Daemon : public brillo::DBusServiceDaemon {
 public:
  explicit Daemon(bool expect_config)
      : DBusServiceDaemon(kAuthPolicyServiceName, kObjectServicePath),
        expect_config_(expect_config) {}

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

  int OnInit() override {
    int return_code = brillo::DBusServiceDaemon::OnInit();
    if (return_code != EX_OK)
      return return_code;

    std::unique_ptr<PathService> path_service = base::MakeUnique<PathService>();
    if (!auth_policy_->Initialize(std::move(path_service), expect_config_)) {
      // Exit with "success" to prevent respawn by upstart.
      exit(0);
    }

    return EX_OK;
  }

 private:
  bool expect_config_;
  std::unique_ptr<AuthPolicy> auth_policy_;

  DISALLOW_COPY_AND_ASSIGN(Daemon);
};

}  // namespace authpolicy

int main(int argc, const char* const* argv) {
  brillo::OpenLog("authpolicyd", true);
  brillo::InitLog(brillo::kLogToSyslog);

  // Make it possible to switch to authpolicyd-exec without caps and drop caps.
  if (!SetSavedUserAndDropCaps()) {
    // Exit with "success" to prevent respawn by upstart.
    exit(0);
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
  bool expect_config = false;
  InstallAttributesReader install_attributes_reader;
  if (install_attributes_reader.IsLocked()) {
    const std::string& mode =
        install_attributes_reader.GetAttribute(
            InstallAttributesReader::kAttrMode);
    if (mode != InstallAttributesReader::kDeviceModeEnterpriseAD) {
      LOG(ERROR) << "OOBE completed but device not in Active Directory "
                    "management mode.";
      // Exit with "success" to prevent respawn by upstart.
      exit(0);
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
