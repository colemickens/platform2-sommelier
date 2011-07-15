// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_BYTE_STRING_
#define SHILL_BYTE_STRING_

#include <vector>

#include <base/basictypes.h>

namespace shill {

// Provides a holder of a string of bytes
class ByteString {
 public:
  ByteString(const ByteString &b) : data_(b.data_) {}
  explicit ByteString(size_t length) : data_(length) {}
  ByteString(const unsigned char *data, size_t length)
      : data_(data, data + length) {}

  ByteString &operator=(const ByteString &b) {
    data_ = b.data_;
    return *this;
  }

  // Inserts a uint32 into a ByteString in cpu-order
  static ByteString CreateFromCPUUInt32(uint32 val);
  // Inserts a uint32 into a ByteString in network-order
  static ByteString CreateFromNetUInt32(uint32 val);

  unsigned char *GetData() { return data_.data(); }
  const unsigned char *GetConstData() const { return data_.data(); }
  size_t GetLength() const { return data_.size(); }

  // Converts to a uint32 from a host-order value stored in the ByteString
  // Returns true on success
  bool ConvertToCPUUInt32(uint32 *val) const;
  // Converts to a uint32 from a network-order value stored in the ByteString
  // Returns true on success
  bool ConvertToNetUInt32(uint32 *val) const;

  bool IsZero() const;
  bool Equals(const ByteString &b) const;

 private:
  std::vector<unsigned char> data_;
  // NO DISALLOW_COPY_AND_ASSIGN -- we assign ByteStrings in STL hashes
};

}  // namespace shill


#endif  // SHILL_BYTE_STRING_
