// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_ARCHIVE_MANAGER_H_
#define CROS_DISKS_ARCHIVE_MANAGER_H_

#include <set>
#include <string>
#include <vector>

#include <gtest/gtest_prod.h>

#include "cros-disks/mount-manager.h"

namespace cros_disks {

// A derived class of MountManager for mounting archive files as a virtual
// filesystem.
class ArchiveManager : public MountManager {
 public:
  ArchiveManager(const std::string& mount_root, Platform* platform);

  // Initializes the manager and registers default file extensions.
  // Returns true on success.
  virtual bool Initialize();

  // Starts a session for |user|. Returns true on success.
  virtual bool StartSession(const std::string& user);

  // Stops a session for |user|. Returns true on success.
  virtual bool StopSession(const std::string& user);

  // Returns true if mounting |source_path| is supported.
  virtual bool CanMount(const std::string& source_path) const;

  // Returns the type of mount sources supported by the manager.
  virtual MountSourceType GetMountSourceType() const {
    return kMountSourceArchive;
  }

  // Returns true if an archive file extension is supported.
  bool IsFileExtensionSupported(const std::string& extension) const;

  // Registers a set of default archive file extensions to the manager.
  void RegisterDefaultFileExtensions();

  // Registers an archive file extension to the manager.
  // Subsequent registrations of the same file extension are ignored.
  // |extension| should not include the leading dot.
  void RegisterFileExtension(const std::string& extension);

 protected:
  // Mounts |source_path| to |mount_path| as |filesystem_type| with |options|.
  virtual MountErrorType DoMount(const std::string& source_path,
                                 const std::string& filesystem_type,
                                 const std::vector<std::string>& options,
                                 const std::string& mount_path);

  // Unmounts |path| with |options|. Returns true if |path| is unmounted
  // successfully.
  virtual MountErrorType DoUnmount(const std::string& path,
                                   const std::vector<std::string>& options);

  // Returns a suggested mount path for a source path.
  virtual std::string SuggestMountPath(const std::string& source_path) const;

 private:
  // Returns the corresponding path inside the AVFS mount of a given path,
  // or an empty string if the given path is not supported or has corresponding
  // path inside the AVFS mount.
  std::string GetAVFSPath(const std::string& path) const;

  // Mounts |base_path| to |avfs_path| via AVFS. Return true on success.
  bool MountAVFSPath(const std::string& base_path,
                     const std::string& avfs_path) const;

  // A set of supported archive file extensions.
  std::set<std::string> extensions_;

  FRIEND_TEST(ArchiveManagerTest, GetAVFSPath);
  FRIEND_TEST(ArchiveManagerTest, SuggestMountPath);
  FRIEND_TEST(ArchiveManagerTest,
              DoMountFailedWithExperimentalFeaturesDisabled);
  FRIEND_TEST(ArchiveManagerTest, DoMountFailedWithUnsupportedExtension);
};

}  // namespace cros_disks

#endif  // CROS_DISKS_ARCHIVE_MANAGER_H_
