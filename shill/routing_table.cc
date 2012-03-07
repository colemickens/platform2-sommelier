// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/routing_table.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <netinet/ether.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <string>

#include <base/callback_old.h>
#include <base/file_path.h>
#include <base/file_util.h>
#include <base/hash_tables.h>
#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <base/stl_util-inl.h>
#include <base/stringprintf.h>

#include "shill/byte_string.h"
#include "shill/routing_table_entry.h"
#include "shill/rtnl_handler.h"
#include "shill/rtnl_listener.h"
#include "shill/rtnl_message.h"

using std::string;
using std::vector;

namespace shill {

static base::LazyInstance<RoutingTable> g_routing_table(
    base::LINKER_INITIALIZED);

// static
const char RoutingTable::kRouteFlushPath4[] = "/proc/sys/net/ipv4/route/flush";
// static
const char RoutingTable::kRouteFlushPath6[] = "/proc/sys/net/ipv6/route/flush";

RoutingTable::RoutingTable()
    : route_callback_(NewCallback(this, &RoutingTable::RouteMsgHandler)),
      route_listener_(NULL),
      rtnl_handler_(RTNLHandler::GetInstance()) {
  VLOG(2) << __func__;
}

RoutingTable::~RoutingTable() {}

RoutingTable* RoutingTable::GetInstance() {
  return g_routing_table.Pointer();
}

void RoutingTable::Start() {
  VLOG(2) << __func__;

  route_listener_.reset(
      new RTNLListener(RTNLHandler::kRequestRoute, route_callback_.get()));
  rtnl_handler_->RequestDump(RTNLHandler::kRequestRoute);
}

void RoutingTable::Stop() {
  VLOG(2) << __func__;

  route_listener_.reset();
}

bool RoutingTable::AddRoute(int interface_index,
                            const RoutingTableEntry &entry) {
  VLOG(2) << __func__ << ": "
          << "destination " << entry.dst.ToString()
          << " index " << interface_index
          << " gateway " << entry.gateway.ToString()
          << " metric " << entry.metric;

  CHECK(!entry.from_rtnl);
  if (!ApplyRoute(interface_index,
                  entry,
                  RTNLMessage::kModeAdd,
                  NLM_F_CREATE | NLM_F_EXCL)) {
    return false;
  }
  tables_[interface_index].push_back(entry);
  return true;
}

bool RoutingTable::GetDefaultRoute(int interface_index,
                                   IPAddress::Family family,
                                   RoutingTableEntry *entry) {
  RoutingTableEntry *found_entry;
  bool ret = GetDefaultRouteInternal(interface_index, family, &found_entry);
  if (ret) {
    *entry = *found_entry;
  }
  return ret;
}

bool RoutingTable::GetDefaultRouteInternal(int interface_index,
                                           IPAddress::Family family,
                                           RoutingTableEntry **entry) {
  VLOG(2) << __func__ << " index " << interface_index
          << " family " << IPAddress::GetAddressFamilyName(family);

  base::hash_map<int, vector<RoutingTableEntry> >::iterator table =
    tables_.find(interface_index);

  if (table == tables_.end()) {
    VLOG(2) << __func__ << " no table";
    return false;
  }

  vector<RoutingTableEntry>::iterator nent;

  for (nent = table->second.begin(); nent != table->second.end(); ++nent) {
    if (nent->dst.IsDefault() && nent->dst.family() == family) {
      *entry = &(*nent);
      VLOG(2) << __func__ << ": found"
              << " gateway " << nent->gateway.ToString()
              << " metric " << nent->metric;
      return true;
    }
  }

  VLOG(2) << __func__ << " no route";
  return false;
}

bool RoutingTable::SetDefaultRoute(int interface_index,
                                   const IPConfigRefPtr &ipconfig,
                                   uint32 metric) {
  VLOG(2) << __func__ << " index " << interface_index << " metric " << metric;

  const IPConfig::Properties &ipconfig_props = ipconfig->properties();
  RoutingTableEntry *old_entry;
  IPAddress gateway_address(ipconfig_props.address_family);
  if (!gateway_address.SetAddressFromString(ipconfig_props.gateway)) {
    return false;
  }

  if (GetDefaultRouteInternal(interface_index,
                              ipconfig_props.address_family,
                              &old_entry)) {
    if (old_entry->gateway.Equals(gateway_address)) {
      if (old_entry->metric != metric) {
        ReplaceMetric(interface_index, old_entry, metric);
      }
      return true;
    } else {
      // TODO(quiche): Update internal state as well?
      ApplyRoute(interface_index,
                 *old_entry,
                 RTNLMessage::kModeDelete,
                 0);
    }
  }

  IPAddress default_address(ipconfig_props.address_family);
  default_address.SetAddressToDefault();

  return AddRoute(interface_index,
                  RoutingTableEntry(default_address,
                                    default_address,
                                    gateway_address,
                                    metric,
                                    RT_SCOPE_UNIVERSE,
                                    false));
}

void RoutingTable::FlushRoutes(int interface_index) {
  VLOG(2) << __func__;

  base::hash_map<int, vector<RoutingTableEntry> >::iterator table =
    tables_.find(interface_index);

  if (table == tables_.end()) {
    return;
  }

  vector<RoutingTableEntry>::iterator nent;

  for (nent = table->second.begin(); nent != table->second.end(); ++nent) {
      ApplyRoute(interface_index, *nent, RTNLMessage::kModeDelete, 0);
  }
  table->second.clear();
}

void RoutingTable::ResetTable(int interface_index) {
  tables_.erase(interface_index);
}

void RoutingTable::SetDefaultMetric(int interface_index, uint32 metric) {
  VLOG(2) << __func__ << " index " << interface_index << " metric " << metric;

  RoutingTableEntry *entry;
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

// static
bool RoutingTable::ParseRoutingTableMessage(const RTNLMessage &message,
                                            int *interface_index,
                                            RoutingTableEntry *entry) {
  if (message.type() != RTNLMessage::kTypeRoute ||
      message.family() == IPAddress::kFamilyUnknown ||
      !message.HasAttribute(RTA_OIF)) {
    return false;
  }

  const RTNLMessage::RouteStatus &route_status = message.route_status();

  if (route_status.type != RTN_UNICAST ||
      route_status.table != RT_TABLE_MAIN) {
    return false;
  }

  uint32 interface_index_u32 = 0;
  if (!message.GetAttribute(RTA_OIF).ConvertToCPUUInt32(&interface_index_u32)) {
    return false;
  }
  *interface_index = interface_index_u32;

  uint32 metric = 0;
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

  return true;
}

void RoutingTable::RouteMsgHandler(const RTNLMessage &message) {
  int interface_index;
  RoutingTableEntry entry;

  if (!ParseRoutingTableMessage(message, &interface_index, &entry)) {
    return;
  }

  if (!route_query_sequences_.empty() &&
      message.route_status().protocol == RTPROT_UNSPEC) {
    VLOG(3) << __func__ << ": Message seq: " << message.seq()
            << " mode " << message.mode()
            << ", next query seq: " << route_query_sequences_.front();

    // Purge queries that have expired (sequence number of this message is
    // greater than that of the head of the route query sequence).  Do the
    // math in a way that's roll-over independent.
    while (route_query_sequences_.front() - message.seq() > kuint32max / 2) {
      LOG(ERROR) << __func__ << ": Purging un-replied route request sequence "
                 << route_query_sequences_.front()
                 << " (< " << message.seq() << ")";
      route_query_sequences_.pop();
      if (route_query_sequences_.empty())
        return;
    }

    if (route_query_sequences_.front() == message.seq()) {
      VLOG(2) << __func__ << ": Adding host route to " << entry.dst.ToString();
      route_query_sequences_.pop();
      RoutingTableEntry add_entry(entry);
      add_entry.from_rtnl = false;
      AddRoute(interface_index, add_entry);
    }
    return;
  } else if (message.route_status().protocol != RTPROT_BOOT) {
    // Responses to route queries come back with a protocol of
    // RTPROT_UNSPEC.  Otherwise, normal route updates that we are
    // interested in come with a protocol of RTPROT_BOOT.
    return;
  }

  vector<RoutingTableEntry> &table = tables_[interface_index];
  vector<RoutingTableEntry>::iterator nent;
  for (nent = table.begin(); nent != table.end(); ++nent) {
    if (nent->dst.Equals(entry.dst) &&
        nent->src.Equals(entry.src) &&
        nent->gateway.Equals(entry.gateway) &&
        nent->scope == entry.scope) {
      if (message.mode() == RTNLMessage::kModeDelete &&
          nent->metric == entry.metric) {
        table.erase(nent);
      } else if (message.mode() == RTNLMessage::kModeAdd) {
        nent->from_rtnl = true;
        nent->metric = entry.metric;
      }
      return;
    }
  }

  if (message.mode() == RTNLMessage::kModeAdd) {
    VLOG(2) << __func__ << " adding"
            << " destination " << entry.dst.ToString()
            << " index " << interface_index
            << " gateway " << entry.gateway.ToString()
            << " metric " << entry.metric;
    table.push_back(entry);
  }
}

bool RoutingTable::ApplyRoute(uint32 interface_index,
                              const RoutingTableEntry &entry,
                              RTNLMessage::Mode mode,
                              unsigned int flags) {
  VLOG(2) << base::StringPrintf("%s: dst %s index %d mode %d flags 0x%x",
                                __func__, entry.dst.ToString().c_str(),
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
      RT_TABLE_MAIN,
      RTPROT_BOOT,
      entry.scope,
      RTN_UNICAST,
      0));

  message.SetAttribute(RTA_DST, entry.dst.address());
  if (!entry.src.IsDefault()) {
    message.SetAttribute(RTA_SRC, entry.src.address());
  }
  if (!entry.gateway.IsDefault()) {
    message.SetAttribute(RTA_GATEWAY, entry.gateway.address());
  }
  message.SetAttribute(RTA_PRIORITY,
                       ByteString::CreateFromCPUUInt32(entry.metric));
  message.SetAttribute(RTA_OIF,
                       ByteString::CreateFromCPUUInt32(interface_index));

  return rtnl_handler_->SendMessage(&message);
}

// Somewhat surprisingly, the kernel allows you to create multiple routes
// to the same destination through the same interface with different metrics.
// Therefore, to change the metric on a route, we can't just use the
// NLM_F_REPLACE flag by itself.  We have to explicitly remove the old route.
// We do so after creating the route at a new metric so there is no traffic
// disruption to existing network streams.
void RoutingTable::ReplaceMetric(uint32 interface_index,
                                 RoutingTableEntry *entry,
                                 uint32 metric) {
  VLOG(2) << __func__ << " index " << interface_index << " metric " << metric;
  RoutingTableEntry new_entry = *entry;
  new_entry.metric = metric;
  // First create the route at the new metric.
  ApplyRoute(interface_index, new_entry, RTNLMessage::kModeAdd,
             NLM_F_CREATE | NLM_F_REPLACE);
  // Then delete the route at the old metric.
  ApplyRoute(interface_index, *entry, RTNLMessage::kModeDelete, 0);
  // Now, update our routing table (via |*entry|) from |new_entry|.
  *entry = new_entry;
}

bool RoutingTable::FlushCache() {
  static const char *kPaths[2] = { kRouteFlushPath4, kRouteFlushPath6 };
  bool ret = true;

  VLOG(2) << __func__;

  for (size_t i = 0; i < arraysize(kPaths); ++i) {
    if (file_util::WriteFile(FilePath(kPaths[i]), "-1", 2) != 2) {
      LOG(ERROR) << base::StringPrintf("Cannot write to route flush file %s",
                                       kPaths[i]);
      ret = false;
    }
  }

  return ret;
}

bool RoutingTable::RequestRouteToHost(const IPAddress &address,
                                      int interface_index) {
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
  message.SetAttribute(RTA_OIF,
                       ByteString::CreateFromCPUUInt32(interface_index));

  if (!rtnl_handler_->SendMessage(&message)) {
    return false;
  }

  // Save the sequence number of the request so we can create a route for
  // this host when we get a reply.
  route_query_sequences_.push(message.seq());

  return true;
}

}  // namespace shill
