// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mtpd/file_entry.h"

#include <base/time.h>
#include <chromeos/dbus/service_constants.h>

#include "string_helpers.h"

namespace mtpd {

FileEntry::FileEntry(const LIBMTP_file_struct& file)
    : item_id_(file.item_id),
      parent_id_(file.parent_id),
      file_size_(file.filesize),
      modification_date_(file.modificationdate),
      file_type_(file.filetype) {
  if (file.filename)
    file_name_ = file.filename;
}

FileEntry::~FileEntry() {
}

DBusFileEntry FileEntry::ToDBusFormat() const {
  DBusFileEntry entry;
  entry[kItemId].writer().append_uint32(item_id_);
  entry[kParentId].writer().append_uint32(parent_id_);
  entry[kFileName].writer().append_string(
      EnsureUTF8String(file_name_).c_str());
  entry[kFileSize].writer().append_uint64(file_size_);
  entry[kModificationDate].writer().append_int64(
      base::Time::FromTimeT(modification_date_).ToInternalValue());
  entry[kFileType].writer().append_uint16(file_type_);
  return entry;
}

}  // namespace mtpd
