// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/ip_address.h"

#include <arpa/inet.h>
#include <netinet/in.h>

#include <string>

#include <base/logging.h>

#include "shill/byte_string.h"

using std::string;

namespace shill {

// static
const IPAddress::Family IPAddress::kFamilyUnknown = AF_UNSPEC;
// static
const IPAddress::Family IPAddress::kFamilyIPv4 = AF_INET;
// static
const IPAddress::Family IPAddress::kFamilyIPv6 = AF_INET6;

// static
const char IPAddress::kFamilyNameUnknown[] = "Unknown";
// static
const char IPAddress::kFamilyNameIPv4[] = "IPv4";
// static
const char IPAddress::kFamilyNameIPv6[] = "IPv6";

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

// static
size_t IPAddress::GetAddressLength(Family family) {
  switch (family) {
  case kFamilyIPv4:
    return sizeof(in_addr);
  case kFamilyIPv6:
    return sizeof(in6_addr);
  default:
    return 0;
  }
}

// static
size_t IPAddress::GetPrefixLengthFromMask(Family family, const string &mask) {
  switch (family) {
    case kFamilyIPv4: {
      in_addr_t mask_val = inet_network(mask.c_str());
      int subnet_cidr = 0;
      while (mask_val) {
        subnet_cidr++;
        mask_val <<= 1;
      }
      return subnet_cidr;
    }
    case kFamilyIPv6:
      NOTIMPLEMENTED();
      break;
    default:
      LOG(WARNING) << "Unexpected address family: " << family;
      break;
  }
  return 0;
}

// static
string IPAddress::GetAddressFamilyName(Family family) {
  switch (family) {
  case kFamilyIPv4:
    return kFamilyNameIPv4;
  case kFamilyIPv6:
    return kFamilyNameIPv6;
  default:
    return kFamilyNameUnknown;
  }
}

bool IPAddress::SetAddressFromString(const string &address_string) {
  size_t address_length = GetAddressLength(family_);

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

bool IPAddress::IntoString(string *address_string) const {
  // Noting that INET6_ADDRSTRLEN > INET_ADDRSTRLEN
  char address_buf[INET6_ADDRSTRLEN];
  if (GetLength() != GetAddressLength(family_) ||
      !inet_ntop(family_, GetConstData(), address_buf, sizeof(address_buf))) {
    return false;
  }
  *address_string = address_buf;
  return true;
}

string IPAddress::ToString() const {
  string out("<unknown>");
  IntoString(&out);
  return out;
}

}  // namespace shill
