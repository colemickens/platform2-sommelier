// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/connection.h"

#include <arpa/inet.h>
#include <linux/rtnetlink.h>

#include "shill/device_info.h"
#include "shill/resolver.h"
#include "shill/routing_table.h"
#include "shill/rtnl_handler.h"

using std::string;

namespace shill {

// static
const uint32 Connection::kDefaultMetric = 1;
// static
const uint32 Connection::kNonDefaultMetricBase = 10;

Connection::Connection(int interface_index,
                       const std::string& interface_name,
                       const DeviceInfo *device_info)
    : is_default_(false),
      routing_request_count_(0),
      interface_index_(interface_index),
      interface_name_(interface_name),
      device_info_(device_info),
      resolver_(Resolver::GetInstance()),
      routing_table_(RoutingTable::GetInstance()),
      rtnl_handler_(RTNLHandler::GetInstance()) {
  VLOG(2) << __func__;
}

Connection::~Connection() {
  VLOG(2) << __func__ << " " << interface_name_;

  DCHECK(!routing_request_count_);
  routing_table_->FlushRoutes(interface_index_, true);
  device_info_->FlushAddresses(interface_index_);
}

void Connection::UpdateFromIPConfig(const IPConfigRefPtr &config) {
  VLOG(2) << __func__ << " " << interface_name_;

  const IPConfig::Properties &properties = config->properties();
  IPAddress local(properties.address_family);
  if (!local.SetAddressFromString(properties.address)) {
    LOG(ERROR) << "Local address " << properties.address << " is invalid";
    return;
  }
  local.set_prefix(properties.subnet_cidr);

  IPAddress broadcast(properties.address_family);
  if (!broadcast.SetAddressFromString(properties.broadcast_address)) {
    LOG(ERROR) << "Broadcast address " << properties.broadcast_address
               << " is invalid";
    return;
  }

  rtnl_handler_->AddInterfaceAddress(interface_index_, local, broadcast);

  routing_table_->SetDefaultRoute(interface_index_, config,
                                  GetMetric(is_default_));

  // Save a copy of the last non-null DNS config
  if (!config->properties().dns_servers.empty()) {
    dns_servers_ = config->properties().dns_servers;
    dns_domain_search_ = config->properties().domain_search;
  }

  if (is_default_) {
    resolver_->SetDNSFromIPConfig(config);
  }
}

void Connection::SetIsDefault(bool is_default) {
  VLOG(2) << __func__ << " "
          << interface_name_ << " (index " << interface_index_ << ") "
          << is_default_ << " -> " << is_default;
  if (is_default == is_default_) {
    return;
  }

  routing_table_->SetDefaultMetric(interface_index_, GetMetric(is_default));

  is_default_ = is_default;

  if (is_default) {
    resolver_->SetDNSFromLists(dns_servers_, dns_domain_search_);
    DeviceRefPtr device = device_info_->GetDevice(interface_index_);
    if (device) {
      device->RequestPortalDetection();
    }
  }
}

void Connection::RequestRouting() {
  if (routing_request_count_++ == 0) {
    DeviceRefPtr device = device_info_->GetDevice(interface_index_);
    DCHECK(device.get());
    if (!device.get()) {
      LOG(ERROR) << "Device is NULL!";
      return;
    }
    device->DisableReversePathFilter();
  }
}

void Connection::ReleaseRouting() {
  DCHECK(routing_request_count_ > 0);
  if (--routing_request_count_ == 0) {
    DeviceRefPtr device = device_info_->GetDevice(interface_index_);
    DCHECK(device.get());
    if (!device.get()) {
      LOG(ERROR) << "Device is NULL!";
      return;
    }
    device->EnableReversePathFilter();

    // Clear any cached routes that might have accumulated while reverse-path
    // filtering was disabled.
    routing_table_->FlushCache();
  }
}

uint32 Connection::GetMetric(bool is_default) {
  // If this is not the default route, assign a metric based on the interface
  // index.  This way all non-default routes (even to the same gateway IP) end
  // up with unique metrics so they do not collide.
  return is_default ? kDefaultMetric : kNonDefaultMetricBase + interface_index_;
}

}  // namespace shill
