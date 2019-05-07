// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/routing_table.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/fib_rules.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>  // NOLINT - must be included after netinet/ether.h
#include <net/if_arp.h>
#include <netinet/ether.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <limits>
#include <string>

#include <base/bind.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/strings/stringprintf.h>

#include "shill/ipconfig.h"
#include "shill/logging.h"
#include "shill/net/byte_string.h"
#include "shill/net/rtnl_handler.h"
#include "shill/net/rtnl_listener.h"
#include "shill/net/rtnl_message.h"
#include "shill/routing_table_entry.h"

using base::Bind;
using base::FilePath;
using base::Unretained;
using std::string;
using std::vector;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kRoute;
static string ObjectID(RoutingTable* r) { return "(routing_table)"; }
}

namespace {

base::LazyInstance<RoutingTable>::DestructorAtExit g_routing_table =
    LAZY_INSTANCE_INITIALIZER;

// These don't have named constants in the system header files, but they
// are documented in ip-rule(8) and hardcoded in net/ipv4/fib_rules.c.
const uint32_t kRulePriorityLocal = 0;
const uint32_t kRulePriorityMain = 32766;

const char kRouteFlushPath4[] = "/proc/sys/net/ipv4/route/flush";
const char kRouteFlushPath6[] = "/proc/sys/net/ipv6/route/flush";

bool ParseRoutingTableMessage(const RTNLMessage& message,
                              int* interface_index,
                              RoutingTableEntry* entry) {
  if (message.type() != RTNLMessage::kTypeRoute ||
      message.family() == IPAddress::kFamilyUnknown ||
      !message.HasAttribute(RTA_OIF)) {
    return false;
  }

  const RTNLMessage::RouteStatus& route_status = message.route_status();

  if (route_status.type != RTN_UNICAST) {
    return false;
  }

  uint32_t interface_index_u32 = 0;
  if (!message.GetAttribute(RTA_OIF).ConvertToCPUUInt32(&interface_index_u32)) {
    return false;
  }
  *interface_index = interface_index_u32;

  uint32_t metric = 0;
  if (message.HasAttribute(RTA_PRIORITY)) {
    message.GetAttribute(RTA_PRIORITY).ConvertToCPUUInt32(&metric);
  }

  IPAddress default_addr(message.family());
  default_addr.SetAddressToDefault();

  ByteString dst_bytes(default_addr.address());
  if (message.HasAttribute(RTA_DST)) {
    dst_bytes = message.GetAttribute(RTA_DST);
  }
  ByteString src_bytes(default_addr.address());
  if (message.HasAttribute(RTA_SRC)) {
    src_bytes = message.GetAttribute(RTA_SRC);
  }
  ByteString gateway_bytes(default_addr.address());
  if (message.HasAttribute(RTA_GATEWAY)) {
    gateway_bytes = message.GetAttribute(RTA_GATEWAY);
  }

  entry->dst = IPAddress(message.family(), dst_bytes, route_status.dst_prefix);
  entry->src = IPAddress(message.family(), src_bytes, route_status.src_prefix);
  entry->gateway = IPAddress(message.family(), gateway_bytes);
  entry->metric = metric;
  entry->scope = route_status.scope;
  entry->from_rtnl = true;
  entry->table = route_status.table;
  entry->type = route_status.type;

  return true;
}

}  // namespace

RoutingTable::RoutingTable() : rtnl_handler_(RTNLHandler::GetInstance()) {
  SLOG(this, 2) << __func__;
}

RoutingTable::~RoutingTable() = default;

RoutingTable* RoutingTable::GetInstance() {
  return g_routing_table.Pointer();
}

void RoutingTable::Start() {
  SLOG(this, 2) << __func__;

  route_listener_.reset(
      new RTNLListener(RTNLHandler::kRequestRoute | RTNLHandler::kRequestRule,
                       Bind(&RoutingTable::RouteMsgHandler, Unretained(this))));
  rtnl_handler_->RequestDump(RTNLHandler::kRequestRoute);
  rtnl_handler_->RequestDump(RTNLHandler::kRequestRule);

  for (uint8_t i = RT_TABLE_DEFAULT - 1; i > RT_TABLE_UNSPEC; i--) {
    available_table_ids_.push_back(i);
  }
}

