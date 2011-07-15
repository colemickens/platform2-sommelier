// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/ip_address.h"

#include <netinet/in.h>

#include <string>

#include "shill/byte_string.h"

namespace shill {

IPAddress::IPAddress(Family family, const ByteString &address)
    : family_(family) ,
      address_(address) {}

IPAddress::IPAddress(Family family)
    : family_(family) {}

IPAddress::~IPAddress() {}

int IPAddress::GetAddressLength(Family family) {
  switch (family) {
  case kAddressFamilyIPv4:
    return sizeof(in_addr);
  case kAddressFamilyIPv6:
    return sizeof(in6_addr);
  default:
    return 0;
  }
}

bool IPAddress::SetAddressFromString(const std::string &address_string) {
  int address_length = GetAddressLength(family_);

  if (!address_length) {
    return false;
  }

  ByteString address(address_length);
  if (inet_pton(family_, address_string.c_str(), address.GetData()) <= 0) {
    return false;
  }
  address_ = address;
  return true;
}

void IPAddress::SetAddressToDefault() {
  address_ = ByteString(GetAddressLength(family_));
}

}  // namespace shill
