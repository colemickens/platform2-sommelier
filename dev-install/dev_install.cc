// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dev-install/dev_install.h"

#include <unistd.h>

#include <iostream>
#include <istream>
#include <string>
#include <vector>

#include <base/logging.h>
#include <vboot/crossystem.h>

namespace dev_install {

namespace {

// The legacy dev_install shell script to be migrated here.
constexpr char kDevInstallScript[] = "/usr/share/dev-install/main.sh";

}  // namespace

DevInstall::DevInstall()
    : reinstall_(false),
      uninstall_(false),
      yes_(false),
      only_bootstrap_(false),
      binhost_(""),
      binhost_version_("") {}

DevInstall::DevInstall(const std::string& binhost,
                       const std::string& binhost_version,
                       bool reinstall,
                       bool uninstall,
                       bool yes,
                       bool only_bootstrap)
    : reinstall_(reinstall),
      uninstall_(uninstall),
      yes_(yes),
      only_bootstrap_(only_bootstrap),
      binhost_(binhost),
      binhost_version_(binhost_version) {}

bool DevInstall::IsDevMode() const {
  int value = ::VbGetSystemPropertyInt("cros_debug");
  return value == 1;
}

bool DevInstall::PromptUser(std::istream& input, const std::string& prompt) {
  if (yes_)
    return true;

  std::cout << prompt << "? (y/N) " << std::flush;

  std::string buffer;
  if (std::getline(input, buffer)) {
    return (buffer == "y" || buffer == "y\n");
  }
  return false;
}

int DevInstall::Exec(const std::vector<const char*>& argv) {
  execv(kDevInstallScript, const_cast<char* const*>(argv.data()));
  PLOG(ERROR) << kDevInstallScript << " failed";
  return EXIT_FAILURE;
}

int DevInstall::Run() {
  // Only run if dev mode is enabled.
  if (!IsDevMode()) {
    LOG(ERROR) << "Chrome OS is not in developer mode";
    return 2;
  }

  std::vector<const char*> argv{kDevInstallScript};

  if (!binhost_.empty()) {
    argv.push_back("--binhost");
    argv.push_back(binhost_.c_str());
  }

  if (!binhost_version_.empty()) {
    argv.push_back("--binhost_version");
    argv.push_back(binhost_version_.c_str());
  }

  if (reinstall_)
    argv.push_back("--reinstall");

  if (uninstall_)
    argv.push_back("--uninstall");

  if (yes_)
    argv.push_back("--yes");

  if (only_bootstrap_)
    argv.push_back("--only_bootstrap");

  return Exec(argv);
}

}  // namespace dev_install
