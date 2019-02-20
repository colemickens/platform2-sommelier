// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unistd.h>

#include <memory>
#include <utility>

#include <base/files/scoped_file.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <brillo/flag_helper.h>
#include <brillo/key_value_store.h>
#include <brillo/syslog_logging.h>

#include "arc/network/helper_process.h"
#include "arc/network/ip_helper.h"
#include "arc/network/manager.h"

bool ShouldEnableMultinet() {
  static const char kLsbReleasePath[] = "/etc/lsb-release";
  static int kMinAndroidSdkVersion = 28;     // P
  static int kMinChromeMilestone = INT_MAX;  // TODO(garrick)

  brillo::KeyValueStore store;
  if (!store.Load(base::FilePath(kLsbReleasePath))) {
    LOG(ERROR) << "Could not read lsb-release";
    return false;
  }

  std::string value;
  if (!store.GetString("CHROMEOS_ARC_ANDROID_SDK_VERSION", &value)) {
    LOG(ERROR) << "ARC multi-networking disabled - cannot determine Android "
                  "SDK version";
    return false;
  }
  int ver = 0;
  if (!base::StringToInt(value.c_str(), &ver)) {
    LOG(ERROR) << "ARC multi-networking disabled - invalid Android SDK version";
    return false;
  }
  if (ver < kMinAndroidSdkVersion) {
    LOG(INFO) << "ARC multi-networking disabled for Android SDK " << value;
    return false;
  }
  if (!store.GetString("CHROMEOS_RELEASE_CHROME_MILESTONE", &value)) {
    LOG(ERROR)
        << "ARC multi-networking disabled - cannot determine Chrome milestone";
    return false;
  }
  if (atoi(value.c_str()) < kMinChromeMilestone) {
    LOG(INFO) << "ARC multi-networking disabled for Chrome M" << value;
    return false;
  }
  return true;
}

int main(int argc, char* argv[]) {
  DEFINE_bool(log_to_stderr, false, "Log to both syslog and stderr");
  DEFINE_int32(
      ip_helper_fd, -1,
      "Control socket for starting an IpHelper subprocess. Used internally.");
  DEFINE_string(force_multinet, "",
                "Override auto-detection and toggle multi-networking support.");

  brillo::FlagHelper::Init(argc, argv, "ARC network daemon");

  int flags = brillo::kLogToSyslog | brillo::kLogHeader;
  if (FLAGS_log_to_stderr)
    flags |= brillo::kLogToStderr;
  brillo::InitLog(flags);

  LOG(INFO) << "Starting arc-networkd "
            << (FLAGS_ip_helper_fd >= 0 ? "helper" : "manager");

  if (FLAGS_ip_helper_fd >= 0) {
    base::ScopedFD fd(FLAGS_ip_helper_fd);
    arc_networkd::IpHelper ip_helper{std::move(fd)};
    return ip_helper.Run();
  }

  auto ip_helper = std::make_unique<arc_networkd::HelperProcess>();
  ip_helper->Start(argc, argv, "--ip_helper_fd");

  bool enable_mnet =
      (FLAGS_force_multinet == "on") ||
      ((FLAGS_force_multinet != "off") && ShouldEnableMultinet());
  arc_networkd::Manager manager{std::move(ip_helper), enable_mnet};
  return manager.Run();
}
