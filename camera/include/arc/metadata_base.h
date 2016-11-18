/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef INCLUDE_ARC_METADATA_BASE_H_
#define INCLUDE_ARC_METADATA_BASE_H_

#include <string>
#include <vector>

#include <hardware/camera3.h>
#include <system/camera_metadata.h>

namespace arc {

// MetadataBase is a convenience class for dealing with libcamera_metadata
class MetadataBase {
 public:
  // Creates an empty object; best used when expecting to acquire contents
  // from elsewhere
  MetadataBase();
  // Takes ownership of passed-in buffer
  explicit MetadataBase(camera_metadata_t* buffer);
  // Clones the metadata
  MetadataBase(const MetadataBase& other);

  // Assignment clones metadata buffer.
  MetadataBase& operator=(const MetadataBase& other);
  MetadataBase& operator=(const camera_metadata_t* buffer);

  ~MetadataBase();

  // Get reference to the underlying metadata buffer. Ownership remains with
  // the MetadataBase object, but non-const MetadataBase methods will not
  // work until unlock() is called. Note that the lock has nothing to do with
  // thread-safety, it simply prevents the camera_metadata_t pointer returned
  // here from being accidentally invalidated by MetadataBase operations.
  const camera_metadata_t* GetAndLock() const;

  // Unlock the MetadataBase for use again. After this unlock, the pointer
  // given from getAndLock() may no longer be used. The pointer passed out
  // from getAndLock must be provided to guarantee that the right object is
  // being unlocked.
  int Unlock(const camera_metadata_t* buffer);

  // Release a raw metadata buffer to the caller. After this call,
  // MetadataBase no longer references the buffer, and the caller takes
  // responsibility for freeing the raw metadata buffer (using
  // free_camera_metadata()), or for handing it to another MetadataBase
  // instance.
  camera_metadata_t* Release();

  // Clear the metadata buffer and free all storage used by it.
  void Clear();

  // Acquire a raw metadata buffer from the caller. After this call,
  // the caller no longer owns the raw buffer, and must not free or manipulate
  // it. If MetadataBase already contains metadata, it is freed.
  void Acquire(camera_metadata_t* buffer);

  // Acquires raw buffer from other MetadataBase object. After the call, the
  // argument object no longer has any metadata.
  void Acquire(MetadataBase* other);

  // Append metadata from another MetadataBase object.
  int Append(const MetadataBase& other);

  // Append metadata from a raw camera_metadata buffer.
  int Append(const camera_metadata* other);

  // Number of metadata entries.
  size_t EntryCount() const;

  // Is the buffer empty (no entires).
  bool IsEmpty() const;

  // Sort metadata buffer for faster find.
  int Sort();

  // Update metadata entry. Will create entry if it doesn't exist already, and
  // will reallocate the buffer if insufficient space exists. Overloaded for
  // the various types of valid data.
  int Update(uint32_t tag, const uint8_t* data, size_t data_count);
  int Update(uint32_t tag, const int32_t* data, size_t data_count);
  int Update(uint32_t tag, const float* data, size_t data_count);
  int Update(uint32_t tag, const int64_t* data, size_t data_count);
  int Update(uint32_t tag, const double* data, size_t data_count);
  int Update(uint32_t tag,
             const camera_metadata_rational_t* data,
             size_t data_count);
  int Update(uint32_t tag, const std::string& string);

  template <typename T>
  int Update(uint32_t tag, std::vector<T> data) {
    return Update(tag, data.array(), data.size());
  }

  // Check if a metadata entry exists for a given tag id.
  bool Exists(uint32_t tag) const;

  // Get metadata entry by tag id.
  camera_metadata_entry Find(uint32_t tag);

  // Get metadata entry by tag id, with no editing.
  camera_metadata_ro_entry Find(uint32_t tag) const;

  // Delete metadata entry by tag.
  int Erase(uint32_t tag);

  // Dump contents into FD for debugging. The verbosity levels are
  // 0: Tag entry information only, no data values
  // 1: Level 0 plus at most 16 data values per entry
  // 2: All information
  // The indentation parameter sets the number of spaces to add to the start
  // each line of output.
  void Dump(int fd, int verbosity = 1, int indentation = 0) const;

 private:
  // Check if tag has a given type.
  int CheckType(uint32_t tag, uint8_t expected_type);

  // Base update entry method.
  int UpdateImpl(uint32_t tag, const void* data, size_t data_count);

  // Resize metadata buffer if needed by reallocating it and copying it over.
  int ResizeIfNeeded(size_t extra_entries, size_t extra_data);

  // Actual internal storage
  camera_metadata_t* buffer_;
  mutable bool locked_;
};

}  // namespace arc

#endif  // INCLUDE_ARC_METADATA_BASE_H_
