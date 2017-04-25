// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/dev_features_tool.h"

#include <functional>

#include <base/bind.h>
#include <base/callback.h>
#include <chromeos/dbus/service_constants.h>

#include "debugd/src/process_with_output.h"

namespace debugd {

namespace {

using ArgList = ProcessWithOutput::ArgList;

const char kDefaultRootPassword[] = "test0000";

const char kDevFeaturesErrorString[] = "org.chromium.debugd.error.DevFeatures";
const char kRootfsLockedErrorString[] =
    "Rootfs verification must be removed first";

// Executes a helper process with the expectation that any message printed to
// stderr indicates a failure that should be passed back over the D-Bus.
int RunHelper(const std::string& command,
              const ArgList& arguments,
              bool requires_root,
              const std::string* stdin,
              DBus::Error* error) {
  std::string stderr;
  int result = ProcessWithOutput::RunHelper(command,
                                            arguments,
                                            requires_root,
                                            stdin,
                                            nullptr,  // Don't need stdout.
                                            &stderr,
                                            error);
  if (!stderr.empty()) {
    if (error && !error->is_set()) {
      error->set(kDevFeaturesErrorString, stderr.c_str());
    }
  }
  return result;
}

bool RemoveRootfsVerificationQuery(DBus::Error* error) {
  return RunHelper("dev_features_rootfs_verification", ArgList{"-q"},
                   true,  // requires root to check if / is writable by root.
                   nullptr,  // no stdin.
                   error) == 0;
}

bool EnableBootFromUsbQuery(DBus::Error* error) {
  return RunHelper("dev_features_usb_boot", ArgList{"-q"},
                   true,  // requires root for crossystem queries.
                   nullptr,  // no stdin.
                   error) == 0;
}

bool ConfigureSshServerQuery(DBus::Error* error) {
  return RunHelper("dev_features_ssh", ArgList{"-q"},
                   true,  // needs root to check for files in 700 folders.
                   nullptr,  // no stdin.
                   error) == 0;
}

bool EnableChromeRemoteDebuggingQuery(DBus::Error* error) {
  return RunHelper("dev_features_chrome_remote_debugging", ArgList{"-q"},
                   false,
                   nullptr,  // no stdin.
                   error) == 0;
}

bool SetUserPasswordQuery(const std::string& username,
                          bool system,
                          DBus::Error* error) {
  ArgList args{"-q", "--user=" + username};
  if (system)
    args.push_back("--system");

  return RunHelper("dev_features_password", args,
                   true,  // requires root to read either password file.
                   nullptr,  // no stdin.
                   error) == 0;
}

}  // namespace

void DevFeaturesTool::RemoveRootfsVerification(DBus::Error* error) const {
  RunHelper("dev_features_rootfs_verification", ArgList{},
            true,  // requires root for make_dev_ssd.sh script.
            nullptr,  // no stdin.
            error);
}

void DevFeaturesTool::EnableBootFromUsb(DBus::Error* error) const {
  RunHelper("dev_features_usb_boot", ArgList{},
            true,  // requires root for enable_dev_usb_boot script.
            nullptr,  // no stdin.
            error);
}

void DevFeaturesTool::ConfigureSshServer(DBus::Error* error) const {
  // SSH server configuration requires writing to rootfs.
  if (RemoveRootfsVerificationQuery(error)) {
    RunHelper("dev_features_ssh", ArgList{},
              true,  // requires root to write to rootfs directories.
              nullptr,  // no stdin.
              error);
  } else if (!error->is_set()) {
    error->set(kDevFeaturesErrorString, kRootfsLockedErrorString);
  }
}

void DevFeaturesTool::EnableChromeRemoteDebugging(DBus::Error* error) const {
  if (RemoveRootfsVerificationQuery(error)) {
    RunHelper("dev_features_chrome_remote_debugging", ArgList{},
              true,  // requires root to write to rootfs directories.
              nullptr,  // no stdin.
              error);
  } else if (!error->is_set()) {
    error->set(kDevFeaturesErrorString, kRootfsLockedErrorString);
  }
}

void DevFeaturesTool::SetUserPassword(const std::string& username,
                                      const std::string& password,
                                      DBus::Error* error) const {
  ArgList args{"--user=" + username};

  // Set the devmode password regardless of rootfs verification state.
  RunHelper("dev_features_password", args,
            true,  // requires root to write devmode password file.
            &password,  // pipe the password through stdin.
            error);
  if (!error->is_set()) {
    // If rootfs is unlocked, we should set the system password as well.
    if (RemoveRootfsVerificationQuery(error)) {
      args.push_back("--system");
      RunHelper("dev_features_password", args,
                true,  // requires root to write system password file.
                &password,  // pipe the password through stdin.
                error);
    }
  }
}

void DevFeaturesTool::EnableChromeDevFeatures(const std::string& root_password,
                                              DBus::Error* error) const {
  EnableBootFromUsb(error);
  if (error->is_set()) {
    return;
  }
  ConfigureSshServer(error);
  if (error->is_set()) {
    return;
  }
  SetUserPassword("root",
                  root_password.empty() ? kDefaultRootPassword : root_password,
                  error);
}

namespace {

struct Query {
  using Function = base::Callback<bool(DBus::Error*)>;

  Function function;
  DevFeatureFlag flag;
};

}  // namespace

int32_t DevFeaturesTool::QueryDevFeatures(DBus::Error* error) const {
  Query queries[] = {
      {base::Bind(&RemoveRootfsVerificationQuery),
            DEV_FEATURE_ROOTFS_VERIFICATION_REMOVED},
      {base::Bind(&EnableBootFromUsbQuery),
            DEV_FEATURE_BOOT_FROM_USB_ENABLED},
      {base::Bind(&EnableChromeRemoteDebuggingQuery),
            DEV_FEATURE_CHROME_REMOTE_DEBUGGING_ENABLED},
      {base::Bind(&ConfigureSshServerQuery),
            DEV_FEATURE_SSH_SERVER_CONFIGURED},
      {base::Bind(&SetUserPasswordQuery, "root", /* system = */ false),
            DEV_FEATURE_DEV_MODE_ROOT_PASSWORD_SET},
      {base::Bind(&SetUserPasswordQuery, "root", /* system = */ true),
            DEV_FEATURE_SYSTEM_ROOT_PASSWORD_SET}
  };

  int32_t flags = 0;
  for (const auto& query : queries) {
    if (query.function.Run(error))
      flags |= query.flag;
    // D-Bus is only set up to handle a single error so exit as soon as we
    // hit one.
    if (error->is_set())
      return 0;
  }
  return flags;
}

}  // namespace debugd
