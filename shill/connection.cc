// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/connection.h"

#include <arpa/inet.h>
#include <linux/rtnetlink.h>

#include <limits>
#include <utility>

#include <base/bind.h>
#include <base/stl_util.h>
#include <base/strings/stringprintf.h>

#include "shill/control_interface.h"
#include "shill/device_info.h"
#include "shill/logging.h"
#include "shill/net/rtnl_handler.h"
#include "shill/resolver.h"
#include "shill/routing_table.h"

using std::vector;
using std::string;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kConnection;
static string ObjectID(Connection* c) {
  if (c == nullptr)
    return "(connection)";
  return c->interface_name();
}
}  // namespace Logging

// static
const uint32_t Connection::kDefaultMetric = 10;
// static
//
// UINT_MAX is also a valid priority metric, but we reserve this as a sentinel
// value, as in RoutingTable::GetDefaultRouteInternal.
const uint32_t Connection::kLowestPriorityMetric =
  std::numeric_limits<uint32_t>::max() - 1;
// static
const uint32_t Connection::kMetricIncrement = 10;

Connection::Binder::Binder(const string& name,
                           const base::Closure& disconnect_callback)
    : name_(name),
      client_disconnect_callback_(disconnect_callback) {}

Connection::Binder::~Binder() {
  Attach(nullptr);
}

void Connection::Binder::Attach(const ConnectionRefPtr& to_connection) {
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
    SLOG(connection_.get(), 2) << "Running client disconnect callback.";
    client_disconnect_callback_.Run();
  }
}

Connection::Connection(int interface_index,
                       const std::string& interface_name,
                       bool fixed_ip_params,
                       Technology::Identifier technology,
                       const DeviceInfo* device_info,
                       ControlInterface* control_interface)
    : weak_ptr_factory_(this),
      use_dns_(false),
      metric_(kLowestPriorityMetric),
      is_primary_physical_(false),
      has_broadcast_domain_(false),
      routing_request_count_(0),
      interface_index_(interface_index),
      interface_name_(interface_name),
      technology_(technology),
      use_if_addrs_(false),
      blackholed_addrs_(nullptr),
      fixed_ip_params_(fixed_ip_params),
      table_id_(RT_TABLE_MAIN),
      blackhole_table_id_(RT_TABLE_UNSPEC),
      local_(IPAddress::kFamilyUnknown),
      gateway_(IPAddress::kFamilyUnknown),
      device_info_(device_info),
      resolver_(Resolver::GetInstance()),
      routing_table_(RoutingTable::GetInstance()),
      rtnl_handler_(RTNLHandler::GetInstance()),
      control_interface_(control_interface) {
  SLOG(this, 2) << __func__ << "(" << interface_index << ", "
                << interface_name << ", "
                << Technology::NameFromIdentifier(technology) << ")";
}

Connection::~Connection() {
  SLOG(this, 2) << __func__ << " " << interface_name_;

  NotifyBindersOnDisconnect();

  DCHECK(!routing_request_count_);
  routing_table_->FlushRoutes(interface_index_);
  routing_table_->FlushRoutesWithTag(interface_index_);
  if (!fixed_ip_params_) {
    device_info_->FlushAddresses(interface_index_);
  }
  routing_table_->FlushRules(interface_index_);
  routing_table_->FreeTableId(table_id_);
  if (blackhole_table_id_ != RT_TABLE_UNSPEC) {
    routing_table_->FreeTableId(blackhole_table_id_);
  }
}

bool Connection::SetupExcludedRoutes(const IPConfig::Properties& properties,
                                     const IPAddress& gateway) {
  excluded_ips_cidr_ = properties.exclusion_list;

  // If this connection has its own dedicated routing table, exclusion
  // is as simple as adding an RTN_THROW entry for each item on the list.
  // Traffic that matches the RTN_THROW entry will cause the kernel to
  // stop traversing our routing table and try the next rule in the list.
  IPAddress empty_ip(properties.address_family);
  RoutingTableEntry entry(empty_ip,
                          empty_ip,
                          empty_ip,
                          0,
                          RT_SCOPE_LINK,
                          false,
                          table_id_,
                          RTN_THROW,
                          RoutingTableEntry::kDefaultTag);
  for (const auto& excluded_ip : excluded_ips_cidr_) {
    if (!entry.dst.SetAddressAndPrefixFromString(excluded_ip) ||
        !entry.dst.IsValid() ||
        !routing_table_->AddRoute(interface_index_, entry)) {
      LOG(ERROR) << "Unable to setup route for " << excluded_ip << ".";
      return false;
    }
  }
  return true;
}

