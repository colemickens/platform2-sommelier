// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/newblued/uuid.h"

#include <algorithm>

#include <base/strings/stringprintf.h>

namespace bluetooth {

Uuid::Uuid(const std::vector<uint8_t>& value) {
  switch (value.size()) {
    case kUuid16Size:
      format_ = UuidFormat::UUID16;
      value128_ = kUuidBase;
      std::copy(value.begin(), value.end(), value128_.begin() + 2);
      break;
    case kUuid32Size:
      format_ = UuidFormat::UUID32;
      value128_ = kUuidBase;
      std::copy(value.begin(), value.end(), value128_.begin());
      break;
    case kUuid128Size:
      format_ = UuidFormat::UUID128;
      std::copy(value.begin(), value.end(), value128_.begin());
      break;
    default:
      format_ = UuidFormat::UUID_INVALID;
      value128_.fill(0);
  }
  value_canonical_ = base::StringPrintf(
      "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
      value128_[0], value128_[1], value128_[2], value128_[3], value128_[4],
      value128_[5], value128_[6], value128_[7], value128_[8], value128_[9],
      value128_[10], value128_[11], value128_[12], value128_[13], value128_[14],
      value128_[15]);
}

bool Uuid::operator<(const Uuid& uuid) const {
  return value128_ < uuid.value();
}

bool Uuid::operator==(const Uuid& uuid) const {
  return value128_ == uuid.value();
}

bool Uuid::operator!=(const Uuid& uuid) const {
  return value128_ != uuid.value();
}

}  // namespace bluetooth
