// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "portier/ll_address.h"

#include <ctype.h>
#include <net/if_arp.h>
#include <stdio.h>

#include <algorithm>

#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <base/strings/string_util.h>

namespace portier {

using shill::ByteString;
using std::string;

using Type = LLAddress::Type;

namespace {

uint8_t ParseHexDigit(char c) {
  DCHECK(base::IsHexDigit(c));
  if (base::IsAsciiDigit(c)) {
    return c - '0';
  }
  if (base::IsAsciiLower(c)) {
    return c - 'a' + 10;
  }
  return c - 'A' + 10;
}

// Given a specified link-layer address type and a string
// representation of the address, this function will attempt to
// extract the bytes of the address.
//
// EUI-48 and EUI-64 are very similar in format, only difference being
// their lengths (6 bytes for EUI-48 and 8 bytes for EUI-64).  The
// accepted format is a sequences of hexadecimal character pais,
// representing a byte, each byte is separated by a colon ':'.
// No leading or trailing white space is allowed.
//
// EUI-48: xx:xx:xx:xx:xx:xx
// EUI-64: xx:xx:xx:xx:xx:xx:xx:xx
//
// Function returns true if the input string was parsed correctly
// and false otherwise.
bool ParseLinkLayerAddressString(LLAddress::Type type,
                                 string address_string,
                                 uint8_t* out) {
  // Covert to lower case.
  address_string = base::ToLowerASCII(address_string);

  // Ensure that all the characters provided are part of the address.
  //  |hex_count| tracks that each octet is 2 hex digits.
  //  |octet_count| counts the number of octets.
  uint32_t hex_count = 0;
  uint32_t octet_count = 0;
  for (const char c : address_string) {
    if (base::IsHexDigit(c) && hex_count < 2) {
      // If it is a hexadecimal, there cannot be more than 2 per byte.
      hex_count++;
    } else if (c == ':' && hex_count == 2) {
      // If there is a colon, it must come after 2 hexadecimal characters.
      hex_count = 0;
      octet_count++;
    } else {
      // All other cases are invalid.
      return false;
    }
  }

  // Check that there was at least one hexadecimal after the final
  // octet separator.
  if (hex_count != 2) {
    return false;
  }
  octet_count++;

  // Verify length.
  if (!((type == Type::kEui48 && octet_count == 6) ||
        (type == Type::kEui64 && octet_count == 8))) {
    return false;
  }

  const char* in_cursor = address_string.c_str();
  uint8_t* out_cursor = out;
  while (*in_cursor) {
    uint8_t upper = ParseHexDigit(*in_cursor++);
    uint8_t lower = ParseHexDigit(*in_cursor++);
    *out_cursor++ = (upper << 4) | lower;
    if (*in_cursor && *in_cursor == ':') {
      in_cursor++;
    }
  }
  return true;
}

}  // namespace

string LLAddress::GetTypeName(LLAddress::Type type) {
  switch (type) {
    case Type::kEui48:
      return "EUI-48";
    case Type::kEui64:
      return "EUI-64";
    default:
      return "unknown";
  }
}

int32_t LLAddress::GetTypeLength(LLAddress::Type type) {
  switch (type) {
    case Type::kEui48:
      return 6;
    case Type::kEui64:
      return 8;
    default:
      return -1;
  }
}

uint16_t LLAddress::GetTypeArpType(Type type) {
  switch (type) {
    case Type::kEui48:
      return ARPHRD_ETHER;
    case Type::kEui64:
      return ARPHRD_EUI64;
    default:
      return ARPHRD_VOID;
  }
}

// Constructors.

LLAddress::LLAddress() : type_(Type::kInvalid), address_(0) {}

LLAddress::LLAddress(LLAddress::Type type) : type_(type), address_(0) {
  const int32_t len = GetTypeLength(type);
  if (len <= 0) {
    // |type| is invalid.
    type_ = Type::kInvalid;
  } else {
    address_.Resize(len);
  }
}

LLAddress::LLAddress(LLAddress::Type type, const ByteString& address)
    : type_(type), address_(address) {
  // Check if invalid.
  const int32_t len = GetTypeLength(type);
  if (len <= 0 || address.GetLength() != len) {
    type_ = Type::kInvalid;
    address_.Clear();
  }
}

LLAddress::LLAddress(LLAddress::Type type, const string& address)
    : type_(type), address_(0) {
  const int32_t len = GetTypeLength(type);
  if (len <= 0) {
    type_ = Type::kInvalid;
    return;
  }

  address_.Resize(len);
  if (!ParseLinkLayerAddressString(type, address, address_.GetData())) {
    type_ = Type::kInvalid;
    address_.Clear();
  }
}

LLAddress::LLAddress(const struct sockaddr_ll* address_struct) {
  if (address_struct == nullptr) {
    type_ = Type::kInvalid;
    return;
  }

  if (GetTypeArpType(Type::kEui48) == address_struct->sll_hatype &&
      6 == address_struct->sll_halen) {
    type_ = Type::kEui48;
    address_ = ByteString(address_struct->sll_addr, 6);
  } else if (GetTypeArpType(Type::kEui64) == address_struct->sll_hatype &&
             8 == address_struct->sll_halen) {
    type_ = Type::kEui64;
    address_ = ByteString(address_struct->sll_addr, 8);
  } else {
    // Assume invalid.
    type_ = Type::kInvalid;
  }
}

LLAddress::~LLAddress() = default;

// Copy constructor.

LLAddress::LLAddress(const LLAddress& other)
    : type_(other.type_), address_(other.address_) {}

LLAddress& LLAddress::operator=(const LLAddress& other) {
  type_ = other.type_;
  address_ = other.address_;
  return *this;
}

// Getters.

uint16_t LLAddress::GetArpType() const {
  return GetTypeArpType(type());
}

const uint8_t* LLAddress::GetConstData() const {
  return address_.GetConstData();
}

uint32_t LLAddress::GetLength() const {
  return address_.GetLength();
}

// Address information

bool LLAddress::IsValid() const {
  return Type::kInvalid != type_;
}

bool LLAddress::IsUnicast() const {
  // Is a Unicast if the least significant bit in the first byte is 0.
  if (!IsValid()) {
    return false;
  }
  const uint8_t first = GetConstData()[0];
  return ((first & 0x01) == 0x00);
}

bool LLAddress::IsMulticast() const {
  // Is a Multicast if the least significant bit in the first byte is 1.
  if (!IsValid()) {
    return false;
  }
  return !IsUnicast();
}

bool LLAddress::IsBroadcast() const {
  // Is Broadcast is all bits are set.
  if (!IsValid()) {
    return false;
  }
  const uint8_t* data = GetConstData();
  return std::all_of(data, data + GetLength(),
                     [](uint8_t c) { return (0xff == c); });
}

bool LLAddress::IsUniversal() const {
  // Is Universal if the second least significant bit in the first byte
  // is 0.
  if (!IsValid()) {
    return false;
  }
  const uint8_t first = GetConstData()[0];
  return ((first & 0x02) == 0x00);
}

bool LLAddress::IsLocal() const {
  // Is Local if the second least significant bit in the first byte
  // is 1.
  if (!IsValid()) {
    return false;
  }
  return !IsUniversal();
}

std::string LLAddress::ToString() const {
  if (!IsValid()) {
    return "invalid";
  }
  const uint8_t* data = GetConstData();
  if (Type::kEui64 == type()) {
    return base::StringPrintf(
        "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx", data[0],
        data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
  }
  // Else must be EUI-48.
  CHECK_EQ(Type::kEui48, type());
  return base::StringPrintf("%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
                            data[0], data[1], data[2], data[3], data[4],
                            data[5]);
}

bool LLAddress::Equals(const LLAddress& other) const {
  // Check if they are the exact same object.
  if (this == &other) {
    return true;
  }
  // Any invalid address cannot be compared.
  if (!IsValid() || !other.IsValid()) {
    return false;
  }

  if (type_ != other.type_) {
    return false;
  }
  return address_.Equals(other.address_);
}

}  // namespace portier
