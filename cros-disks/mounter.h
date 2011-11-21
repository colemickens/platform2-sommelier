// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_MOUNTER_H_
#define CROS_DISKS_MOUNTER_H_

#include <string>

#include <base/basictypes.h>
#include <chromeos/dbus/service_constants.h>

#include "cros-disks/mount-options.h"

namespace cros_disks {

// A base class for mounting a filesystem.
// This class (and its derived classes) does not handle unmounting
// of the filesystem.
class Mounter {
 public:
  Mounter(const std::string& source_path,
          const std::string& target_path,
          const std::string& filesystem_type,
          const MountOptions& mount_options);

  virtual ~Mounter();

  // This method implements the common steps to mount a filesystem.
  // It internally calls MountImpl() on a derived class.
  MountErrorType Mount();

  const std::string& filesystem_type() const { return filesystem_type_; }
  void set_filesystem_type(const std::string& filesystem_type) {
    filesystem_type_ = filesystem_type;
  }

  const std::string& source_path() const { return source_path_; }
  void set_source_path(const std::string& source_path) {
    source_path_ = source_path;
  }

  const std::string& target_path() const { return target_path_; }
  void set_target_path(const std::string& target_path) {
    target_path_ = target_path;
  }

  const MountOptions& mount_options() const { return mount_options_; }
  void set_mount_options(const MountOptions& mount_options) {
    mount_options_ = mount_options;
  }

 protected:
  // This pure virtual method is implemented by a derived class to mount
  // a filesystem.
  virtual MountErrorType MountImpl() = 0;

 private:
  // Type of filesystem to mount.
  std::string filesystem_type_;

  // Source path to mount from.
  std::string source_path_;

  // Target path where the filesystem is mounted to.
  std::string target_path_;

  // Mount options.
  MountOptions mount_options_;

  DISALLOW_COPY_AND_ASSIGN(Mounter);
};

}  // namespace cros_disks

#endif  // CROS_DISKS_MOUNTER_H_
