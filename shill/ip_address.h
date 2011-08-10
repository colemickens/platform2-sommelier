// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_IP_ADDRESS_
#define SHILL_IP_ADDRESS_

#include <string>

#include "shill/byte_string.h"

namespace shill {

class IPAddress {
 public:
  typedef unsigned char Family;
  static const Family kAddressFamilyUnknown;
  static const Family kAddressFamilyIPv4;
  static const Family kAddressFamilyIPv6;

  explicit IPAddress(Family family);
  IPAddress(Family family, const ByteString &address);
  ~IPAddress();

  // Static utilities
  // Get the length in bytes of addresses of the given family
  static int GetAddressLength(Family family);

  // Getters and Setters
  Family family() const { return family_; }
  const ByteString &address() const { return address_; }
  const unsigned char *GetConstData() const { return address_.GetConstData(); }
  int GetLength() const { return address_.GetLength(); }
  bool IsDefault() const { return address_.IsZero(); }
  bool IsValid() const {
    return family_ != kAddressFamilyUnknown &&
        GetLength() == GetAddressLength(family_);
  }

  // Parse an IP address string
  bool SetAddressFromString(const std::string &address_string);
  // An uninitialized IPAddress is empty and invalid when constructed.
  // Use SetAddressToDefault() to set it to the default or "all-zeroes" address.
  void SetAddressToDefault();

  bool Equals(const IPAddress &b) const {
    return family_ == b.family_ && address_.Equals(b.address_);
  }

  void Clone(const IPAddress &b) {
    family_ = b.family_;
    address_ = b.address_;
  }

 private:
  Family family_;
  ByteString address_;
  DISALLOW_COPY_AND_ASSIGN(IPAddress);
};

}  // namespace shill

#endif  // SHILL_IP_ADDRESS_
