// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIMAX_MANAGER_BYTE_IDENTIFIER_H_
#define WIMAX_MANAGER_BYTE_IDENTIFIER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include <base/basictypes.h>

namespace wimax_manager {

class ByteIdentifier {
 public:
  explicit ByteIdentifier(size_t length);
  ByteIdentifier(const uint8_t *data, size_t length);
  ~ByteIdentifier();

  const std::vector<uint8_t> &data() const { return data_; }

  std::string GetHexString() const;

  void CopyFrom(const ByteIdentifier &identifier);

 private:
  std::vector<uint8_t> data_;

  DISALLOW_COPY_AND_ASSIGN(ByteIdentifier);
};

}  // namespace wimax_manager

#endif  // WIMAX_MANAGER_BYTE_IDENTIFIER_H_
