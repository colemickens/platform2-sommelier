// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/sshfs_helper.h"

#include <algorithm>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>

#include "cros-disks/fuse_mounter.h"
#include "cros-disks/mount_options.h"
#include "cros-disks/platform.h"
#include "cros-disks/uri.h"

namespace cros_disks {

namespace {

const char kUserName[] = "fuse-sshfs";
const char kHelperTool[] = "/usr/bin/sshfs";
const char kType[] = "sshfs";

const char kOptionFollowSymlinks[] = "follow_symlinks";
const char kOptionIdentityFile[] = "IdentityFile=";
const char kOptionUserKnownHostsFile[] = "UserKnownHostsFile=";
const char kOptionHostName[] = "HostName=";
const char kOptionPort[] = "Port=";

const char* const kEnforcedOptions[] = {
    "KbdInteractiveAuthentication=no",
    "PasswordAuthentication=no",
    "BatchMode=yes",
    FUSEHelper::kOptionAllowOther,
    FUSEHelper::kOptionDefaultPermissions,
};

const char* const kCopiedFiles[] = {
    kOptionIdentityFile, kOptionUserKnownHostsFile,
};

}  // namespace

SshfsHelper::SshfsHelper(const Platform* platform)
    : FUSEHelper(kType, platform, base::FilePath(kHelperTool), kUserName) {}

SshfsHelper::~SshfsHelper() = default;

std::unique_ptr<FUSEMounter> SshfsHelper::CreateMounter(
    const base::FilePath& working_dir,
    const Uri& source,
    const base::FilePath& target_path,
    const std::vector<std::string>& options) const {
  uid_t sshfs_uid, files_uid;
  gid_t sshfs_gid, files_gid;
  if (!platform()->GetUserAndGroupId(kUserName, &sshfs_uid, &sshfs_gid) ||
      !platform()->GetUserAndGroupId(kFilesUser, &files_uid, nullptr) ||
      !platform()->GetGroupId(kFilesGroup, &files_gid)) {
    LOG(ERROR) << "Invalid user configuration.";
    return nullptr;
  }

  std::vector<std::string> opts = options;
  if (!PrepareWorkingDirectory(working_dir, sshfs_uid, sshfs_gid, &opts)) {
    return nullptr;
  }

  MountOptions mount_options;
  for (const auto& opt : kEnforcedOptions) {
    mount_options.EnforceOption(opt);
  }
  mount_options.WhitelistOption(kOptionFollowSymlinks);
  mount_options.WhitelistOptionPrefix(kOptionIdentityFile);
  mount_options.WhitelistOptionPrefix(kOptionUserKnownHostsFile);
  mount_options.WhitelistOptionPrefix(kOptionHostName);
  mount_options.WhitelistOptionPrefix(kOptionPort);
  mount_options.Initialize(opts, true, base::IntToString(files_uid),
                           base::IntToString(files_gid));

  return std::make_unique<FUSEMounter>(source.path(), target_path.value(),
                                       type(), mount_options, platform(),
                                       program_path().value(), user(), true);
}

bool SshfsHelper::PrepareWorkingDirectory(
    const base::FilePath& working_dir,
    uid_t uid,
    gid_t gid,
    std::vector<std::string>* options) const {
  for (auto& opt : *options) {
    for (const auto& copied : kCopiedFiles) {
      if (base::StartsWith(opt, copied, base::CompareCase::SENSITIVE)) {
        size_t pos = opt.find('=');
        CHECK_NE(std::string::npos, pos) << "option has no value";

        base::FilePath src(opt.substr(pos + 1));
        if (!src.IsAbsolute() || src.ReferencesParent()) {
          LOG(WARNING) << "Insecure path in sshfs options.";
          return false;
        }

        base::FilePath dst = base::FilePath(working_dir).Append(src.BaseName());
        if (!platform()->CopyFile(src.value(), dst.value())) {
          LOG(ERROR) << "Can't copy file '" << src.value() << "'";
          return false;
        }

        if (!platform()->SetPermissions(dst.value(), 0600) ||
            !platform()->SetOwnership(dst.value(), uid, gid)) {
          LOG(ERROR) << "Can't change owner of the file copy '" << dst.value()
                     << "'";
          return false;
        }
        opt.replace(pos + 1, std::string::npos, dst.value());
        break;
      }
    }
  }

  // We retain group ownership on the directory to allow potential cleanup
  // of its contents.
  if (!platform()->SetPermissions(working_dir.value(), 0770) ||
      !platform()->SetOwnership(working_dir.value(), uid, getgid())) {
    LOG(ERROR) << "Can't set proper ownership of the working dir '"
               << working_dir.value() << "'";
    return false;
  }
  return true;
}

}  // namespace cros_disks
