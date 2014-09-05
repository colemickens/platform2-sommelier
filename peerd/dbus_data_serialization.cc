// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/dbus_data_serialization.h"

#include <utility>
#include <vector>

#include "peerd/ip_addr.h"

namespace chromeos {
namespace dbus_utils {

// Extend D-Bus serialization mechanism to handle "peerd::ip_addr" structure.
std::string DBusSignature<peerd::ip_addr>::get() {
  // Returns "(ayq)".
  return GetDBusSignature<std::pair<std::vector<uint8_t>, uint16_t>>();
}

bool AppendValueToWriter(dbus::MessageWriter* writer,
                         const peerd::ip_addr& value) {
  std::vector<uint8_t> address_bytes;
  uint16_t port = 0;
  if (value.ss_family == AF_INET) {
    // IPv4.
    address_bytes.resize(sizeof(in_addr));
    auto ipv4 = reinterpret_cast<const sockaddr_in*>(&value);
    *reinterpret_cast<in_addr*>(address_bytes.data()) = ipv4->sin_addr;
    port = ipv4->sin_port;
  } else if (value.ss_family == AF_INET6) {
    // IPv6.
    address_bytes.resize(sizeof(in6_addr));
    auto ipv6 = reinterpret_cast<const sockaddr_in6*>(&value);
    *reinterpret_cast<in6_addr*>(address_bytes.data()) = ipv6->sin6_addr;
    port = ipv6->sin6_port;
  } else {
    LOG(ERROR) << "Address family " << value.ss_family << " not supported.";
    return false;
  }
  return AppendValueToWriter(writer,
                             std::make_pair(std::move(address_bytes), port));
}

bool PopValueFromReader(dbus::MessageReader* reader, peerd::ip_addr* value) {
  std::pair<std::vector<uint8_t>, uint16_t> addr;
  if (!PopValueFromReader(reader, &addr))
    return false;

  if (addr.first.size() == sizeof(in_addr)) {
    // IPv4.
    value->ss_family = AF_INET;
    auto ipv4 = reinterpret_cast<sockaddr_in*>(value);
    ipv4->sin_addr = *reinterpret_cast<const in_addr*>(addr.first.data());
    ipv4->sin_port = addr.second;
  } else if (addr.first.size() == sizeof(in6_addr)) {
    // IPv6.
    value->ss_family = AF_INET6;
    auto ipv6 = reinterpret_cast<sockaddr_in6*>(value);
    ipv6->sin6_addr = *reinterpret_cast<const in6_addr*>(addr.first.data());
    ipv6->sin6_port = addr.second;
  } else {
    LOG(ERROR) << "Unsupported IP address size.";
    return false;
  }
  return true;
}

}  // namespace dbus_utils
}  // namespace chromeos
