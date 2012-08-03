// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mtpd/file_entry.h"

#include <base/time.h>

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
  // TODO(thestig) Move string constants to system_api/dbus/service_constants.h.
  DBusFileEntry entry;
  entry["ItemId"].writer().append_uint32(item_id_);
  entry["ParentId"].writer().append_uint32(parent_id_);
  entry["FileName"].writer().append_string(
      EnsureUTF8String(file_name_).c_str());
  entry["FileSize"].writer().append_uint64(file_size_);
  entry["ModificationDate"].writer().append_int64(
      base::Time::FromTimeT(modification_date_).ToInternalValue());
  entry["FileType"].writer().append_uint16(file_type_);
  return entry;
}

}  // namespace mtpd
