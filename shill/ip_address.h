// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
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
  static const Family kFamilyUnknown;
  static const Family kFamilyIPv4;
  static const Family kFamilyIPv6;

  explicit IPAddress(Family family);
  IPAddress(Family family, const ByteString &address);
  IPAddress(Family family, const ByteString &address, unsigned int prefix);
  ~IPAddress();

  // Since this is a copyable datatype...
  IPAddress(const IPAddress &b)
    : family_(b.family_),
      address_(b.address_),
      prefix_(b.prefix_) {}
  IPAddress &operator=(const IPAddress &b) {
    family_ = b.family_;
    address_ = b.address_;
    prefix_ = b.prefix_;
    return *this;
  }

  // Static utilities
  // Get the length in bytes of addresses of the given family
  static size_t GetAddressLength(Family family);

  // Returns the prefix length given an address |family| and a |mask|. For
  // example, returns 24 for an IPv4 mask 255.255.255.0.
  static size_t GetPrefixLengthFromMask(Family family, const std::string &mask);

  // Getters and Setters
  Family family() const { return family_; }
  const ByteString &address() const { return address_; }
  unsigned int prefix() const { return prefix_; }
  void set_prefix(unsigned int prefix) { prefix_ = prefix; }
  const unsigned char *GetConstData() const { return address_.GetConstData(); }
  size_t GetLength() const { return address_.GetLength(); }
  bool IsDefault() const { return address_.IsZero(); }
  bool IsValid() const {
    return family_ != kFamilyUnknown &&
        GetLength() == GetAddressLength(family_);
  }

  // Parse an IP address string
  bool SetAddressFromString(const std::string &address_string);
  // An uninitialized IPAddress is empty and invalid when constructed.
  // Use SetAddressToDefault() to set it to the default or "all-zeroes" address.
  void SetAddressToDefault();
  // Return the string equivalent of the address.  Returns true if the
  // conversion succeeds in which case |address_string| is set to the
  // result.  Otherwise the function returns false and |address_string|
  // is left unmodified.
  bool IntoString(std::string *address_string) const;
  // Similar to IntoString, but returns by value. Convenient for logging.
  std::string ToString() const;

  bool Equals(const IPAddress &b) const {
    return family_ == b.family_ && address_.Equals(b.address_) &&
        prefix_ == b.prefix_;
  }

 private:
  Family family_;
  ByteString address_;
  unsigned int prefix_;
  // NO DISALLOW_COPY_AND_ASSIGN -- we assign IPAddresses in STL datatypes
};

}  // namespace shill

#endif  // SHILL_IP_ADDRESS_
