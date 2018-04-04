// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_FUSE_HELPER_H_
#define CROS_DISKS_FUSE_HELPER_H_

#include <string>
#include <vector>

#include <base/macros.h>
#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest_prod.h>

namespace cros_disks {

class FUSEMounter;
class MountOptions;
class Platform;

// Base class to simplify calling of individual mounters based on
// specific conventions for a particular userspace FUSE implementation.
class FUSEHelper {
 public:
  // OS user that will access files provided by this module.
  static const char kFilesUser[];

  // OS group that will access files provided by this module.
  static const char kFilesGroup[];

  // FUSE kernel-level option allowing access to the mount by UIDs different
  // from the one that FUSE helper is being run. Relevant if FUSE helper is
  // being run in a sandbox under different UID.
  static const char kOptionAllowOther[];

  FUSEHelper(const std::string& fuse_type,
             const Platform* platform,
             const std::string& mount_program_path,
             const std::string& mount_user);
  virtual ~FUSEHelper();

  const std::string& type() const { return fuse_type_; }
  const std::string& user() const { return mount_user_; }
  const Platform* platform() const { return platform_; }

  // Whether this helper is able to handle this kind of source.
  virtual bool CanMount(const std::string& source) const = 0;

  // Derives suggested directory name for the mount point from the source.
  virtual std::string GetTargetSuffix(const std::string& source) const = 0;

  virtual MountErrorType Mount(const std::string& source,
                               const std::string& target_path,
                               const std::vector<std::string>& options) const;

 protected:
  virtual bool PrepareMountOptions(const std::vector<std::string>& options,
                                   uid_t uid,
                                   gid_t gid,
                                   MountOptions* mount_options) const;

  // Make sure the dir is set up to be used by FUSE's helper user.
  // Some FUSE modules may need secure and/or persistent storage of cached
  // data somewhere in cryptohome. This method provides implementations
  // with a shortcut for doing this.
  // This is approximately `chown user:chronos-access dirpath'.
  bool SetupDirectoryForFUSEAccess(const std::string& dirpath) const;

 private:
  FRIEND_TEST(FUSEHelperTest, PrepareMountOptions);
  friend class FUSEHelperTest;

  const std::string fuse_type_;
  const Platform* const platform_;
  const std::string mount_program_path_;
  const std::string mount_user_;

  DISALLOW_COPY_AND_ASSIGN(FUSEHelper);
};

}  // namespace cros_disks

#endif  // CROS_DISKS_FUSE_HELPER_H_
