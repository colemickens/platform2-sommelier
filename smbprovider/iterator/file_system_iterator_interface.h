// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMBPROVIDER_ITERATOR_FILE_SYSTEM_ITERATOR_INTERFACE_H_
#define SMBPROVIDER_ITERATOR_FILE_SYSTEM_ITERATOR_INTERFACE_H_

#include <string>

namespace smbprovider {

struct DirectoryEntry;

class FileSystemIteratorInterface {
 public:
  // Initializes the iterator, setting the first value of current. Returns 0 on
  // success, error on failure. Must be called before any other operation.
  virtual int32_t Init() = 0;

  // Advances current to the next entry. Returns 0 on success,
  // error on failure.
  virtual int32_t Next() = 0;

  // Returns the current DirectoryEntry.
  virtual const DirectoryEntry& Get() = 0;

  // Returns true if there is nothing left to iterate over.
  virtual bool IsDone() = 0;

  virtual ~FileSystemIteratorInterface() = default;
};

}  // namespace smbprovider

#endif  // SMBPROVIDER_ITERATOR_FILE_SYSTEM_ITERATOR_INTERFACE_H_
