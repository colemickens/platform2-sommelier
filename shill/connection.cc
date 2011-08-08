// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/connection.h"

#include "shill/resolver.h"
#include "shill/routing_table.h"
#include "shill/rtnl_handler.h"

using std::string;

namespace shill {

// static
const uint32 Connection::kDefaultMetric = 1;
// static
const uint32 Connection::kNonDefaultMetric = 10;

Connection::Connection(int interface_index, const std::string& interface_name)
    : is_default_(false),
      interface_index_(interface_index),
      interface_name_(interface_name),
      resolver_(Resolver::GetInstance()),
      routing_table_(RoutingTable::GetInstance()),
      rtnl_handler_(RTNLHandler::GetInstance()) {
  VLOG(2) << __func__;
}

Connection::~Connection() {
  routing_table_->FlushRoutes(interface_index_);
  // TODO(pstew): Also flush all addresses when DeviceInfo starts tracking them
  VLOG(2) << __func__;
}

void Connection::UpdateFromIPConfig(const IPConfigRefPtr &config) {
  VLOG(2) << __func__;
  rtnl_handler_->AddInterfaceAddress(interface_index_, *config);

  uint32 metric = is_default_ ? kDefaultMetric : kNonDefaultMetric;
  routing_table_->SetDefaultRoute(interface_index_, config, metric);

  // Save a copy of the last non-null DNS config
  if (!config->properties().dns_servers.empty()) {
    dns_servers_ = config->properties().dns_servers;
    dns_domain_search_ = config->properties().domain_search;
  }

  if (is_default_) {
    resolver_->SetDNSFromIPConfig(config);
  }
}

void Connection::SetDefault(bool is_default) {
  VLOG(2) << __func__;
  if (is_default == is_default_) {
    return;
  }

  routing_table_->SetDefaultMetric(interface_index_,
      is_default ? kDefaultMetric : kNonDefaultMetric);

  if (is_default) {
    resolver_->SetDNSFromLists(dns_servers_, dns_domain_search_);
  }

  is_default_ = is_default;
}

}  // namespace shill
