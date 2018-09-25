// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/connection_info.h"

#include <netinet/in.h>

namespace shill {

ConnectionInfo::ConnectionInfo()
    : protocol_(IPPROTO_MAX),
      time_to_expire_seconds_(0),
      is_unreplied_(false),
      original_source_ip_address_(IPAddress::kFamilyUnknown),
      original_source_port_(0),
      original_destination_ip_address_(IPAddress::kFamilyUnknown),
      original_destination_port_(0),
      reply_source_ip_address_(IPAddress::kFamilyUnknown),
      reply_source_port_(0),
      reply_destination_ip_address_(IPAddress::kFamilyUnknown),
      reply_destination_port_(0) {
}

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
    : protocol_(protocol),
      time_to_expire_seconds_(time_to_expire_seconds),
      is_unreplied_(is_unreplied),
      original_source_ip_address_(original_source_ip_address),
      original_source_port_(original_source_port),
      original_destination_ip_address_(original_destination_ip_address),
      original_destination_port_(original_destination_port),
      reply_source_ip_address_(reply_source_ip_address),
      reply_source_port_(reply_source_port),
      reply_destination_ip_address_(reply_destination_ip_address),
      reply_destination_port_(reply_destination_port) {
}

ConnectionInfo::ConnectionInfo(const ConnectionInfo& info)
    : protocol_(info.protocol_),
      time_to_expire_seconds_(info.time_to_expire_seconds_),
      is_unreplied_(info.is_unreplied_),
      original_source_ip_address_(info.original_source_ip_address_),
      original_source_port_(info.original_source_port_),
      original_destination_ip_address_(
          info.original_destination_ip_address_),
      original_destination_port_(info.original_destination_port_),
      reply_source_ip_address_(info.reply_source_ip_address_),
      reply_source_port_(info.reply_source_port_),
      reply_destination_ip_address_(
          info.reply_destination_ip_address_),
      reply_destination_port_(info.reply_destination_port_) {
}

ConnectionInfo::~ConnectionInfo() {}

ConnectionInfo& ConnectionInfo::operator=(const ConnectionInfo& info) {
  protocol_ = info.protocol_;
  time_to_expire_seconds_ = info.time_to_expire_seconds_;
  is_unreplied_ = info.is_unreplied_;
  original_source_ip_address_ = info.original_source_ip_address_;
  original_source_port_ = info.original_source_port_;
  original_destination_ip_address_ =
      info.original_destination_ip_address_;
  original_destination_port_ = info.original_destination_port_;
  reply_source_ip_address_ = info.reply_source_ip_address_;
  reply_source_port_ = info.reply_source_port_;
  reply_destination_ip_address_ = info.reply_destination_ip_address_;
  reply_destination_port_ = info.reply_destination_port_;

  return *this;
}

}  // namespace shill
