// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_NEWBLUED_UUID_H_
#define BLUETOOTH_NEWBLUED_UUID_H_

#include <stdint.h>
#include <array>
#include <string>
#include <vector>

namespace bluetooth {

// Sizes of UUIDs in different format in bytes.
constexpr size_t kUuid16Size = 2;
constexpr size_t kUuid32Size = 4;
constexpr size_t kUuid128Size = 16;

// The base UUID defined by Bluetooth SIG for constructing UUID128 from UUID16
// and UUID32.
constexpr std::array<uint8_t, 16> kUuidBase = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
    0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb,
};

// Possible representation formats of UUID in different bit length.
enum class UuidFormat {
  UUID_INVALID,
  UUID16,
  UUID32,
  UUID128,
};

// A wrapper around a Bluetooth universally unique identifier (UUID). UUIDs are
// adopted to identify pre-defined profiles, pre-definded attributes and
// user-specified services.
class Uuid {
 public:
  // |value| can be either a vector of 2, 4 or 16 bytes of UUID value in big
  // endian order. Note that UUID16 and UUID32 must be the assigned number
  // defined by Bluetooth SIG, so the base should be applied to generate
  // |value128_| for these two formats while UUID128 can be either assigned by
  // Bluetooth SIG or assigned by user applications.
  //
  // Here are some valid examples of |value|.
  // {0x18, 0x0F}: a 16-bit UUID representing the battery service
  // {0x00, 0x00, 0x18, 0x0F}: a 32-bit UUID representing the battery service
  // {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
  //  0x0C, 0x0D, 0x0E, 0x0F}: a 128-bit UUID represent a user-defined service
  explicit Uuid(const std::vector<uint8_t>& value);

  // Parses |uuid_str| into Uuid. Malformatted |uuid_str| results in
  // UUID_INVALID. Supported formats include:
  // UUID16:  xxxx
  // UUID32:  xxxxxxxx
  // UUID128: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
  explicit Uuid(const std::string& uuid_str);

  UuidFormat format() const { return format_; }
  // If |format_| is UuidFormat::UUID_INVALID, the return value is not valid.
  const std::array<uint8_t, 16>& value() const { return value128_; }
  // If |format_| is UuidFormat::UUID_INVALID, the return value is not valid.
  const std::string& canonical_value() const { return value_canonical_; }

  // Permit sufficient comparison to allow a UUID to be used as a key in a
  // std::map.
  bool operator<(const Uuid& uuid) const;

  // Equality operators.
  bool operator==(const Uuid& uuid) const;
  bool operator!=(const Uuid& uuid) const;

 private:
  // Format provided originally when the instance was constructed.
  UuidFormat format_;
  // The 128-bit UUID representation of the UUID.
  // Take GAP for instance, this is {0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x10,
  // 0x00, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb}.
  std::array<uint8_t, 16> value128_;
  // |value_128_| represented as a string in the following format:
  // xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
  // Take GAP for instance, this is 00001800-0000-1000-8000-00805f9b34fb.
  std::string value_canonical_;
};

}  // namespace bluetooth

#endif  // BLUETOOTH_NEWBLUED_UUID_H_
