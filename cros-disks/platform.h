// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_PLATFORM_H_
#define CROS_DISKS_PLATFORM_H_

#include <sys/stat.h>

#include <string>

#include <base/basictypes.h>

namespace cros_disks {

// A class that provides functionalities such as creating and removing
// directories, and getting user ID and group ID for a username.
class Platform {
 public:
  Platform();
  virtual ~Platform();

  // Creates a directory at |path| if it does not exist. Returns true on
  // success.
  virtual bool CreateDirectory(const std::string& path) const;

  // Creates a directory at |path| if it does not exist. If |path| already
  // exists and is a directory, this function tries to reuse it if it is empty
  // not in use. The created directory is only accessible by the current user.
  // Returns true if the directory is created successfully.
  virtual bool CreateOrReuseEmptyDirectory(const std::string& path) const;

  // Creates a directory at |path| similar to CreateDirectory() but retries
  // on failure by augmenting a numeric suffix (e.g. "mydir (1)"), starting
  // from 1 to |max_suffix_to_retry|, to the directory name. The created
  // directory is only accessible by the current user. Returns true if the
  // directory is created successfully.
  virtual bool CreateOrReuseEmptyDirectoryWithFallback(
      std::string* path, unsigned max_suffix_to_retry) const;

  // Gets the user ID and group ID for a given username. Returns true on
  // success.
  virtual bool GetUserAndGroupId(const std::string& username,
                                 uid_t *uid, gid_t *gid) const;

  // Removes a directory at |path| if it is empty and not in use.
  // Returns true on success.
  virtual bool RemoveEmptyDirectory(const std::string& path) const;

  // Sets the user ID and group ID of |path| to |user_id| and |group_id|,
  // respectively. Returns true on success.
  virtual bool SetOwnership(const std::string& path,
                            uid_t user_id, gid_t group_id) const;

  // Sets the permissions of |path| to |mode|. Returns true on success.
  virtual bool SetPermissions(const std::string& path, mode_t mode) const;

  // Unmounts |path|. Returns true on success.
  virtual bool Unmount(const std::string& path) const;

  bool experimental_features_enabled() const {
    return experimental_features_enabled_;
  }
  void set_experimental_features_enabled(bool experimental_features_enabled) {
    experimental_features_enabled_ = experimental_features_enabled;
  }

 private:
  // This variable is set to true if the experimental features are enabled.
  bool experimental_features_enabled_;

  DISALLOW_COPY_AND_ASSIGN(Platform);
};

}  // namespace cros_disks

#endif  // CROS_DISKS_PLATFORM_H_
