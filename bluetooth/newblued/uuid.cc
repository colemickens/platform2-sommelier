// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/newblued/uuid.h"

#include <algorithm>

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
