// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ctype.h>
#include <stdio.h>

#include <net/if_arp.h>

#include <algorithm>

#include <base/logging.h>
#include <base/strings/stringprintf.h>

#include "portier/ll_address.h"

namespace portier {

using ::shill::ByteString;
using ::std::string;

using Type = LLAddress::Type;

// Static methods for Type.

const Type LLAddress::kTypeInvalid = LLAddress::Type::Invalid;
const Type LLAddress::kTypeEui48 = LLAddress::Type::Eui48;
const Type LLAddress::kTypeEui64 = LLAddress::Type::Eui64;

namespace {

bool ParseStringLinkLayerAddress(LLAddress::Type type,
                                 const string& address_string,
                                 unsigned char* out) {
  // Covert to lower case.
  string lower_address(address_string);
  std::transform(lower_address.begin(), lower_address.end(),
                 lower_address.begin(), tolower);

  // Two expected string forms for the LL address, XX:XX or XX-XX.
  switch (type) {
    case LLAddress::kTypeEui48:
      if (sscanf(lower_address.c_str(),
                 "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx", &out[0], &out[1],
                 &out[2], &out[3], &out[4], &out[5]) == 6) {
        // Parse successful.
        return true;
      } else if (sscanf(lower_address.c_str(),
                        "%02hhx-%02hhx-%02hhx-%02hhx-%02hhx-%02hhx", &out[0],
                        &out[1], &out[2], &out[3], &out[4], &out[5]) == 6) {
        return true;
      }
      break;
    case LLAddress::kTypeEui64:
      if (sscanf(lower_address.c_str(),
                 "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
                 &out[0], &out[1], &out[2], &out[3], &out[4], &out[5], &out[6],
                 &out[7]) == 8) {
        // Parse successful.
        return true;
      } else if (sscanf(
                     lower_address.c_str(),
                     "%02hhx-%02hhx-%02hhx-%02hhx-%02hhx-%02hhx-%02hhx-%02hhx",
                     &out[0], &out[1], &out[2], &out[3], &out[4], &out[5],
                     &out[6], &out[7]) == 8) {
        return true;
      }
      break;
    default:
      break;
  }
  return false;
}

}  // namespace

string LLAddress::GetTypeName(LLAddress::Type type) {
  switch (type) {
    case kTypeEui48:
      return "EUI-48";
    case kTypeEui64:
      return "EUI-64";
    default:
      return "unknown";
  }
}

int32_t LLAddress::GetTypeLength(LLAddress::Type type) {
  switch (type) {
    case kTypeEui48:
      return 6;
    case kTypeEui64:
      return 8;
    default:
      return -1;
  }
}

uint16_t LLAddress::GetTypeArpType(Type type) {
  switch (type) {
    case kTypeEui48:
      return ARPHRD_ETHER;
    case kTypeEui64:
      return ARPHRD_EUI64;
    default:
      return ARPHRD_VOID;
  }
}

// Constructors.

LLAddress::LLAddress() : type_(kTypeInvalid), address_(0) {}

LLAddress::LLAddress(LLAddress::Type type) : type_(type), address_(0) {
  const int32_t len = GetTypeLength(type);
  if (len <= 0) {
    // |type| is invalid.
    type_ = kTypeInvalid;
  } else {
    address_.Resize(len);
  }
}

LLAddress::LLAddress(LLAddress::Type type, const ByteString& address)
    : type_(type), address_(address) {
  // Check if invalid.
  const int32_t len = GetTypeLength(type);
  if (len <= 0 || address.GetLength() != len) {
    type_ = kTypeInvalid;
    address_.Clear();
  }
}

LLAddress::LLAddress(LLAddress::Type type, const string& address)
    : type_(type), address_(0) {
  const int32_t len = GetTypeLength(type);
  if (len <= 0) {
    type_ = kTypeInvalid;
    return;
  }

  address_.Resize(len);
  if (!ParseStringLinkLayerAddress(type, address, address_.GetData())) {
    type_ = kTypeInvalid;
    address_.Clear();
  }
}

LLAddress::LLAddress(const struct sockaddr_ll* address_struct) {
  if (NULL == address_struct) {
    type_ = kTypeInvalid;
    return;
  }

  if (GetTypeArpType(kTypeEui48) == address_struct->sll_hatype &&
      6 == address_struct->sll_halen) {
    type_ = kTypeEui48;
    address_ = ByteString(address_struct->sll_addr, 6);
  } else if (GetTypeArpType(kTypeEui64) == address_struct->sll_hatype &&
             8 == address_struct->sll_halen) {
    type_ = kTypeEui64;
    address_ = ByteString(address_struct->sll_addr, 8);
  } else {
    // Assume invalid.
    type_ = kTypeInvalid;
  }
}

LLAddress::~LLAddress() {}

// Copy constructor.

LLAddress::LLAddress(const LLAddress& other)
    : type_(other.type_), address_(other.address_) {}

LLAddress& LLAddress::operator=(const LLAddress& other) {
  type_ = other.type_;
  address_ = other.address_;
  return *this;
}

// Getters.

LLAddress::Type LLAddress::type() const {
  return type_;
}

uint16_t LLAddress::arpType() const {
  return GetTypeArpType(type());
}

const ByteString& LLAddress::address() const {
  return address_;
}

const unsigned char* LLAddress::GetConstData() const {
  return address_.GetConstData();
}

uint32_t LLAddress::GetLength() const {
  return address_.GetLength();
}

// Address information

bool LLAddress::IsValid() const {
  return kTypeInvalid != type_;
}

bool LLAddress::IsUnicast() const {
  // Is a Unicast if the least significant bit in the first byte is 0.
  if (!IsValid()) {
    return false;
  }
  const unsigned char first = GetConstData()[0];
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
  const unsigned char* data = GetConstData();
  return std::all_of(data, data + GetLength(),
                     [](unsigned char c) { return (0xff == c); });
}

bool LLAddress::IsUniversal() const {
  // Is Universal if the second least significant bit in the first byte
  // is 0.
  if (!IsValid()) {
    return false;
  }
  const unsigned char first = GetConstData()[0];
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
  const unsigned char* data = GetConstData();
  if (kTypeEui64 == type()) {
    return base::StringPrintf(
        "%02hhx-%02hhx-%02hhx-%02hhx-%02hhx-%02hhx-%02hhx-%02hhx", data[0],
        data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
  }
  // Else must be EUI-48.
  CHECK(kTypeEui48 == type());
  return base::StringPrintf("%02hhx-%02hhx-%02hhx-%02hhx-%02hhx-%02hhx",
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