void RoutingTable::Stop() {
  SLOG(this, 2) << __func__;

  available_table_ids_.clear();
  route_listener_.reset();
}

bool RoutingTable::AddRoute(int interface_index,
                            const RoutingTableEntry& entry) {
  CHECK(!entry.from_rtnl);
  if (!AddRouteToKernelTable(interface_index, entry)) {
    return false;
  }
  tables_[interface_index].push_back(entry);
  return true;
}

bool RoutingTable::RemoveRoute(int interface_index,
                               const RoutingTableEntry& entry) {
  if (!RemoveRouteFromKernelTable(interface_index, entry)) {
    return false;
  }
  RouteTableEntryVector& table = tables_[interface_index];
  for (auto nent = table.begin(); nent != table.end(); ++nent) {
    if (nent->Equals(entry)) {
      table.erase(nent);
      return true;
    }
  }
  SLOG(this, 1) << "Successfully removed routing entry but could not find the "
                << "corresponding entry in shill's representation of the "
                << "routing table.";
  return true;
}

bool RoutingTable::GetDefaultRoute(int interface_index,
                                   IPAddress::Family family,
                                   RoutingTableEntry* entry) {
  RoutingTableEntry* found_entry;
  bool ret = GetDefaultRouteInternal(interface_index, family, &found_entry);
  if (ret) {
    *entry = *found_entry;
  }
  return ret;
}

bool RoutingTable::GetDefaultRouteInternal(int interface_index,
                                           IPAddress::Family family,
                                           RoutingTableEntry** entry) {
  SLOG(this, 2) << __func__ << " index " << interface_index
                << " family " << IPAddress::GetAddressFamilyName(family);

  RouteTables::iterator table = tables_.find(interface_index);
  if (table == tables_.end()) {
    SLOG(this, 2) << __func__ << " no table";
    return false;
  }

  // For IPv6 the kernel will add a new default route with metric 1024
  // every time it sees a router advertisement (which could happen every
  // couple of seconds).  Ignore these when there is another default route
  // with a lower metric.
  uint32_t lowest_metric = UINT_MAX;
  for (auto& nent : table->second) {
    if (nent.dst.IsDefault() &&
        nent.dst.family() == family &&
        nent.metric < lowest_metric) {
      *entry = &nent;
      lowest_metric = nent.metric;
    }
  }

  if (lowest_metric == UINT_MAX) {
    SLOG(this, 2) << __func__ << " no route";
    return false;
  } else {
    SLOG(this, 2) << __func__ << ": found"
                  << " gateway " << (*entry)->gateway.ToString()
                  << " metric " << (*entry)->metric;
    return true;
  }
}

bool RoutingTable::SetDefaultRoute(int interface_index,
                                   const IPAddress& gateway_address,
                                   uint32_t metric,
                                   uint8_t table_id) {
  SLOG(this, 2) << __func__ << " index " << interface_index
                << " metric " << metric;

  RoutingTableEntry* old_entry;

  // metric 0 isn't allowed on IPv6; it will create a metric 1024 route
  // and cause |tables_| to get out of sync with the kernel.
  DCHECK_NE(metric, 0U);

  if (GetDefaultRouteInternal(interface_index,
                              gateway_address.family(),
                              &old_entry)) {
    if (old_entry->gateway.Equals(gateway_address)) {
      if (old_entry->metric != metric) {
        ReplaceMetric(interface_index, old_entry, metric);
      }
      return true;
    } else {
      if (!RemoveRoute(interface_index, *old_entry)) {
        LOG(WARNING) << "Failed to remove old default route for interface "
                     << interface_index;
      }
    }
  }

  IPAddress default_address(gateway_address.family());
  default_address.SetAddressToDefault();

  return AddRoute(interface_index,
                  RoutingTableEntry(default_address,
                                    default_address,
                                    gateway_address,
                                    metric,
                                    RT_SCOPE_UNIVERSE,
                                    false,
                                    table_id,
                                    RTN_UNICAST,
                                    RoutingTableEntry::kDefaultTag));
}

