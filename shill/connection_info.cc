// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/connection_info.h"

#include <netinet/in.h>

namespace shill {

ConnectionInfo::ConnectionInfo()
    : protocol(IPPROTO_MAX),
      time_to_expire_seconds(0),
      is_unreplied(false),
      original_source_ip_address(IPAddress::kFamilyUnknown),
      original_source_port(0),
      original_destination_ip_address(IPAddress::kFamilyUnknown),
      original_destination_port(0),
      reply_source_ip_address(IPAddress::kFamilyUnknown),
      reply_source_port(0),
      reply_destination_ip_address(IPAddress::kFamilyUnknown),
      reply_destination_port(0) {}

ConnectionInfo::ConnectionInfo(int protocol,
                               int64_t time_to_expire_seconds,
                               bool is_unreplied,
                               IPAddress original_source_ip_address,
                               uint16_t original_source_port,
                               IPAddress original_destination_ip_address,
                               uint16_t original_destination_port,
                               IPAddress reply_source_ip_address,
                               uint16_t reply_source_port,
                               IPAddress reply_destination_ip_address,
                               uint16_t reply_destination_port)
    : protocol(protocol),
      time_to_expire_seconds(time_to_expire_seconds),
      is_unreplied(is_unreplied),
      original_source_ip_address(original_source_ip_address),
      original_source_port(original_source_port),
      original_destination_ip_address(original_destination_ip_address),
      original_destination_port(original_destination_port),
      reply_source_ip_address(reply_source_ip_address),
      reply_source_port(reply_source_port),
      reply_destination_ip_address(reply_destination_ip_address),
      reply_destination_port(reply_destination_port) {}

ConnectionInfo::ConnectionInfo(const ConnectionInfo& info) = default;

ConnectionInfo::~ConnectionInfo() = default;

ConnectionInfo& ConnectionInfo::operator=(const ConnectionInfo& info) = default;

}  // namespace shill
