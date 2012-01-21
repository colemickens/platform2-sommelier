// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/ip_address.h"

#include <arpa/inet.h>
#include <netinet/in.h>

#include <string>

#include "shill/byte_string.h"

using std::string;

namespace shill {

// static
const IPAddress::Family IPAddress::kFamilyUnknown = AF_UNSPEC;
// static
const IPAddress::Family IPAddress::kFamilyIPv4 = AF_INET;
// static
const IPAddress::Family IPAddress::kFamilyIPv6 = AF_INET6;

IPAddress::IPAddress(Family family, const ByteString &address)
    : family_(family) ,
      address_(address),
      prefix_(0) {}

IPAddress::IPAddress(Family family,
                     const ByteString &address,
                     unsigned int prefix)
    : family_(family) ,
      address_(address),
      prefix_(prefix) {}

IPAddress::IPAddress(Family family)
    : family_(family),
      prefix_(0) {}

IPAddress::~IPAddress() {}

int IPAddress::GetAddressLength(Family family) {
  switch (family) {
  case kFamilyIPv4:
    return sizeof(in_addr);
  case kFamilyIPv6:
    return sizeof(in6_addr);
  default:
    return 0;
  }
}

bool IPAddress::SetAddressFromString(const string &address_string) {
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

bool IPAddress::ToString(string *address_string) const {
  // Noting that INET6_ADDRSTRLEN > INET_ADDRSTRLEN
  char address_buf[INET6_ADDRSTRLEN];
  if (GetLength() != GetAddressLength(family_) ||
      !inet_ntop(family_, GetConstData(), address_buf, sizeof(address_buf))) {
    return false;
  }
  *address_string = address_buf;
  return true;
}

}  // namespace shill