bool RoutingTable::ConfigureRoutes(int interface_index,
                                   const IPConfigRefPtr& ipconfig,
                                   uint32_t metric,
                                   uint8_t table_id) {
  bool ret = true;

  IPAddress::Family address_family = ipconfig->properties().address_family;
  const vector<IPConfig::Route>& routes = ipconfig->properties().routes;

  for (const auto& route : routes) {
    SLOG(this, 3) << "Installing route:"
                  << " Destination: " << route.host
                  << " Prefix: " << route.prefix
                  << " Gateway: " << route.gateway;
    IPAddress destination_address(address_family);
    IPAddress source_address(address_family);  // Left as default.
    IPAddress gateway_address(address_family);
    if (!destination_address.SetAddressFromString(route.host)) {
      LOG(ERROR) << "Failed to parse host "
                 << route.host;
      ret = false;
      continue;
    }
    if (!gateway_address.SetAddressFromString(route.gateway)) {
      LOG(ERROR) << "Failed to parse gateway "
                 << route.gateway;
      ret = false;
      continue;
    }
    destination_address.set_prefix(route.prefix);
    if (!AddRoute(interface_index,
                  RoutingTableEntry(destination_address,
                                    source_address,
                                    gateway_address,
                                    metric,
                                    RT_SCOPE_UNIVERSE,
                                    false,
                                    table_id,
                                    RTN_UNICAST,
                                    RoutingTableEntry::kDefaultTag))) {
      ret = false;
    }
  }
  return ret;
}

void RoutingTable::FlushRoutes(int interface_index) {
  SLOG(this, 2) << __func__;

  auto table = tables_.find(interface_index);
  if (table == tables_.end()) {
    return;
  }

  for (const auto& nent : table->second) {
    RemoveRouteFromKernelTable(interface_index, nent);
  }
  table->second.clear();
}

void RoutingTable::FlushRoutesWithTag(int tag) {
  SLOG(this, 2) << __func__;

  for (auto& table : tables_) {
    for (auto nent = table.second.begin(); nent != table.second.end();) {
      if (nent->tag == tag) {
        RemoveRouteFromKernelTable(table.first, *nent);
        nent = table.second.erase(nent);
      } else {
        ++nent;
      }
    }
  }
}

void RoutingTable::ResetTable(int interface_index) {
  tables_.erase(interface_index);
}

void RoutingTable::SetDefaultMetric(int interface_index, uint32_t metric) {
  SLOG(this, 2) << __func__ << " index " << interface_index
                << " metric " << metric;

  RoutingTableEntry* entry;
  if (GetDefaultRouteInternal(
          interface_index, IPAddress::kFamilyIPv4, &entry) &&
      entry->metric != metric) {
    ReplaceMetric(interface_index, entry, metric);
  }

  if (GetDefaultRouteInternal(
          interface_index, IPAddress::kFamilyIPv6, &entry) &&
      entry->metric != metric) {
    ReplaceMetric(interface_index, entry, metric);
  }
}

bool RoutingTable::AddRouteToKernelTable(int interface_index,
                                         const RoutingTableEntry& entry) {
  SLOG(this, 2) << __func__ << ": " << " index " << interface_index << " "
                << entry;

  return ApplyRoute(interface_index, entry, RTNLMessage::kModeAdd,
                    NLM_F_CREATE | NLM_F_EXCL);
}

bool RoutingTable::RemoveRouteFromKernelTable(int interface_index,
                                              const RoutingTableEntry& entry) {
  SLOG(this, 2) << __func__ << ": " << " index " << interface_index << " "
                << entry;

  return ApplyRoute(interface_index, entry, RTNLMessage::kModeDelete, 0);
}

