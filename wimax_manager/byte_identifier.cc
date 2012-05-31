// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_manager/byte_identifier.h"

using std::string;

namespace wimax_manager {

namespace {

const char kHexDigits[] = "0123456789abcdef";

}  // namespace

ByteIdentifier::ByteIdentifier(size_t length)
    : data_(length, 0) {
}

ByteIdentifier::ByteIdentifier(const uint8 *data, size_t length)
    : data_(data, data + length) {
}

ByteIdentifier::~ByteIdentifier() {
}

string ByteIdentifier::GetHexString() const {
  string hex_string;
  hex_string.reserve(data_.size() * 3);
  for (size_t i = 0; i < data_.size(); ++i) {
    if (i > 0) {
      hex_string.push_back(':');
    }
    hex_string.push_back(kHexDigits[data_[i] >> 4]);
    hex_string.push_back(kHexDigits[data_[i] & 0x0f]);
  }
  return hex_string;
}

void ByteIdentifier::CopyFrom(const ByteIdentifier &identifier) {
  data_ = identifier.data_;
}

}  // namespace wimax_manager
