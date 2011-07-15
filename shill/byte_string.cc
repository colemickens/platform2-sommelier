// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/byte_string.h"

#include <netinet/in.h>

namespace shill {

// static
ByteString ByteString::CreateFromCPUUInt32(uint32 val) {
  return ByteString(reinterpret_cast<unsigned char *>(&val), sizeof(val));
}

// static
ByteString ByteString::CreateFromNetUInt32(uint32 val) {
  return CreateFromCPUUInt32(htonl(val));
}

bool ByteString::ConvertToCPUUInt32(uint32 *val) const {
  if (val == NULL || data_.size() != sizeof(*val)) {
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
  for (std::vector<unsigned char>::const_iterator i = data_.begin();
       i != data_.end(); ++i) {
    if (*i != 0) {
      return false;
    }
  }
  return true;
}

bool ByteString::Equals(const ByteString &b) const {
  return data_ == b.data_;
}

}  // namespace shill