void RoutingTable::RouteMsgHandler(const RTNLMessage& message) {
  int interface_index;
  RoutingTableEntry entry;

  if (HandleRoutingPolicyMessage(message)) {
    return;
  }

  if (!ParseRoutingTableMessage(message, &interface_index, &entry)) {
    return;
  }

  if (!route_queries_.empty() &&
      message.route_status().protocol == RTPROT_UNSPEC) {
    SLOG(this, 3) << __func__ << ": Message seq: " << message.seq()
                  << " mode " << message.mode()
                  << ", next query seq: " << route_queries_.front().sequence;

    // Purge queries that have expired (sequence number of this message is
    // greater than that of the head of the route query sequence).  Do the
    // math in a way that's roll-over independent.
    const auto kuint32max = std::numeric_limits<uint32_t>::max();
    while (route_queries_.front().sequence - message.seq() > kuint32max / 2) {
      LOG(ERROR) << __func__ << ": Purging un-replied route request sequence "
                 << route_queries_.front().sequence
                 << " (< " << message.seq() << ")";
      route_queries_.pop_front();
      if (route_queries_.empty())
        return;
    }

    const Query& query = route_queries_.front();
    if (query.sequence == message.seq()) {
      RoutingTableEntry add_entry(entry);
      add_entry.from_rtnl = false;
      add_entry.tag = query.tag;
      add_entry.table = query.table_id;
      bool added = true;
      if (add_entry.gateway.IsDefault()) {
        SLOG(this, 2) << __func__ << ": Ignoring route result with no gateway "
                      << "since we don't need to plumb these.";
      } else {
        SLOG(this, 2) << __func__ << ": Adding host route to "
                      << add_entry.dst.ToString();
        added = AddRoute(interface_index, add_entry);
      }
      if (added && !query.callback.is_null()) {
        SLOG(this, 2) << "Running query callback.";
        query.callback.Run(interface_index, add_entry);
      }
      route_queries_.pop_front();
    }
    return;
  } else if (message.route_status().protocol == RTPROT_RA) {
    // The kernel sends one of these messages pretty much every time it
    // connects to another IPv6 host.  The only interesting message is the
    // one containing the default gateway.
    if (!entry.dst.IsDefault() || !entry.gateway.IsValid())
      return;
  } else if (message.route_status().protocol != RTPROT_BOOT) {
    // Responses to route queries come back with a protocol of
    // RTPROT_UNSPEC.  Otherwise, normal route updates that we are
    // interested in come with a protocol of RTPROT_BOOT.
    return;
  }

  RouteTableEntryVector& table = tables_[interface_index];
  for (auto nent = table.begin(); nent != table.end(); ++nent)  {
    if (nent->dst.Equals(entry.dst) &&
        nent->src.Equals(entry.src) &&
        nent->gateway.Equals(entry.gateway) &&
        nent->scope == entry.scope &&
        nent->metric == entry.metric &&
        nent->type == entry.type) {
      if (message.mode() == RTNLMessage::kModeDelete) {
        table.erase(nent);
      } else if (message.mode() == RTNLMessage::kModeAdd) {
        nent->from_rtnl = true;
      }
      return;
    }
  }

  if (message.mode() == RTNLMessage::kModeAdd) {
    SLOG(this, 2) << __func__ << " adding"
                  << " destination " << entry.dst.ToString()
                  << " index " << interface_index
                  << " gateway " << entry.gateway.ToString()
                  << " metric " << entry.metric;
    table.push_back(entry);
  }
}

bool RoutingTable::ApplyRoute(uint32_t interface_index,
                              const RoutingTableEntry& entry,
                              RTNLMessage::Mode mode,
                              unsigned int flags) {
  SLOG(this, 2) << base::StringPrintf(
      "%s: dst %s/%d src %s/%d index %d mode %d flags 0x%x",
      __func__, entry.dst.ToString().c_str(), entry.dst.prefix(),
      entry.src.ToString().c_str(), entry.src.prefix(),
      interface_index, mode, flags);

  RTNLMessage message(
      RTNLMessage::kTypeRoute,
      mode,
      NLM_F_REQUEST | flags,
      0,
      0,
      0,
      entry.dst.family());

  message.set_route_status(RTNLMessage::RouteStatus(
      entry.dst.prefix(),
      entry.src.prefix(),
      entry.table,
      RTPROT_BOOT,
      entry.scope,
      entry.type,
      0));

  if (entry.type != RTN_BLACKHOLE) {
    message.SetAttribute(RTA_DST, entry.dst.address());
  }
  if (!entry.src.IsDefault()) {
    message.SetAttribute(RTA_SRC, entry.src.address());
  }
  if (!entry.gateway.IsDefault()) {
    message.SetAttribute(RTA_GATEWAY, entry.gateway.address());
  }
  message.SetAttribute(RTA_PRIORITY,
                       ByteString::CreateFromCPUUInt32(entry.metric));

  if (entry.type == RTN_UNICAST) {
    // Note that RouteMsgHandler will ignore anything without RTA_OIF,
    // because that is how it looks up the |tables_| vector.  But
    // FlushRoutes() and FlushRoutesWithTag() do not care.
    message.SetAttribute(RTA_OIF,
                         ByteString::CreateFromCPUUInt32(interface_index));
  }

  return rtnl_handler_->SendMessage(&message);
}

