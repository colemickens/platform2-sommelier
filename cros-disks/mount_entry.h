// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_MOUNT_ENTRY_H_
#define CROS_DISKS_MOUNT_ENTRY_H_

#include <string>
#include <vector>

#include <base/basictypes.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus-c++/dbus.h>  // NOLINT

namespace cros_disks {

typedef ::DBus::Struct<uint32, std::string, uint32, std::string> DBusMountEntry;
typedef std::vector<DBusMountEntry> DBusMountEntries;

class MountEntry {
 public:
  MountEntry(MountErrorType error_type,
             const std::string& source_path,
             MountSourceType source_type,
             const std::string& mount_path);
  ~MountEntry();

  DBusMountEntry ToDBusFormat() const;

  MountErrorType error_type() const { return error_type_; }
  const std::string& source_path() const { return source_path_; }
  MountSourceType source_type() const { return source_type_; }
  const std::string& mount_path() const { return mount_path_; }

 private:
  MountErrorType error_type_;
  std::string source_path_;
  MountSourceType source_type_;
  std::string mount_path_;
};

}  // namespace cros_disks

#endif  // CROS_DISKS_MOUNT_ENTRY_H_
