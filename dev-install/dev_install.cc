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
#include <base/files/file_util.h>
#include <base/logging.h>
#include <brillo/key_value_store.h>
#include <vboot/crossystem.h>

namespace dev_install {

namespace {

// The legacy dev_install shell script to be migrated here.
constexpr char kDevInstallScript[] = "/usr/share/dev-install/main.sh";

// The root path that we install our dev packages into.
constexpr char kUsrLocal[] = "/usr/local";

// The Portage profile path as a subdir under the various roots.
constexpr char kPortageProfileSubdir[] = "etc/portage";

// Path to lsb-release file.
constexpr char kLsbReleasePath[] = "/etc/lsb-release";

// The devserer URL for this developer build.
constexpr char kLsbChromeosDevserver[] = "CHROMEOS_DEVSERVER";

// The current OS version.
constexpr char kLsbChromeosReleaseVersion[] = "CHROMEOS_RELEASE_VERSION";

// Setting for the board name.
constexpr char kLsbChromeosReleaseBoard[] = "CHROMEOS_RELEASE_BOARD";

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

bool DevInstall::ClearStateDir(const base::FilePath& dir) {
  LOG(INFO) << "To clean up, we will run:\n  rm -rf /usr/local/\n"
            << "Any content you have stored in there will be deleted.";
  if (!PromptUser(std::cin, "Remove all installed packages now")) {
    LOG(INFO) << "Operation cancelled.";
    return false;
  }

  // Normally we'd use base::DeleteFile, but we don't want to traverse mounts.
  struct stat base_stat;
  if (stat(dir.value().c_str(), &base_stat)) {
    if (errno == ENOENT)
      return true;

    PLOG(ERROR) << "Could not access " << dir.value();
    return false;
  }

  bool success = DeletePath(base_stat, dir);
  if (success)
    LOG(INFO) << "Removed all installed packages.";
  else
    LOG(ERROR) << "Deleting " << dir.value() << " failed";
  return success;
}

bool DevInstall::InitializeStateDir(const base::FilePath& dir) {
  // Create this loop so uncompressed files in /usr/local/usr/ will be reachable
  // through /usr/local/.
  // Note: /usr/local is mount-binded onto the /mnt/stateful_partition/dev_mode
  // during chromeos_startup during boot for machines in dev_mode.
  const base::FilePath usr = dir.Append("usr");
  if (!base::PathExists(usr)) {
    // Create /usr/local/usr -> . symlink.
    if (!base::CreateSymbolicLink(base::FilePath("."), usr)) {
      PLOG(ERROR) << "Creating " << usr.value() << " failed";
      return false;
    }
  }

  const base::FilePath local = usr.Append("local");
  if (!base::PathExists(local)) {
    // Create /usr/local/usr/local -> . symlink.
    if (!base::CreateSymbolicLink(base::FilePath("."), local)) {
      PLOG(ERROR) << "Creating " << local.value() << " failed";
      return false;
    }
  }

  // Set up symlinks for etc/{group,passwd}, so that packages can look up users
  // and groups correctly.
  const base::FilePath etc = usr.Append("etc");
  if (!base::PathExists(etc)) {
    if (!base::CreateDirectory(etc) ||
        !base::SetPosixFilePermissions(etc, 0755)) {
      PLOG(ERROR) << "Creating " << etc.value() << " failed";
      return false;
    }
  }

  // Create /usr/local/etc/group -> /etc/group symlink.
  const base::FilePath group = etc.Append("group");
  if (!base::PathExists(group)) {
    if (!base::CreateSymbolicLink(base::FilePath("/etc/group"), group)) {
      PLOG(ERROR) << "Creating " << group.value() << " failed";
      return false;
    }
  }

  // Create /usr/local/etc/passwd -> /etc/passwd symlink.
  const base::FilePath passwd = etc.Append("passwd");
  if (!base::PathExists(passwd)) {
    if (!base::CreateSymbolicLink(base::FilePath("/etc/passwd"), passwd)) {
      PLOG(ERROR) << "Creating " << passwd.value() << " failed";
      return false;
    }
  }

  return true;
}

bool DevInstall::LoadRuntimeSettings(const base::FilePath& lsb_release) {
  brillo::KeyValueStore store;
  if (!store.Load(lsb_release)) {
    PLOG(WARNING) << "Could not read " << kLsbReleasePath;
    return true;
  }

  if (!store.GetString(kLsbChromeosDevserver, &devserver_url_))
    devserver_url_.clear();

  if (store.GetString(kLsbChromeosReleaseBoard, &board_)) {
    size_t pos = board_.find("-signed-");
    if (pos != std::string::npos)
      board_.erase(pos);
  } else {
    board_.clear();
  }

  // If --binhost_version wasn't specified, calculate it.
  if (binhost_version_.empty()) {
    store.GetString(kLsbChromeosReleaseVersion, &binhost_version_);
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

  // Handle reinstall & uninstall operations.
  if (reinstall_ || uninstall_) {
    if (!ClearStateDir(state_dir_))
      return 1;
    if (uninstall_)
      return 0;

    LOG(INFO) << "Reinstalling dev state";
  }

  // See if the system has been initialized already.
  const base::FilePath state_dir(state_dir_);
  const base::FilePath profile = state_dir.Append(kPortageProfileSubdir);
  if (base::DirectoryExists(profile)) {
    LOG(ERROR) << "Directory " << profile.value() << " exists.";
    LOG(ERROR) << "Did you mean dev_install --reinstall?";
    return 4;
  }

  // Initialize the base set of paths before we install any packages.
  if (!InitializeStateDir(state_dir_))
    return 5;

  // Load the settings from the active device.
  if (!LoadRuntimeSettings(base::FilePath(kLsbReleasePath)))
    return 6;

  std::vector<const char*> argv{kDevInstallScript};

  if (!binhost_.empty()) {
    argv.push_back("--binhost");
    argv.push_back(binhost_.c_str());
  }

  if (!binhost_version_.empty()) {
    argv.push_back("--binhost_version");
    argv.push_back(binhost_version_.c_str());
  }

  if (yes_)
    argv.push_back("--yes");

  if (only_bootstrap_)
    argv.push_back("--only_bootstrap");

  argv.push_back(nullptr);
  return Exec(argv);
}

}  // namespace dev_install