// Somewhat surprisingly, the kernel allows you to create multiple routes
// to the same destination through the same interface with different metrics.
// Therefore, to change the metric on a route, we can't just use the
// NLM_F_REPLACE flag by itself.  We have to explicitly remove the old route.
// We do so after creating the route at a new metric so there is no traffic
// disruption to existing network streams.
void RoutingTable::ReplaceMetric(uint32_t interface_index,
                                 RoutingTableEntry* entry,
                                 uint32_t metric) {
  SLOG(this, 2) << __func__ << " index " << interface_index
                << " metric " << metric;
  RoutingTableEntry new_entry = *entry;
  new_entry.metric = metric;
  // First create the route at the new metric.
  ApplyRoute(interface_index, new_entry, RTNLMessage::kModeAdd,
             NLM_F_CREATE | NLM_F_REPLACE);
  // Then delete the route at the old metric.
  RemoveRouteFromKernelTable(interface_index, *entry);
  // Now, update our routing table (via |*entry|) from |new_entry|.
  *entry = new_entry;
}

bool RoutingTable::FlushCache() {
  static const char* const kPaths[] = {kRouteFlushPath4, kRouteFlushPath6};
  bool ret = true;

  SLOG(this, 2) << __func__;

  for (auto path : kPaths) {
    if (base::WriteFile(FilePath(path), "-1", 2) != 2) {
      LOG(ERROR) << base::StringPrintf("Cannot write to route flush file %s",
                                       path);
      ret = false;
    }
  }

  return ret;
}

bool RoutingTable::RequestRouteToHost(const IPAddress& address,
                                      int interface_index,
                                      int tag,
                                      const QueryCallback& callback,
                                      uint8_t table_id) {
  // Make sure we don't get a cached response that is no longer valid.
  FlushCache();

  RTNLMessage message(
      RTNLMessage::kTypeRoute,
      RTNLMessage::kModeQuery,
      NLM_F_REQUEST,
      0,
      0,
      interface_index,
      address.family());

  RTNLMessage::RouteStatus status;
  status.dst_prefix = address.prefix();
  message.set_route_status(status);
  message.SetAttribute(RTA_DST, address.address());

  if (interface_index != -1) {
    message.SetAttribute(RTA_OIF,
                         ByteString::CreateFromCPUUInt32(interface_index));
  }

  if (!rtnl_handler_->SendMessage(&message)) {
    return false;
  }

  // Save the sequence number of the request so we can create a route for
  // this host when we get a reply.
  route_queries_.push_back(Query(message.seq(), tag, callback, table_id));

  return true;
}

bool RoutingTable::CreateBlackholeRoute(int interface_index,
                                        IPAddress::Family family,
                                        uint32_t metric,
                                        uint8_t table_id) {
  SLOG(this, 2) << base::StringPrintf(
      "%s: family %s metric %d",
      __func__, IPAddress::GetAddressFamilyName(family).c_str(), metric);

  IPAddress any_addr(family);
  RoutingTableEntry entry(any_addr,
                          any_addr,
                          any_addr,
                          metric,
                          RT_SCOPE_UNIVERSE,
                          false,
                          table_id,
                          RTN_BLACKHOLE,
                          0);
  return AddRoute(interface_index, entry);
}

