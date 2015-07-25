// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SETTINGSD_BLOB_REF_H_
#define SETTINGSD_BLOB_REF_H_

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

namespace settingsd {

// A simple wrapper to refer to a blob of binary data. Note that BlobRef doesn't
// grab ownership of the memory that backs the BlobRef, so the object a BlobRef
// was initialized from needs to remain valid for the lifetime of the BlobRef.
class BlobRef {
 public:
  BlobRef() : data_(nullptr), size_(0) {}
  BlobRef(const uint8_t* data, size_t size) : data_(data), size_(size) {}
  explicit BlobRef(const std::vector<uint8_t>* data)
      : data_(data->data()), size_(data->size()) {}
  explicit BlobRef(const std::string* data)
      : data_(reinterpret_cast<const uint8_t*>(data->data())),
        size_(data->size()) {}

  const uint8_t* data() const { return data_; }
  size_t size() const { return size_; }

  std::string ToString() const {
    return std::string(reinterpret_cast<const char*>(data_), size_);
  }

 private:
  const uint8_t* const data_;
  const size_t size_;
};

}  // namespace settingsd

#endif  // SETTINGSD_BLOB_REF_H_
