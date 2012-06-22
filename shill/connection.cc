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
#include "shill/scope_logger.h"

using base::Bind;
using base::Closure;
using base::Unretained;
using std::deque;
using std::string;

namespace shill {

// static
const uint32 Connection::kDefaultMetric = 1;
// static
const uint32 Connection::kNonDefaultMetricBase = 10;

Connection::Binder::Binder(const string &name,
                           const Closure &disconnect_callback)
    : name_(name),
      client_disconnect_callback_(disconnect_callback) {}

Connection::Binder::~Binder() {
  Attach(NULL);
}

void Connection::Binder::Attach(const ConnectionRefPtr &to_connection) {
  if (connection_) {
    connection_->DetachBinder(this);
    LOG(INFO) << name_ << ": unbound from connection: "
              << connection_->interface_name();
    connection_.reset();
  }
  if (to_connection) {
    connection_ = to_connection->weak_ptr_factory_.GetWeakPtr();
    connection_->AttachBinder(this);
    LOG(INFO) << name_ << ": bound to connection: "
              << connection_->interface_name();
  }
}

void Connection::Binder::OnDisconnect() {
  LOG(INFO) << name_ << ": bound connection disconnected: "
            << connection_->interface_name();
  connection_.reset();
  if (!client_disconnect_callback_.is_null()) {
    SLOG(Connection, 2) << "Running client disconnect callback.";
    client_disconnect_callback_.Run();
  }
}

Connection::Connection(int interface_index,
                       const std::string& interface_name,
                       Technology::Identifier technology,
                       const DeviceInfo *device_info)
    : weak_ptr_factory_(this),
      is_default_(false),
      routing_request_count_(0),
      interface_index_(interface_index),
      interface_name_(interface_name),
      technology_(technology),
      lower_binder_(
          interface_name_,
          // Connection owns a single instance of |lower_binder_| so it's safe
          // to use an Unretained callback.
          Bind(&Connection::OnLowerDisconnect, Unretained(this))),
      device_info_(device_info),
      resolver_(Resolver::GetInstance()),
      routing_table_(RoutingTable::GetInstance()),
      rtnl_handler_(RTNLHandler::GetInstance()) {
  SLOG(Connection, 2) << __func__ << "(" << interface_index << ", "
                      << interface_name << ", "
                      << Technology::NameFromIdentifier(technology) << ")";
}

Connection::~Connection() {
  SLOG(Connection, 2) << __func__ << " " << interface_name_;

  NotifyBindersOnDisconnect();

  DCHECK(!routing_request_count_);
  routing_table_->FlushRoutes(interface_index_);
  routing_table_->FlushRoutesWithTag(interface_index_);
  device_info_->FlushAddresses(interface_index_);
}

void Connection::UpdateFromIPConfig(const IPConfigRefPtr &config) {
  SLOG(Connection, 2) << __func__ << " " << interface_name_;

  const IPConfig::Properties &properties = config->properties();
  if (!properties.trusted_ip.empty() && !PinHostRoute(properties)) {
    LOG(ERROR) << "Unable to pin host route to " << properties.trusted_ip;
    return;
  }

  IPAddress local(properties.address_family);
  if (!local.SetAddressFromString(properties.address)) {
    LOG(ERROR) << "Local address " << properties.address << " is invalid";
    return;
  }
  local.set_prefix(properties.subnet_prefix);

  IPAddress broadcast(properties.address_family);
  if (properties.broadcast_address.empty()) {
    if (properties.peer_address.empty()) {
      LOG(WARNING) << "Broadcast address is not set.  Using default.";
      broadcast = local.GetDefaultBroadcast();
    }
  } else if (!broadcast.SetAddressFromString(properties.broadcast_address)) {
    LOG(ERROR) << "Broadcast address " << properties.broadcast_address
               << " is invalid";
    return;
  }

  IPAddress peer(properties.address_family);
  if (!properties.peer_address.empty() &&
      !peer.SetAddressFromString(properties.peer_address)) {
    LOG(ERROR) << "Peer address " << properties.peer_address
               << " is invalid";
    return;
  }

  IPAddress gateway(properties.address_family);
  if (!properties.gateway.empty() &&
      !gateway.SetAddressFromString(properties.gateway)) {
    LOG(ERROR) << "Gateway address " << properties.peer_address
               << " is invalid";
    return;
  }

  if (!FixGatewayReachability(&local, &peer, gateway)) {
    LOG(WARNING) << "Expect limited network connectivity.";
  }

  LOG(INFO) << __func__ << ": Installing with parameters:"
            << " local=" << local.ToString()
            << " broadcast=" << broadcast.ToString()
            << " peer=" << peer.ToString()
            << " gateway=" << gateway.ToString();
  rtnl_handler_->AddInterfaceAddress(interface_index_, local, broadcast, peer);

  if (gateway.IsValid()) {
    routing_table_->SetDefaultRoute(interface_index_, gateway,
                                    GetMetric(is_default_));
  }

  // Install any explicitly configured routes at the default metric.
  routing_table_->ConfigureRoutes(interface_index_, config, kDefaultMetric);

  // Save a copy of the last non-null DNS config
  if (!config->properties().dns_servers.empty()) {
    dns_servers_ = config->properties().dns_servers;
    dns_domain_search_ = config->properties().domain_search;
  }

  ipconfig_rpc_identifier_ = config->GetRpcIdentifier();

  if (is_default_) {
    resolver_->SetDNSFromIPConfig(config);
  }
}

void Connection::SetIsDefault(bool is_default) {
  SLOG(Connection, 2) << __func__ << " " << interface_name_
                      << " (index " << interface_index_ << ") "
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
  routing_table_->FlushCache();
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

bool Connection::RequestHostRoute(const IPAddress &address) {
  // Set the prefix to be the entire address size.
  IPAddress address_prefix(address);
  address_prefix.set_prefix(address_prefix.GetLength() * 8);

  // Do not set interface_index_ since this may not be the default route through
  // which this destination can be found.  However, we should tag the created
  // route with our interface index so we can clean this route up when this
  // connection closes.  Also, add route query callback to determine the lower
  // connection and bind to it.
  if (!routing_table_->RequestRouteToHost(
          address_prefix,
          -1,
          interface_index_,
          Bind(&Connection::OnRouteQueryResponse,
               weak_ptr_factory_.GetWeakPtr()))) {
    LOG(ERROR) << "Could not request route to " << address.ToString();
    return false;
  }

  return true;
}

// static
bool Connection::FixGatewayReachability(IPAddress *local,
                                        IPAddress *peer,
                                        const IPAddress &gateway) {
  if (!gateway.IsValid()) {
    LOG(WARNING) << "No gateway address was provided for this connection.";
    return false;
  }

  if (peer->IsValid()) {
    if (gateway.Equals(*peer)) {
      return true;
    }
    LOG(WARNING) << "Gateway address "
                 << gateway.ToString()
                 << " does not match peer address "
                 << peer->ToString();
    return false;
  }

  if (local->CanReachAddress(gateway)) {
    return true;
  }

  LOG(WARNING) << "Gateway "
               << gateway.ToString()
               << " is unreachable from local address/prefix "
               << local->ToString() << "/" << local->prefix();

  bool found_new_prefix = false;
  size_t original_prefix = local->prefix();
  // Only try to expand the netmask if the configured prefix is
  // less than "all ones".  This special-cases the "all-ones"
  // prefix as a forced conversion to point-to-point networking.
  if (local->prefix() < IPAddress::GetMaxPrefixLength(local->family())) {
    size_t prefix = original_prefix - 1;
    for (; prefix >= local->GetMinPrefixLength(); --prefix) {
      local->set_prefix(prefix);
      if (local->CanReachAddress(gateway)) {
        found_new_prefix = true;
        break;
      }
    }
  }

  if (!found_new_prefix) {
    // Restore the original prefix since we cannot find a better one.
    local->set_prefix(original_prefix);
    DCHECK(!peer->IsValid());
    LOG(WARNING) << "Assuming point-to-point configuration.";
    *peer = gateway;
    return true;
  }

  LOG(WARNING) << "Mitigating this by setting local prefix to "
               << local->prefix();
  return true;
}

uint32 Connection::GetMetric(bool is_default) {
  // If this is not the default route, assign a metric based on the interface
  // index.  This way all non-default routes (even to the same gateway IP) end
  // up with unique metrics so they do not collide.
  return is_default ? kDefaultMetric : kNonDefaultMetricBase + interface_index_;
}

bool Connection::PinHostRoute(const IPConfig::Properties &properties) {
  SLOG(Connection, 2) << __func__;
  if (properties.gateway.empty() || properties.trusted_ip.empty()) {
    return false;
  }

  IPAddress trusted_ip(properties.address_family);
  if (!trusted_ip.SetAddressFromString(properties.trusted_ip)) {
    LOG(ERROR) << "Failed to parse trusted_ip "
               << properties.trusted_ip << "; ignored.";
    return false;
  }

  return RequestHostRoute(trusted_ip);
}

void Connection::OnRouteQueryResponse(int interface_index,
                                      const RoutingTableEntry &entry) {
  SLOG(Connection, 2) << __func__ << "(" << interface_index << ", "
                      << entry.tag << ")" << " @ " << interface_name_;
  lower_binder_.Attach(NULL);
  DeviceRefPtr device = device_info_->GetDevice(interface_index);
  if (!device) {
    LOG(ERROR) << "Unable to lookup device for index " << interface_index;
    return;
  }
  ConnectionRefPtr connection = device->connection();
  if (!connection) {
    LOG(ERROR) << "Device " << interface_index << " has no connection.";
    return;
  }
  lower_binder_.Attach(connection);
}

void Connection::OnLowerDisconnect() {
  SLOG(Connection, 2) << __func__ << " @ " << interface_name_;
  // Ensures that |this| instance doesn't get destroyed in the middle of
  // notifying the binders. This method needs to be separate from
  // NotifyBindersOnDisconnect because the latter may be invoked by Connection's
  // destructor when |this| instance's reference count is already 0.
  ConnectionRefPtr connection(this);
  connection->NotifyBindersOnDisconnect();
}

void Connection::NotifyBindersOnDisconnect() {
  // Note that this method may be invoked by the destructor.
  SLOG(Connection, 2) << __func__ << " @ " << interface_name_;

  // Unbinds the lower connection before notifying the binders. This ensures
  // correct behavior in case of circular binding.
  lower_binder_.Attach(NULL);
  while (!binders_.empty()) {
    // Pop the binder first and then notify it to ensure that each binder is
    // notified only once.
    Binder *binder = binders_.front();
    binders_.pop_front();
    binder->OnDisconnect();
  }
}

void Connection::AttachBinder(Binder *binder) {
  SLOG(Connection, 2) << __func__ << "(" << binder->name() << ")" << " @ "
                      << interface_name_;
  binders_.push_back(binder);
}

void Connection::DetachBinder(Binder *binder) {
  SLOG(Connection, 2) << __func__ << "(" << binder->name() << ")" << " @ "
                      << interface_name_;
  for (deque<Binder *>::iterator it = binders_.begin();
       it != binders_.end(); ++it) {
    if (binder == *it) {
      binders_.erase(it);
      return;
    }
  }
}

}  // namespace shill