bool RoutingTable::CreateLinkRoute(int interface_index,
                                   const IPAddress& local_address,
                                   const IPAddress& remote_address,
                                   uint8_t table_id) {
  if (!local_address.CanReachAddress(remote_address)) {
    LOG(ERROR) << __func__ << " failed: "
               << remote_address.ToString() << " is not reachable from "
               << local_address.ToString();
    return false;
  }

  IPAddress default_address(local_address.family());
  default_address.SetAddressToDefault();
  IPAddress destination_address(remote_address);
  destination_address.set_prefix(
      IPAddress::GetMaxPrefixLength(remote_address.family()));
  SLOG(this, 2) << "Creating link route to " << destination_address.ToString()
                << " from " << local_address.ToString()
                << " on interface index " << interface_index;
  return AddRoute(interface_index,
                  RoutingTableEntry(destination_address,
                                    local_address,
                                    default_address,
                                    0,
                                    RT_SCOPE_LINK,
                                    false,
                                    table_id,
                                    RTN_UNICAST,
                                    RoutingTableEntry::kDefaultTag));
}

bool RoutingTable::ApplyRule(uint32_t interface_index,
                             const RoutingPolicyEntry& entry,
                             RTNLMessage::Mode mode,
                             unsigned int flags) {
  SLOG(this, 2) << base::StringPrintf(
      "%s: index %d family %s prio %d",
      __func__,
      interface_index,
      IPAddress::GetAddressFamilyName(entry.family).c_str(),
      entry.priority);

  RTNLMessage message(RTNLMessage::kTypeRule,
                      mode,
                      NLM_F_REQUEST | flags,
                      0,
                      0,
                      0,
                      entry.family);

  message.set_route_status(
      RTNLMessage::RouteStatus(entry.dst.prefix(),
                               entry.src.prefix(),
                               entry.table,
                               RTPROT_BOOT,
                               RT_SCOPE_UNIVERSE,
                               RTN_UNICAST,
                               entry.invert_rule ? FIB_RULE_INVERT : 0));

  message.SetAttribute(FRA_PRIORITY,
                       ByteString::CreateFromCPUUInt32(entry.priority));
  if (entry.has_fwmark) {
    message.SetAttribute(FRA_FWMARK,
                         ByteString::CreateFromCPUUInt32(entry.fwmark_value));
    message.SetAttribute(FRA_FWMASK,
                         ByteString::CreateFromCPUUInt32(entry.fwmark_mask));
  }
  if (entry.has_uidrange) {
    struct fib_rule_uid_range r = {
        .start = entry.uidrange_start, .end = entry.uidrange_end,
    };
    message.SetAttribute(
        FRA_UID_RANGE,
        ByteString(reinterpret_cast<const unsigned char*>(&r), sizeof(r)));
  }
  if (!entry.interface_name.empty()) {
    message.SetAttribute(FRA_IFNAME,
                         ByteString(entry.interface_name, true));
  }
  if (!entry.dst.IsDefault()) {
    message.SetAttribute(FRA_DST, entry.dst.address());
  }
  if (!entry.src.IsDefault()) {
    message.SetAttribute(FRA_SRC, entry.src.address());
  }

  return rtnl_handler_->SendMessage(&message);
}

