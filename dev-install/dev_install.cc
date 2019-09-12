// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dev-install/dev_install.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>
#include <istream>
#include <string>
#include <vector>

#include <base/files/file_enumerator.h>
#include <base/files/file_path.h>
#include <base/logging.h>
#include <vboot/crossystem.h>

namespace dev_install {

namespace {

// The legacy dev_install shell script to be migrated here.
constexpr char kDevInstallScript[] = "/usr/share/dev-install/main.sh";

// The root path that we install our dev packages into.
constexpr char kUsrLocal[] = "/usr/local";

}  // namespace

DevInstall::DevInstall()
    : reinstall_(false),
      uninstall_(false),
      yes_(false),
      only_bootstrap_(false),
      state_dir_(kUsrLocal),
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
      state_dir_(kUsrLocal),
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

// We have a custom DeletePath helper because we don't want to descend across
// mount points, and no base:: helper supports that.
bool DevInstall::DeletePath(const struct stat& base_stat,
                            const base::FilePath& dir) {
  base::FileEnumerator iter(dir, true,
                            base::FileEnumerator::FILES |
                                base::FileEnumerator::DIRECTORIES |
                                base::FileEnumerator::SHOW_SYM_LINKS);
  for (base::FilePath current = iter.Next(); !current.empty();
       current = iter.Next()) {
    const auto& info(iter.GetInfo());
    if (info.IsDirectory()) {
      // Abort if the dir is mounted.
      if (base_stat.st_dev != info.stat().st_dev) {
        LOG(ERROR) << "directory is mounted: " << current.value();
        return false;
      }

      // Clear the contents of this directory.
      if (!DeletePath(base_stat, current))
        return false;

      // Clear this directory itself.
      if (rmdir(current.value().c_str())) {
        PLOG(ERROR) << "deleting failed: " << current.value();
        return false;
      }
    } else {
      if (unlink(current.value().c_str())) {
        PLOG(ERROR) << "deleting failed: " << current.value();
        return false;
      }
    }
  }

  return true;
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

  argv.push_back(nullptr);
  return Exec(argv);
}

}  // namespace dev_install
