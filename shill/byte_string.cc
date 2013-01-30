// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/byte_string.h"

#include <netinet/in.h>

#include <base/string_number_conversions.h>

using std::string;

namespace shill {

unsigned char *ByteString::GetData() {
  return (GetLength() == 0) ? NULL : data_.data() + initial_offset_;
}

const unsigned char *ByteString::GetConstData() const {
  return (GetLength() == 0) ? NULL : data_.data() + initial_offset_;
}

ByteString ByteString::GetSubstring(size_t offset, size_t length) const {
  if (offset > GetLength()) {
    offset = GetLength();
  }
  if (length > GetLength() - offset) {
    length = GetLength() - offset;
  }
  return ByteString(GetConstData() + offset, length);
}

// static
ByteString ByteString::CreateFromCPUUInt32(uint32 val) {
  return ByteString(reinterpret_cast<unsigned char *>(&val), sizeof(val));
}

// static
ByteString ByteString::CreateFromNetUInt32(uint32 val) {
  return CreateFromCPUUInt32(htonl(val));
}

bool ByteString::ConvertToCPUUInt32(uint32 *val) const {
  if (val == NULL || GetLength() != sizeof(*val)) {
    return false;
  }
  memcpy(val, GetConstData(), sizeof(*val));

  return true;
}

bool ByteString::ConvertToNetUInt32(uint32 *val) const {
  if (!ConvertToCPUUInt32(val)) {
    return false;
  }
  *val = ntohl(*val);
  return true;
}

bool ByteString::IsZero() const {
  std::vector<unsigned char>::const_iterator i = data_.begin() +
      initial_offset_;
  for (; i != data_.end(); ++i) {
    if (*i != 0) {
      return false;
    }
  }
  return true;
}

bool ByteString::BitwiseAnd(const ByteString &b) {
  if (GetLength() != b.GetLength()) {
    return false;
  }
  std::vector<unsigned char>::iterator lhs = data_.begin() + initial_offset_;
  std::vector<unsigned char>::const_iterator rhs =
      b.data_.begin() + b.initial_offset_;
  for (size_t i = 0; i < GetLength(); ++i) {
    *lhs++ &= *rhs++;
  }
  return true;
}

bool ByteString::BitwiseOr(const ByteString &b) {
  if (GetLength() != b.GetLength()) {
    return false;
  }
  std::vector<unsigned char>::iterator lhs = data_.begin() + initial_offset_;
  std::vector<unsigned char>::const_iterator rhs =
      b.data_.begin() + b.initial_offset_;
  for (size_t i = 0; i < GetLength(); ++i) {
    *lhs++ |= *rhs++;
  }
  return true;
}

void ByteString::BitwiseInvert() {
  for (size_t i = initial_offset_; i < GetLength() + initial_offset_; ++i) {
    data_[i] = ~data_[i];
  }
}

bool ByteString::Equals(const ByteString &b) const {
  if (GetLength() != b.GetLength()) {
    return false;
  }
  std::vector<unsigned char>::const_iterator lhs =
      data_.begin() + initial_offset_;
  std::vector<unsigned char>::const_iterator rhs =
      b.data_.begin() + b.initial_offset_;
  for (size_t i = 0; i < GetLength(); ++i) {
    if (*lhs++ != *rhs++) {
      return false;
    }
  }
  return true;
}

void ByteString::Append(const ByteString &b) {
  data_.insert(data_.end(), b.data_.begin() + b.initial_offset_,
               b.data_.end());
}

string ByteString::HexEncode() const {
  return base::HexEncode(GetConstData(), GetLength());
}

void ByteString::ChopBeginningBytes(size_t offset) {
  if (offset >= GetLength()) {
    initial_offset_ = data_.size();
  } else {
    initial_offset_ += offset;
  }
}

}  // namespace shill
