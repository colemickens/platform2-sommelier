// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/newblued/uuid.h"

#include <algorithm>

#include <base/strings/stringprintf.h>
#include <base/strings/string_number_conversions.h>
#include <sys/types.h>

namespace bluetooth {

namespace {
std::vector<uint8_t> UuidStrToBytes(std::string uuid_str) {
  std::vector<uint8_t> value;
  // Remove only 4 '-'s in uuid 128.
  for (int i = 8; i < 24 && i < uuid_str.size(); i += 4) {
    if (uuid_str[i] != '-')
      return value;
    uuid_str.erase(i, 1);
  }
  base::HexStringToBytes(uuid_str, &value);
  return value;
}
}  // namespace

Uuid::Uuid() {
  format_ = UuidFormat::UUID_INVALID;
  value128_.fill(0);
  value_canonical_ = ValueToCanonical(value128_);
}

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
  value_canonical_ = ValueToCanonical(value128_);
}

Uuid::Uuid(const std::string& uuid_str) : Uuid(UuidStrToBytes(uuid_str)) {}

bool Uuid::operator<(const Uuid& uuid) const {
  return value128_ < uuid.value();
}

bool Uuid::operator==(const Uuid& uuid) const {
  return value128_ == uuid.value();
}

bool Uuid::operator!=(const Uuid& uuid) const {
  return value128_ != uuid.value();
}

std::string Uuid::ValueToCanonical(const std::array<uint8_t, 16>& value) {
  return base::StringPrintf(
      "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
      value[0], value[1], value[2], value[3], value[4], value[5], value[6],
      value[7], value[8], value[9], value[10], value[11], value[12], value[13],
      value[14], value[15]);
}

}  // namespace bluetooth