void Connection::UpdateFromIPConfig(const IPConfigRefPtr& config) {
  SLOG(this, 2) << __func__ << " " << interface_name_;

  const IPConfig::Properties& properties = config->properties();
  allowed_uids_ = properties.allowed_uids;
  allowed_iifs_ = properties.allowed_iifs;
  use_if_addrs_ = properties.use_if_addrs ||
      Technology::IsPrimaryConnectivityTechnology(technology_);

  if (table_id_ == RT_TABLE_MAIN) {
    table_id_ = routing_table_->AllocTableId();
    CHECK_NE(table_id_, RT_TABLE_UNSPEC);
    routing_table_->SetPerDeviceTable(interface_index_, table_id_);
  }

  IPAddress gateway(properties.address_family);
  if (!properties.gateway.empty() &&
      !gateway.SetAddressFromString(properties.gateway)) {
    LOG(ERROR) << "Gateway address " << properties.gateway << " is invalid";
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
    if (local.family() == IPAddress::kFamilyIPv4 &&
        properties.peer_address.empty()) {
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

  if (!SetupExcludedRoutes(properties, gateway)) {
    return;
  }

  if (!FixGatewayReachability(local, &peer, &gateway)) {
    LOG(WARNING) << "Expect limited network connectivity.";
  }

  if (!fixed_ip_params_) {
    if (device_info_->HasOtherAddress(interface_index_, local)) {
      // The address has changed for this interface.  We need to flush
      // everything and start over.
      LOG(INFO) << __func__ << ": Flushing old addresses and routes.";
      routing_table_->FlushRoutes(interface_index_);
      device_info_->FlushAddresses(interface_index_);
    }

    LOG(INFO) << __func__ << ": Installing with parameters:"
              << " local=" << local.ToString()
              << " broadcast=" << broadcast.ToString()
              << " peer=" << peer.ToString()
              << " gateway=" << gateway.ToString();

    rtnl_handler_->AddInterfaceAddress(
        interface_index_, local, broadcast, peer);
    SetMTU(properties.mtu);
  }

  if (gateway.IsValid() && properties.default_route) {
    routing_table_->SetDefaultRoute(interface_index_, gateway,
                                    metric_,
                                    table_id_);
  }

  if (blackhole_table_id_ != RT_TABLE_UNSPEC) {
    routing_table_->FreeTableId(blackhole_table_id_);
    blackhole_table_id_ = RT_TABLE_UNSPEC;
  }

  blackholed_uids_ = properties.blackholed_uids;
  blackholed_addrs_ = properties.blackholed_addrs;
  bool has_blackholed_addrs =
      blackholed_addrs_ && !blackholed_addrs_->IsEmpty();

  if (!blackholed_uids_.empty() || has_blackholed_addrs) {
    blackhole_table_id_ = routing_table_->AllocTableId();
    CHECK(blackhole_table_id_);
    routing_table_->CreateBlackholeRoute(interface_index_,
                                         IPAddress::kFamilyIPv4,
                                         0,
                                         blackhole_table_id_);
    routing_table_->CreateBlackholeRoute(interface_index_,
                                         IPAddress::kFamilyIPv6,
                                         0,
                                         blackhole_table_id_);
  }

  UpdateRoutingPolicy();

  // Install any explicitly configured routes at the default metric.
  routing_table_->ConfigureRoutes(interface_index_, config, kDefaultMetric,
                                  table_id_);

  if (properties.blackhole_ipv6) {
    routing_table_->CreateBlackholeRoute(interface_index_,
                                         IPAddress::kFamilyIPv6,
                                         0,
                                         table_id_);
  }

  // Save a copy of the last non-null DNS config.
  if (!config->properties().dns_servers.empty()) {
    dns_servers_ = config->properties().dns_servers;
  }

  if (!config->properties().domain_search.empty()) {
    dns_domain_search_ = config->properties().domain_search;
  }

  if (!config->properties().domain_name.empty()) {
    dns_domain_name_ = config->properties().domain_name;
  }

  ipconfig_rpc_identifier_ = config->GetRpcIdentifier();

  PushDNSConfig();

  local_ = local;
  gateway_ = gateway;
  has_broadcast_domain_ = !peer.IsValid();
}

void Connection::UpdateGatewayMetric(const IPConfigRefPtr& config) {
  const IPConfig::Properties& properties = config->properties();
  IPAddress gateway(properties.address_family);

  if (!properties.gateway.empty() &&
      !gateway.SetAddressFromString(properties.gateway)) {
    return;
  }
  if (gateway.IsValid() && properties.default_route) {
    routing_table_->SetDefaultRoute(interface_index_, gateway,
                                    metric_,
                                    table_id_);
    routing_table_->FlushCache();
  }
}

void Connection::UpdateRoutingPolicy() {
  routing_table_->FlushRules(interface_index_);

  uint32_t blackhole_offset = 0;
  if (blackhole_table_id_ != RT_TABLE_UNSPEC) {
    blackhole_offset = 1;
    for (const auto& uid : blackholed_uids_) {
      RoutingPolicyEntry entry(
          IPAddress::kFamilyIPv4, metric_, blackhole_table_id_, uid, uid);
      routing_table_->AddRule(interface_index_, entry);
      entry.family = IPAddress::kFamilyIPv6;
      routing_table_->AddRule(interface_index_, entry);
    }

    if (blackholed_addrs_) {
      blackholed_addrs_->Apply(base::Bind(
          [](Connection* connection, const IPAddress& addr) {
            // Add |addr| to blackhole table.
            RoutingPolicyEntry entry(addr.family(), connection->metric_,
                                     connection->blackhole_table_id_);
            entry.src = addr;
            connection->routing_table_->AddRule(connection->interface_index_,
                                                entry);
          },
          base::Unretained(this)));
    }
  }

  for (const auto& uid : allowed_uids_) {
    RoutingPolicyEntry entry(IPAddress::kFamilyIPv4,
        metric_ + blackhole_offset, table_id_, uid, uid);
    routing_table_->AddRule(interface_index_, entry);
    entry.family = IPAddress::kFamilyIPv6;
    routing_table_->AddRule(interface_index_, entry);
  }

  for (const auto& interface_name : allowed_iifs_) {
    RoutingPolicyEntry entry(IPAddress::kFamilyIPv4,
        metric_ + blackhole_offset, table_id_, interface_name);
    routing_table_->AddRule(interface_index_, entry);
    entry.family = IPAddress::kFamilyIPv6;
    routing_table_->AddRule(interface_index_, entry);
  }

  for (const auto& source_address : allowed_addrs_) {
    RoutingPolicyEntry entry(
      IPAddress::kFamilyIPv4, metric_ + blackhole_offset, table_id_);
    entry.src = source_address;
    entry.family = source_address.family();
    routing_table_->AddRule(interface_index_, entry);
  }

  if (use_if_addrs_) {
    RoutingPolicyEntry entry(
      IPAddress::kFamilyIPv4, metric_ + blackhole_offset, table_id_);
    if (is_primary_physical_) {
      // Main routing table contains kernel-added routes for source address
      // selection. Sending traffic there before all other rules for physical
      // interfaces (but after any VPN rules) ensures that physical interface
      // rules are not inadvertently too aggressive.
      entry.src = IPAddress(IPAddress::kFamilyIPv4);
      entry.priority -= 1;
      entry.table = RT_TABLE_MAIN;
      routing_table_->AddRule(interface_index_, entry);
      // Add a default routing rule to use the primary interface if there is
      // nothing better.
      entry.table = table_id_;
      entry.priority = RoutingTable::kRulePriorityMain - 1;
      routing_table_->AddRule(interface_index_, entry);
      entry.family = IPAddress::kFamilyIPv6;
      entry.src = IPAddress(IPAddress::kFamilyIPv6);
      routing_table_->AddRule(interface_index_, entry);
      entry.priority = metric_ + blackhole_offset;
    }

    // Otherwise, only select the per-device table if the outgoing packet's
    // src address matches the interface's addresses or the input interface is
    // this interface.
    //
    // TODO(crbug.com/941597) This may need to change when NDProxy allows guests
    // to provision IPv6 addresses.
    std::vector<DeviceInfo::AddressData> addr_data;
    bool ok = device_info_->GetAddresses(interface_index_, &addr_data);
    DCHECK(ok);
    for (const auto& data : addr_data) {
      entry.src = data.address;
      entry.family = data.address.family();
      routing_table_->AddRule(interface_index_, entry);
    }
    entry.family = IPAddress::kFamilyIPv4;
    entry.src = IPAddress(entry.family);
    entry.interface_name = interface_name_;
    routing_table_->AddRule(interface_index_, entry);
    entry.family = IPAddress::kFamilyIPv6;
    entry.src = IPAddress(entry.family);
    routing_table_->AddRule(interface_index_, entry);
  }
}

void Connection::AddInputInterfaceToRoutingTable(
    const std::string& interface_name) {
  if (base::ContainsValue(allowed_iifs_, interface_name))
    return;  // interface already whitelisted

  allowed_iifs_.push_back(interface_name);
  UpdateRoutingPolicy();
  routing_table_->FlushCache();
}

void Connection::RemoveInputInterfaceFromRoutingTable(
    const std::string& interface_name) {
  if (!base::ContainsValue(allowed_iifs_, interface_name))
    return;  // interface already removed

  base::Erase(allowed_iifs_, interface_name);
  UpdateRoutingPolicy();
  routing_table_->FlushCache();
}

void Connection::SetMetric(uint32_t metric, bool is_primary_physical) {
  SLOG(this, 2) << __func__ << " " << interface_name_
                << " (index " << interface_index_ << ")"
                << metric_ << " -> " << metric;
  if (metric == metric_) {
    return;
  }

  metric_ = metric;
  is_primary_physical_ = is_primary_physical;
  UpdateRoutingPolicy();

  PushDNSConfig();
  if (metric == kDefaultMetric) {
    DeviceRefPtr device = device_info_->GetDevice(interface_index_);
    if (device) {
      device->RequestPortalDetection();
    }
  }
  routing_table_->FlushCache();
}

bool Connection::IsDefault() const {
  return metric_ == kDefaultMetric;
}

void Connection::SetUseDNS(bool enable) {
  SLOG(this, 2) << __func__ << " " << interface_name_
                << " (index " << interface_index_ << ")"
                << use_dns_ << " -> " << enable;
  use_dns_ = enable;
}

void Connection::UpdateDNSServers(const vector<string>& dns_servers) {
  dns_servers_ = dns_servers;
  PushDNSConfig();
}

void Connection::PushDNSConfig() {
  if (!use_dns_) {
    return;
  }

  vector<string> domain_search = dns_domain_search_;
  if (domain_search.empty() && !dns_domain_name_.empty()) {
    SLOG(this, 2) << "Setting domain search to domain name "
                  << dns_domain_name_;
    domain_search.push_back(dns_domain_name_ + ".");
  }
  resolver_->SetDNSFromLists(dns_servers_, domain_search);
}

void Connection::RequestRouting() {
  if (routing_request_count_++ == 0) {
    DeviceRefPtr device = device_info_->GetDevice(interface_index_);
    DCHECK(device.get());
    if (!device) {
      LOG(ERROR) << "Device is NULL!";
      return;
    }
    device->SetLooseRouting(true);
  }
}

void Connection::ReleaseRouting() {
  DCHECK_GT(routing_request_count_, 0);
  if (--routing_request_count_ == 0) {
    DeviceRefPtr device = device_info_->GetDevice(interface_index_);
    DCHECK(device.get());
    if (!device) {
      LOG(ERROR) << "Device is NULL!";
      return;
    }
    device->SetLooseRouting(false);

    // Clear any cached routes that might have accumulated while reverse-path
    // filtering was disabled.
    routing_table_->FlushCache();
  }
}

string Connection::GetSubnetName() const {
  if (!local().IsValid()) {
    return "";
  }
  return base::StringPrintf("%s/%d",
                            local().GetNetworkPart().ToString().c_str(),
                            local().prefix());
}

void Connection::set_allowed_addrs(std::vector<IPAddress> addresses) {
  allowed_addrs_ = std::move(addresses);
}

bool Connection::FixGatewayReachability(const IPAddress& local,
                                        IPAddress* peer,
                                        IPAddress* gateway) {
  SLOG(nullptr, 2) << __func__
                   << " local " << local.ToString()
                   << ", peer " << peer->ToString()
                   << ", gateway " << gateway->ToString();

  if (peer->IsValid()) {
    // For a PPP connection:
    // 1) Never set a peer (point-to-point) address, because the kernel
    //    will create an implicit routing rule in RT_TABLE_MAIN rather
    //    than our preferred routing table.  If the peer IP is set to the
    //    public IP of a VPN gateway (see below) this creates a routing loop.
    //    If not, it still creates an undesired route.
    // 2) Don't bother setting a gateway address either, because it doesn't
    //    have an effect on a point-to-point link.  So `ip route show table 1`
    //    will just say something like:
    //        default dev ppp0 metric 10
    peer->SetAddressToDefault();
    gateway->SetAddressToDefault();
    return true;
  }

  if (!gateway->IsValid()) {
    LOG(WARNING) << "No gateway address was provided for this connection.";
    return false;
  }

  // The prefix check will usually fail on IPv6 because IPv6 gateways
  // typically use link-local addresses.
  if (local.CanReachAddress(*gateway) ||
      local.family() == IPAddress::kFamilyIPv6) {
    return true;
  }

  LOG(WARNING) << "Gateway "
               << gateway->ToString()
               << " is unreachable from local address/prefix "
               << local.ToString() << "/" << local.prefix();
  LOG(WARNING) << "Mitigating this by creating a link route to the gateway.";

  IPAddress gateway_with_max_prefix(*gateway);
  gateway_with_max_prefix.set_prefix(
      IPAddress::GetMaxPrefixLength(gateway_with_max_prefix.family()));
  IPAddress default_address(gateway->family());
  RoutingTableEntry entry(gateway_with_max_prefix,
                          default_address,
                          default_address,
                          0,
                          RT_SCOPE_LINK,
                          false,
                          table_id_,
                          RTN_UNICAST,
                          RoutingTableEntry::kDefaultTag);

  if (!routing_table_->AddRoute(interface_index_, entry)) {
    LOG(ERROR) << "Unable to add link-scoped route to gateway.";
    return false;
  }

  return true;
}

void Connection::SetMTU(int32_t mtu) {
  SLOG(this, 2) << __func__ << " " << mtu;
  // Make sure the MTU value is valid.
  if (mtu == IPConfig::kUndefinedMTU) {
    mtu = IPConfig::kDefaultMTU;
  } else {
    int min_mtu = IsIPv6() ? IPConfig::kMinIPv6MTU : IPConfig::kMinIPv4MTU;
    if (mtu < min_mtu) {
      SLOG(this, 2) << __func__ << " MTU " << mtu
                    << " is too small; adjusting up to " << min_mtu;
      mtu = min_mtu;
    }
  }

  rtnl_handler_->SetInterfaceMTU(interface_index_, mtu);
}

void Connection::NotifyBindersOnDisconnect() {
  // Note that this method may be invoked by the destructor.
  SLOG(this, 2) << __func__ << " @ " << interface_name_;

  while (!binders_.empty()) {
    // Pop the binder first and then notify it to ensure that each binder is
    // notified only once.
    Binder* binder = binders_.front();
    binders_.pop_front();
    binder->OnDisconnect();
  }
}

void Connection::AttachBinder(Binder* binder) {
  SLOG(this, 2) << __func__ << "(" << binder->name() << ")" << " @ "
                            << interface_name_;
  binders_.push_back(binder);
}

void Connection::DetachBinder(Binder* binder) {
  SLOG(this, 2) << __func__ << "(" << binder->name() << ")" << " @ "
                            << interface_name_;
  for (auto it = binders_.begin(); it != binders_.end(); ++it) {
    if (binder == *it) {
      binders_.erase(it);
      return;
    }
  }
}

bool Connection::IsIPv6() {
  return local_.family() == IPAddress::kFamilyIPv6;
}

}  // namespace shill
