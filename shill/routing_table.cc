// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
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
      route_listener_(NULL) {
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
  RTNLHandler::GetInstance()->RequestDump(
      RTNLHandler::kRequestRoute);
}

void RoutingTable::Stop() {
  VLOG(2) << __func__;

  route_listener_.reset();
}

bool RoutingTable::AddRoute(int interface_index,
                            const RoutingTableEntry &entry) {
  VLOG(2) << __func__;

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
  VLOG(2) << __func__;

  base::hash_map<int, vector<RoutingTableEntry> >::iterator table =
    tables_.find(interface_index);

  if (table == tables_.end()) {
    return false;
  }

  vector<RoutingTableEntry>::iterator nent;

  for (nent = table->second.begin(); nent != table->second.end(); ++nent) {
    if (nent->dst.IsDefault() && nent->dst.family() == family) {
      *entry = *nent;
      return true;
    }
  }

  return false;
}

bool RoutingTable::SetDefaultRoute(int interface_index,
                                   const IPConfigRefPtr &ipconfig,
                                   uint32 metric) {
  const IPConfig::Properties &ipconfig_props = ipconfig->properties();
  RoutingTableEntry old_entry;

  VLOG(2) << __func__;

  IPAddress gateway_address(ipconfig_props.address_family);
  if (!gateway_address.SetAddressFromString(ipconfig_props.gateway)) {
    return false;
  }

  if (GetDefaultRoute(interface_index,
                      ipconfig_props.address_family,
                      &old_entry)) {
    if (old_entry.gateway.Equals(gateway_address)) {
      if (old_entry.metric != metric) {
        old_entry.metric = metric;
        ApplyRoute(interface_index, old_entry, RTNLMessage::kModeAdd,
                   NLM_F_CREATE | NLM_F_REPLACE);
      }
      return true;
    } else {
      ApplyRoute(interface_index,
                 old_entry,
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
}

void RoutingTable::ResetTable(int interface_index) {
  tables_.erase(interface_index);
}

void RoutingTable::SetDefaultMetric(int interface_index, uint32 metric) {
  RoutingTableEntry entry;

  VLOG(2) << __func__;

  if (GetDefaultRoute(interface_index, IPAddress::kAddressFamilyIPv4, &entry) &&
      entry.metric != metric) {
    entry.metric = metric;
    ApplyRoute(interface_index, entry, RTNLMessage::kModeAdd,
               NLM_F_CREATE | NLM_F_REPLACE);
  }

  if (GetDefaultRoute(interface_index, IPAddress::kAddressFamilyIPv6, &entry) &&
      entry.metric != metric) {
    entry.metric = metric;
    ApplyRoute(interface_index, entry, RTNLMessage::kModeAdd,
               NLM_F_CREATE | NLM_F_REPLACE);
  }
}

void RoutingTable::RouteMsgHandler(const RTNLMessage &msg) {
  VLOG(2) << __func__;

  if (msg.type() != RTNLMessage::kTypeRoute ||
      msg.family() == IPAddress::kAddressFamilyUnknown ||
      !msg.HasAttribute(RTA_OIF)) {
    return;
  }

  const RTNLMessage::RouteStatus &route_status = msg.route_status();

  if (route_status.type != RTN_UNICAST ||
      route_status.protocol != RTPROT_BOOT ||
      route_status.table != RT_TABLE_MAIN) {
    return;
  }

  uint32 interface_index = 0;
  if (!msg.GetAttribute(RTA_OIF).ConvertToCPUUInt32(&interface_index)) {
    return;
  }

  uint32 metric = 0;
  if (msg.HasAttribute(RTA_PRIORITY)) {
    msg.GetAttribute(RTA_PRIORITY).ConvertToCPUUInt32(&metric);
  }

  IPAddress default_addr(msg.family());
  default_addr.SetAddressToDefault();

  ByteString dst_bytes(default_addr.address());
  if (msg.HasAttribute(RTA_DST)) {
    dst_bytes = msg.GetAttribute(RTA_DST);
  }
  ByteString src_bytes(default_addr.address());
  if (msg.HasAttribute(RTA_SRC)) {
    src_bytes = msg.GetAttribute(RTA_SRC);
  }
  ByteString gateway_bytes(default_addr.address());
  if (msg.HasAttribute(RTA_GATEWAY)) {
    gateway_bytes = msg.GetAttribute(RTA_GATEWAY);
  }

  RoutingTableEntry entry(
      IPAddress(msg.family(), dst_bytes, route_status.dst_prefix),
      IPAddress(msg.family(), src_bytes, route_status.src_prefix),
      IPAddress(msg.family(), gateway_bytes),
      metric,
      route_status.scope,
      true);

  vector<RoutingTableEntry> &table = tables_[interface_index];
  vector<RoutingTableEntry>::iterator nent;
  for (nent = table.begin(); nent != table.end(); ++nent) {
    if (nent->dst.Equals(entry.dst) &&
        nent->src.Equals(entry.src) &&
        nent->gateway.Equals(entry.gateway) &&
        nent->scope == entry.scope) {
      if (msg.mode() == RTNLMessage::kModeDelete) {
        table.erase(nent);
      } else {
        nent->from_rtnl = true;
        nent->metric = entry.metric;
      }
      return;
    }
  }

  if (msg.mode() == RTNLMessage::kModeAdd) {
    table.push_back(entry);
  }
}

bool RoutingTable::ApplyRoute(uint32 interface_index,
                              const RoutingTableEntry &entry,
                              RTNLMessage::Mode mode,
                              unsigned int flags) {
  VLOG(2) << base::StringPrintf("%s: index %d mode %d flags 0x%x",
                                __func__, interface_index, mode, flags);

  RTNLMessage msg(
      RTNLMessage::kTypeRoute,
      mode,
      NLM_F_REQUEST | flags,
      0,
      0,
      0,
      entry.dst.family());

  msg.set_route_status(RTNLMessage::RouteStatus(
      entry.dst.prefix(),
      entry.src.prefix(),
      RT_TABLE_MAIN,
      RTPROT_BOOT,
      entry.scope,
      RTN_UNICAST,
      0));

  msg.SetAttribute(RTA_DST, entry.dst.address());
  if (!entry.src.IsDefault()) {
    msg.SetAttribute(RTA_SRC, entry.src.address());
  }
  if (!entry.gateway.IsDefault()) {
    msg.SetAttribute(RTA_GATEWAY, entry.gateway.address());
  }
  msg.SetAttribute(RTA_PRIORITY, ByteString::CreateFromCPUUInt32(entry.metric));
  msg.SetAttribute(RTA_OIF, ByteString::CreateFromCPUUInt32(interface_index));

  return RTNLHandler::GetInstance()->SendMessage(&msg);
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

}  // namespace shill
