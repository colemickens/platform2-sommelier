// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "settingsd/blob_ref.h"

#include <algorithm>

#include <base/logging.h>

namespace settingsd {

const uint8_t* BlobRef::kInvalidData = reinterpret_cast<uint8_t*>(1u);

BlobRef::BlobRef() : data_(kInvalidData), size_(0) {}

BlobRef::BlobRef(const uint8_t* data, size_t size) : data_(data), size_(size) {
  CHECK(data_ || !size_);
}

BlobRef::BlobRef(const std::vector<uint8_t>* data)
    : BlobRef(data->data(), data->size()) {}

BlobRef::BlobRef(const std::string* data)
    : BlobRef(reinterpret_cast<const uint8_t*>(data->data()), data->size()) {}

bool BlobRef::Equals(const BlobRef& that) const {
  CHECK(valid());
  CHECK(that.valid());
  return size() == that.size() &&
         std::equal(data(), data() + size(), that.data());
}

std::string BlobRef::ToString() const {
  CHECK_NE(kInvalidData, data_);
  if (!data_ || !size_)
    return std::string();
  return std::string(reinterpret_cast<const char*>(data_), size_);
}

std::vector<uint8_t> BlobRef::ToVector() const {
  CHECK_NE(kInvalidData, data_);
  if (!data_ || !size_)
    return std::vector<uint8_t>();
  return std::vector<uint8_t>(data_, data_ + size_);
}

}  // namespace settingsd
