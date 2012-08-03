// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MTPD_FILE_ENTRY_H_
#define MTPD_FILE_ENTRY_H_

#include <libmtp.h>

#include <dbus-c++/dbus.h>
#include <map>
#include <string>
#include <vector>

#include <base/basictypes.h>

namespace mtpd {

typedef std::map<std::string, DBus::Variant> DBusFileEntry;
typedef std::vector<DBusFileEntry> DBusFileEntries;

class FileEntry {
 public:
  explicit FileEntry(const LIBMTP_file_struct& file);
  ~FileEntry();

  DBusFileEntry ToDBusFormat() const;

 private:
  uint32_t item_id_;
  uint32_t parent_id_;
  std::string file_name_;
  uint64_t file_size_;
  time_t modification_date_;
  LIBMTP_filetype_t file_type_;
};

}  // namespace mtpd

#endif  // MTPD_FILE_ENTRY_H_