bool RoutingTable::ParseRoutingPolicyMessage(const RTNLMessage& message,
                                             RoutingPolicyEntry* entry) {
  if (message.type() != RTNLMessage::kTypeRule ||
      message.family() == IPAddress::kFamilyUnknown) {
    return false;
  }

  const RTNLMessage::RouteStatus& route_status = message.route_status();
  if (route_status.type != RTN_UNICAST) {
    return false;
  }

  entry->family = message.family();
  entry->table = route_status.table;
  entry->invert_rule = !!(route_status.flags & FIB_RULE_INVERT);

  if (message.HasAttribute(FRA_PRIORITY)) {
    // Rule 0 (local table) doesn't have a priority attribute.
    if (!message.GetAttribute(FRA_PRIORITY)
             .ConvertToCPUUInt32(&entry->priority)) {
      return false;
    }
  }

  if (message.HasAttribute(FRA_FWMARK)) {
    entry->has_fwmark = true;
    if (!message.GetAttribute(FRA_FWMARK)
             .ConvertToCPUUInt32(&entry->fwmark_value)) {
      return false;
    }
    if (message.HasAttribute(FRA_FWMASK)) {
      if (!message.GetAttribute(FRA_FWMASK)
               .ConvertToCPUUInt32(&entry->fwmark_mask)) {
        return false;
      }
    }
  }

  if (message.HasAttribute(FRA_UID_RANGE)) {
    struct fib_rule_uid_range r;
    if (!message.GetAttribute(FRA_UID_RANGE).CopyData(sizeof(r), &r)) {
      return false;
    }
    entry->has_uidrange = true;
    entry->uidrange_start = r.start;
    entry->uidrange_end = r.end;
  }

  if (message.HasAttribute(FRA_IFNAME)) {
    entry->interface_name = reinterpret_cast<const char*>(
        message.GetAttribute(FRA_IFNAME).GetConstData());
  }

  IPAddress default_addr(message.family());
  default_addr.SetAddressToDefault();

  ByteString dst_bytes(default_addr.address());
  if (message.HasAttribute(FRA_DST)) {
    dst_bytes = message.GetAttribute(FRA_DST);
  }
  ByteString src_bytes(default_addr.address());
  if (message.HasAttribute(FRA_SRC)) {
    src_bytes = message.GetAttribute(FRA_SRC);
  }

  entry->dst = IPAddress(message.family(), dst_bytes, route_status.dst_prefix);
  entry->src = IPAddress(message.family(), src_bytes, route_status.src_prefix);

  return true;
}

bool RoutingTable::HandleRoutingPolicyMessage(const RTNLMessage& message) {
  RoutingPolicyEntry entry;

  if (!ParseRoutingPolicyMessage(message, &entry)) {
    return false;
  }

  if (!(entry.priority > kRulePriorityLocal &&
        entry.priority < kRulePriorityMain)) {
    // Don't touch the system-managed rules.
    return true;
  }

  // If this rule matches one of our known rules, ignore it.  Otherwise,
  // assume it is left over from an old run and delete it.
  for (auto& table : policy_tables_) {
    for (auto nent = table.second.begin(); nent != table.second.end(); ++nent) {
      if (nent->Equals(entry)) {
        return true;
      }
    }
  }

  ApplyRule(-1, entry, RTNLMessage::kModeDelete, 0);
  return true;
}

bool RoutingTable::AddRule(int interface_index,
                           const RoutingPolicyEntry& entry) {
  if (!ApplyRule(interface_index,
                 entry,
                 RTNLMessage::kModeAdd,
                 NLM_F_CREATE | NLM_F_EXCL)) {
    return false;
  }
  policy_tables_[interface_index].push_back(entry);
  return true;
}

void RoutingTable::FlushRules(int interface_index) {
  SLOG(this, 2) << __func__;

  auto table = policy_tables_.find(interface_index);
  if (table == policy_tables_.end()) {
    return;
  }

  for (const auto& nent : table->second) {
    ApplyRule(interface_index, nent, RTNLMessage::kModeDelete, 0);
  }
  table->second.clear();
}

uint8_t RoutingTable::AllocTableId() {
  if (available_table_ids_.empty()) {
    return 0;
  } else {
    uint8_t table_id = available_table_ids_.back();
    available_table_ids_.pop_back();

    // Flush any entries currently in this table before letting the caller
    // use it.
    for (auto& table : tables_) {
      for (auto nent = table.second.begin(); nent != table.second.end();) {
        if (nent->table == table_id) {
          RemoveRouteFromKernelTable(table.first, *nent);
          nent = table.second.erase(nent);
        } else {
          ++nent;
        }
      }
    }
    return table_id;
  }
}

void RoutingTable::FreeTableId(uint8_t id) {
  if (id == RT_TABLE_MAIN) {
    return;
  }
  CHECK(id > RT_TABLE_UNSPEC && id < RT_TABLE_DEFAULT);
  available_table_ids_.push_back(id);
}

}  // namespace shill
