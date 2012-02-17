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
  ArchiveManager(const std::string& mount_root, Platform* platform,
                 Metrics* metrics);
  virtual ~ArchiveManager();

  // Initializes the manager and registers default file extensions.
  // Returns true on success.
  virtual bool Initialize();

  // Stops a session for |user|. Returns true on success.
  virtual bool StopSession(const std::string& user);

  // Returns true if mounting |source_path| is supported.
  virtual bool CanMount(const std::string& source_path) const;

  // Returns the type of mount sources supported by the manager.
  virtual MountSourceType GetMountSourceType() const {
    return MOUNT_SOURCE_ARCHIVE;
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
  // Type definition of a cache mapping a mount path to its source virtual path
  // in the AVFS mount.
  typedef std::map<std::string, std::string> VirtualPathMap;

  // Returns the extension of a file, in lower case, at the specified |path|.
  std::string GetFileExtension(const std::string& path) const;

  // Returns the corresponding path inside the AVFS mount of a given path,
  // or an empty string if the given path does not have a corresponding
  // path inside the AVFS mount.
  std::string GetAVFSPath(const std::string& path) const;

  // Starts AVFS daemons to initialize AVFS mounts. Returns true on success or
  // if the AVFS daemons have started.
  bool StartAVFS();

  // Stops AVFS daemons and unmounts AVFS mounts. Returns true on success or if
  // the AVFS daemons have not yet started.
  bool StopAVFS();

  // Mounts |base_path| to |avfs_path| via AVFS. Return true on success.
  bool MountAVFSPath(const std::string& base_path,
                     const std::string& avfs_path) const;

  // Adds a mapping of |mount_path| to |virtual_path| to |virtual_paths_|.
  // An existing mapping of |mount_path| is overwritten.
  void AddMountVirtualPath(const std::string& mount_path,
                           const std::string& virtual_path);

  // Removes a mapping of |mount_path| to its virtual path from
  // |virtual_paths_|. It is a no-op if no such mapping exists.
  void RemoveMountVirtualPath(const std::string& mount_path);

  // A set of supported archive file extensions.
  std::set<std::string> extensions_;

  // A cache mapping a mount path to its source virtual path in the AVFS mount.
  VirtualPathMap virtual_paths_;

  // This variable is set to true if the AVFS daemons have started.
  bool avfs_started_;

  FRIEND_TEST(ArchiveManagerTest, GetAVFSPath);
  FRIEND_TEST(ArchiveManagerTest, GetAVFSPathWithNestedArchives);
  FRIEND_TEST(ArchiveManagerTest, GetFileExtension);
  FRIEND_TEST(ArchiveManagerTest, SuggestMountPath);
  FRIEND_TEST(ArchiveManagerTest, DoMountFailedWithUnsupportedExtension);
};

}  // namespace cros_disks

#endif  // CROS_DISKS_ARCHIVE_MANAGER_H_
