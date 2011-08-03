// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_MOUNT_MANAGER_H_
#define CROS_DISKS_MOUNT_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include <base/basictypes.h>
#include <gtest/gtest_prod.h>

#include "cros-disks/service-constants.h"

namespace cros_disks {

class Platform;

// A base class for managing mounted filesystems of certain kinds.
// It provides template methods for mounting and unmounting filesystems.
// A derived class implements pure virtual methods CanMount, DoMount,
// DoUnmount and SuggestMountPath to provide specific operations for
// supporting certain kinds of filesystem.
class MountManager {
 public:
  MountManager(const std::string& mount_root, Platform* platform);
  virtual ~MountManager();

  // Initializes the mount manager. Returns true on success.
  // This base class provides a default implementation that creates the
  // mount root directory. A derived class can override this method to
  // perform any necessary initialization.
  virtual bool Initialize();

  // Starts a session for |user|. Returns true on success.
  // This base class provides a default implementation that does nothing.
  // A derived class can override this method to perform any necessary
  // operations when a session starts.
  virtual bool StartSession(const std::string& user);

  // Stops a session for |user|. Returns true on success.
  // This base class provides a default implementation that does nothing.
  // A derived class can override this method to perform any necessary
  // operations when a session stops.
  virtual bool StopSession(const std::string& user);

  // Implemented by a derived class to return true if it supports mounting
  // |source_path|.
  virtual bool CanMount(const std::string& source_path) const = 0;

  // Returns true if unmounting |path| is supported by the manager.
  // This base class provides a default implementation such that a path
  // can be unmounted by the manager if it can be mounted by the manager
  // or is an immediate child of the mount root directory where mount
  // directories are created. A derived class can override this method to
  // support additional paths.
  virtual bool CanUnmount(const std::string& path) const;

  // Implemented by a derived class to return the type of mount sources
  // it supports.
  virtual MountSourceType GetMountSourceType() const = 0;

  // Mounts |source_path| to |mount_path| as |filesystem_type| with |options|.
  // If |mount_path| is an empty string, SuggestMountPath() is called to
  // obtain a suggested mount path. |mount_path| is set to actual mount path
  // on success.
  MountErrorType Mount(const std::string& source_path,
                       const std::string& filesystem_type,
                       const std::vector<std::string>& options,
                       std::string *mount_path);

  // Unmounts |path|, which can be a source path or a mount path,
  // with |options|.
  MountErrorType Unmount(const std::string& path,
                         const std::vector<std::string>& options);

  // Unmounts all mounted paths.
  bool UnmountAll();

  // Adds a mapping from |source_path| to |mount_path| to the cache.
  // Returns false if |source_path| is already in the cache.
  bool AddMountPathToCache(const std::string& source_path,
                           const std::string& mount_path);

  // Gets the corresponding |mount_path| of |source_path| from the cache.
  // Returns false if |source_path| is not found in the cache.
  bool GetMountPathFromCache(const std::string& source_path,
                             std::string* mount_path) const;

  // Returns true if |mount_path| is found in the cache.
  bool IsMountPathInCache(const std::string& mount_path) const;

  // Removes |mount_path| from the cache. Returns false if |mount_path|
  // is not found in the cache.
  bool RemoveMountPathFromCache(const std::string& mount_path);

 protected:
  // Type definition of a cache mapping a source path to its mount path of
  // filesystems mounted by the manager.
  typedef std::map<std::string, std::string> MountPathMap;

  // Implemented by a derived class to mount |source_path| to |mount_path|
  // as |filesystem_type| with |options|.
  virtual MountErrorType DoMount(const std::string& source_path,
                                 const std::string& filesystem_type,
                                 const std::vector<std::string>& options,
                                 const std::string& mount_path) = 0;

  // Implemented by a derived class to unmount |path| with |options|.
  virtual MountErrorType DoUnmount(const std::string& path,
                                   const std::vector<std::string>& options) = 0;

  // Returns a suggested mount path for |source_path|.
  virtual std::string SuggestMountPath(
      const std::string& source_path) const = 0;

  // Extracts unmount flags for umount() from an array of options.
  virtual bool ExtractUnmountOptions(const std::vector<std::string>& options,
                                     int *unmount_flags) const;

  // Returns true if |path| is an immediate child of |parent|, i.e.
  // |path| is an immediate file or directory under |parent|.
  bool IsPathImmediateChildOfParent(const std::string& path,
                                    const std::string& parent) const;

  // A cache mapping a source path to its mount path of filesystems mounted
  // by the manager.
  MountPathMap mount_paths_;

  // The root directory under which mount directories are created.
  std::string mount_root_;

  // An object that provides platform service.
  Platform* platform_;

 private:
  FRIEND_TEST(MountManagerTest, ExtractSupportedUnmountOptions);
  FRIEND_TEST(MountManagerTest, ExtractUnsupportedUnmountOptions);
  FRIEND_TEST(MountManagerTest, IsPathImmediateChildOfParent);

  DISALLOW_COPY_AND_ASSIGN(MountManager);
};

}  // namespace cros_disks

#endif  // CROS_DISKS_MOUNT_MANAGER_H_
