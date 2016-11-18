/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "arc/metadata_base.h"

#include <system/camera_metadata.h>

#include "arc/common.h"

namespace arc {

MetadataBase::MetadataBase() : buffer_(NULL), locked_(false) {}

MetadataBase::MetadataBase(camera_metadata_t* buffer)
    : buffer_(NULL), locked_(false) {
  Acquire(buffer);
}

MetadataBase::MetadataBase(const MetadataBase& other) : locked_(false) {
  buffer_ = clone_camera_metadata(other.buffer_);
}

MetadataBase& MetadataBase::operator=(const MetadataBase& other) {
  return operator=(other.buffer_);
}

MetadataBase& MetadataBase::operator=(const camera_metadata_t* buffer) {
  if (locked_) {
    LOGF(ERROR) << "Assignment to a locked MetadataBase!";
    return *this;
  }

  if (buffer != buffer_) {
    camera_metadata_t* newBuffer = clone_camera_metadata(buffer);
    Clear();
    buffer_ = newBuffer;
  }
  return *this;
}

MetadataBase::~MetadataBase() {
  locked_ = false;
  Clear();
}

const camera_metadata_t* MetadataBase::GetAndLock() const {
  locked_ = true;
  return buffer_;
}

int MetadataBase::Unlock(const camera_metadata_t* buffer) {
  if (!locked_) {
    LOGF(ERROR) << "Can't unlock a non-locked MetadataBase!";
    return -EINVAL;
  }
  if (buffer != buffer_) {
    LOGF(ERROR) << "Can't unlock MetadataBase with wrong pointer!";
    return -EINVAL;
  }
  locked_ = false;
  return 0;
}

camera_metadata_t* MetadataBase::Release() {
  if (locked_) {
    LOGF(ERROR) << "MetadataBase is locked";
    return NULL;
  }
  camera_metadata_t* released = buffer_;
  buffer_ = NULL;
  return released;
}

void MetadataBase::Clear() {
  if (locked_) {
    LOGF(ERROR) << "MetadataBase is locked";
    return;
  }
  if (buffer_) {
    free_camera_metadata(buffer_);
    buffer_ = NULL;
  }
}

void MetadataBase::Acquire(camera_metadata_t* buffer) {
  if (locked_) {
    LOGF(ERROR) << "MetadataBase is locked";
    return;
  }
  Clear();
  buffer_ = buffer;

  if (!validate_camera_metadata_structure(buffer_, /*size*/ NULL)) {
    LOGF(ERROR) << "Failed to validate metadata structure " << buffer;
  }
}

void MetadataBase::Acquire(MetadataBase* other) {
  if (locked_) {
    LOGF(ERROR) << "MetadataBase is locked";
    return;
  }
  Acquire(other->Release());
}

int MetadataBase::Append(const MetadataBase& other) {
  return Append(other.buffer_);
}

int MetadataBase::Append(const camera_metadata_t* other) {
  if (locked_) {
    LOGF(ERROR) << "MetadataBase is locked";
    return -EBUSY;
  }
  size_t extra_entries = get_camera_metadata_entry_count(other);
  size_t extra_data = get_camera_metadata_data_count(other);
  ResizeIfNeeded(extra_entries, extra_data);

  return append_camera_metadata(buffer_, other);
}

size_t MetadataBase::EntryCount() const {
  return (buffer_ == NULL) ? 0 : get_camera_metadata_entry_count(buffer_);
}

bool MetadataBase::IsEmpty() const {
  return EntryCount() == 0;
}

int MetadataBase::Sort() {
  if (locked_) {
    LOGF(ERROR) << "MetadataBase is locked";
    return -EBUSY;
  }
  return sort_camera_metadata(buffer_);
}

int MetadataBase::CheckType(uint32_t tag, uint8_t expected_type) {
  int tagType = get_camera_metadata_tag_type(tag);
  if (tagType == -1) {
    LOGF(ERROR) << "Update metadata entry: Unknown tag " << tag;
    return -EINVAL;
  }
  if (tagType != expected_type) {
    LOGF(ERROR) << "Mismatched tag type when updating entry "
                << get_camera_metadata_tag_name(tag) << " (" << tag
                << ") of type " << camera_metadata_type_names[tagType]
                << "; got type " << camera_metadata_type_names[expected_type]
                << " data instead ";
    return -EINVAL;
  }
  return 0;
}

int MetadataBase::Update(uint32_t tag, const int32_t* data, size_t data_count) {
  int res;
  if (locked_) {
    LOGF(ERROR) << "MetadataBase is locked";
    return -EBUSY;
  }
  if ((res = CheckType(tag, TYPE_INT32))) {
    return res;
  }
  return UpdateImpl(tag, (const void*)data, data_count);
}

int MetadataBase::Update(uint32_t tag, const uint8_t* data, size_t data_count) {
  int res;
  if (locked_) {
    LOGF(ERROR) << "MetadataBase is locked";
    return -EBUSY;
  }
  if ((res = CheckType(tag, TYPE_BYTE))) {
    return res;
  }
  return UpdateImpl(tag, (const void*)data, data_count);
}

int MetadataBase::Update(uint32_t tag, const float* data, size_t data_count) {
  int res;
  if (locked_) {
    LOGF(ERROR) << "MetadataBase is locked";
    return -EBUSY;
  }
  if ((res = CheckType(tag, TYPE_FLOAT))) {
    return res;
  }
  return UpdateImpl(tag, (const void*)data, data_count);
}

int MetadataBase::Update(uint32_t tag, const int64_t* data, size_t data_count) {
  int res;
  if (locked_) {
    LOGF(ERROR) << "MetadataBase is locked";
    return -EBUSY;
  }
  if ((res = CheckType(tag, TYPE_INT64))) {
    return res;
  }
  return UpdateImpl(tag, (const void*)data, data_count);
}

int MetadataBase::Update(uint32_t tag, const double* data, size_t data_count) {
  int res;
  if (locked_) {
    LOGF(ERROR) << "MetadataBase is locked";
    return -EBUSY;
  }
  if ((res = CheckType(tag, TYPE_DOUBLE))) {
    return res;
  }
  return UpdateImpl(tag, (const void*)data, data_count);
}

int MetadataBase::Update(uint32_t tag,
                         const camera_metadata_rational_t* data,
                         size_t data_count) {
  int res;
  if (locked_) {
    LOGF(ERROR) << "MetadataBase is locked";
    return -EBUSY;
  }
  if ((res = CheckType(tag, TYPE_RATIONAL))) {
    return res;
  }
  return UpdateImpl(tag, (const void*)data, data_count);
}

int MetadataBase::Update(uint32_t tag, const std::string& string) {
  int res;
  if (locked_) {
    LOGF(ERROR) << "MetadataBase is locked";
    return -EBUSY;
  }
  if ((res = CheckType(tag, TYPE_BYTE))) {
    return res;
  }
  // string.size() doesn't count the null termination character.
  return UpdateImpl(tag, (const void*)string.c_str(), string.size() + 1);
}

int MetadataBase::UpdateImpl(uint32_t tag,
                             const void* data,
                             size_t data_count) {
  int res;
  if (locked_) {
    LOGF(ERROR) << "MetadataBase is locked";
    return -EBUSY;
  }
  int type = get_camera_metadata_tag_type(tag);
  if (type == -1) {
    LOGF(ERROR) << "Tag " << tag << " not found";
    return -EINVAL;
  }
  // Safety check - ensure that data isn't pointing to this metadata, since
  // that would get invalidated if a resize is needed
  size_t buffer_size = get_camera_metadata_size(buffer_);
  uintptr_t buf_addr = reinterpret_cast<uintptr_t>(buffer_);
  uintptr_t data_addr = reinterpret_cast<uintptr_t>(data);
  if (data_addr > buf_addr && data_addr < (buf_addr + buffer_size)) {
    LOGF(ERROR) << "Update attempted with data from the same metadata buffer!";
    return -EINVAL;
  }

  size_t data_size =
      calculate_camera_metadata_entry_data_size(type, data_count);

  res = ResizeIfNeeded(1, data_size);

  if (res == 0) {
    camera_metadata_entry_t entry;
    res = find_camera_metadata_entry(buffer_, tag, &entry);
    if (res == -ENOENT) {
      res = add_camera_metadata_entry(buffer_, tag, data, data_count);
    } else if (res == 0) {
      res = update_camera_metadata_entry(buffer_, entry.index, data, data_count,
                                         NULL);
    }
  }

  if (res) {
    LOGF(ERROR) << "Unable to update metadata entry "
                << get_camera_metadata_section_name(tag) << "."
                << get_camera_metadata_tag_name(tag) << " (" << tag
                << "): " << strerror(-res) << " (" << res << ")";
  }

  if (validate_camera_metadata_structure(buffer_, /*size*/ NULL)) {
    LOGF(ERROR) << "Failed to validate metadata structure after update "
                << buffer_;
  }

  return res;
}

bool MetadataBase::Exists(uint32_t tag) const {
  camera_metadata_ro_entry entry;
  return find_camera_metadata_ro_entry(buffer_, tag, &entry) == 0;
}

camera_metadata_entry_t MetadataBase::Find(uint32_t tag) {
  int res;
  camera_metadata_entry entry;
  if (locked_) {
    LOGF(ERROR) << "MetadataBase is locked";
    entry.count = 0;
    return entry;
  }
  res = find_camera_metadata_entry(buffer_, tag, &entry);
  if (res) {
    entry.count = 0;
    entry.data.u8 = NULL;
  }
  return entry;
}

camera_metadata_ro_entry_t MetadataBase::Find(uint32_t tag) const {
  int res;
  camera_metadata_ro_entry entry;
  res = find_camera_metadata_ro_entry(buffer_, tag, &entry);
  if (res) {
    entry.count = 0;
    entry.data.u8 = NULL;
  }
  return entry;
}

int MetadataBase::Erase(uint32_t tag) {
  camera_metadata_entry_t entry;
  int res;
  if (locked_) {
    LOGF(ERROR) << "MetadataBase is locked";
    return -EBUSY;
  }
  res = find_camera_metadata_entry(buffer_, tag, &entry);
  if (res == -ENOENT) {
    return 0;
  } else if (res) {
    LOGF(ERROR) << "Error looking for entry "
                << get_camera_metadata_section_name(tag) << "."
                << get_camera_metadata_tag_name(tag) << " (" << tag
                << "): " << strerror(-res) << " " << res;
    return res;
  }
  res = delete_camera_metadata_entry(buffer_, entry.index);
  if (res) {
    LOGF(ERROR) << "Error deleting entry "
                << get_camera_metadata_section_name(tag) << "."
                << get_camera_metadata_tag_name(tag) << " (" << tag
                << "): " << strerror(-res) << " " << res;
  }
  return res;
}

void MetadataBase::Dump(int fd, int verbosity, int indentation) const {
  dump_indented_camera_metadata(buffer_, fd, verbosity, indentation);
}

int MetadataBase::ResizeIfNeeded(size_t extra_entries, size_t extra_data) {
  if (buffer_ == NULL) {
    buffer_ = allocate_camera_metadata(extra_entries * 2, extra_data * 2);
    if (buffer_ == NULL) {
      LOGF(ERROR) << "Can't allocate larger metadata buffer";
      return -ENOMEM;
    }
  } else {
    size_t current_entry_count = get_camera_metadata_entry_count(buffer_);
    size_t current_entry_cap = get_camera_metadata_entry_capacity(buffer_);
    size_t new_entry_count = current_entry_count + extra_entries;
    new_entry_count = (new_entry_count > current_entry_cap)
                          ? new_entry_count * 2
                          : current_entry_cap;

    size_t current_data_count = get_camera_metadata_data_count(buffer_);
    size_t current_data_cap = get_camera_metadata_data_capacity(buffer_);
    size_t new_data_count = current_data_count + extra_data;
    new_data_count = (new_data_count > current_data_cap) ? new_data_count * 2
                                                         : current_data_cap;

    if (new_entry_count > current_entry_cap ||
        new_data_count > current_data_cap) {
      camera_metadata_t* old_buffer = buffer_;
      buffer_ = allocate_camera_metadata(new_entry_count, new_data_count);
      if (buffer_ == NULL) {
        LOGF(ERROR) << "Can't allocate larger metadata buffer";
        return -ENOMEM;
      }
      append_camera_metadata(buffer_, old_buffer);
      free_camera_metadata(old_buffer);
    }
  }
  return 0;
}

}  // namespace arc
