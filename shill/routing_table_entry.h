// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_ROUTING_TABLE_ENTRY_H_
#define SHILL_ROUTING_TABLE_ENTRY_H_

#include <iostream>
#include <string>

#include <base/strings/stringprintf.h>
#include <linux/rtnetlink.h>

#include "shill/net/ip_address.h"

namespace shill {

// Holds table entries for routing.  These are held in an STL vector
// in the RoutingTable object, hence the need for copy contructor and
// operator=.
struct RoutingTableEntry {
 public:
  static const int kDefaultTag = -1;

  RoutingTableEntry()
      : dst(IPAddress::kFamilyUnknown),
        src(IPAddress::kFamilyUnknown),
        gateway(IPAddress::kFamilyUnknown),
        metric(0),
        scope(0),
        from_rtnl(false),
        table(RT_TABLE_MAIN),
        type(RTN_UNICAST),
        tag(kDefaultTag) {}

  RoutingTableEntry(const IPAddress& dst_in,
                    const IPAddress& src_in,
                    const IPAddress& gateway_in,
                    uint32_t metric_in,
                    unsigned char scope_in,
                    bool from_rtnl_in)
      : dst(dst_in),
        src(src_in),
        gateway(gateway_in),
        metric(metric_in),
        scope(scope_in),
        from_rtnl(from_rtnl_in),
        table(RT_TABLE_MAIN),
        type(RTN_UNICAST),
        tag(kDefaultTag) {}

  RoutingTableEntry(const IPAddress& dst_in,
                    const IPAddress& src_in,
                    const IPAddress& gateway_in,
                    uint32_t metric_in,
                    unsigned char scope_in,
                    bool from_rtnl_in,
                    int tag_in)
      : dst(dst_in),
        src(src_in),
        gateway(gateway_in),
        metric(metric_in),
        scope(scope_in),
        from_rtnl(from_rtnl_in),
        table(RT_TABLE_MAIN),
        type(RTN_UNICAST),
        tag(tag_in) {}

  RoutingTableEntry(const IPAddress& dst_in,
                    const IPAddress& src_in,
                    const IPAddress& gateway_in,
                    uint32_t metric_in,
                    unsigned char scope_in,
                    bool from_rtnl_in,
                    unsigned char table_in,
                    unsigned char type_in,
                    int tag_in)
      : dst(dst_in),
        src(src_in),
        gateway(gateway_in),
        metric(metric_in),
        scope(scope_in),
        from_rtnl(from_rtnl_in),
        table(table_in),
        type(type_in),
        tag(tag_in) {}

  RoutingTableEntry(const RoutingTableEntry& b)
      : dst(b.dst),
        src(b.src),
        gateway(b.gateway),
        metric(b.metric),
        scope(b.scope),
        from_rtnl(b.from_rtnl),
        table(b.table),
        type(b.type),
        tag(b.tag) {}

  RoutingTableEntry& operator=(const RoutingTableEntry& b) {
    dst = b.dst;
    src = b.src;
    gateway = b.gateway;
    metric = b.metric;
    scope = b.scope;
    from_rtnl = b.from_rtnl;
    table = b.table;
    type = b.type;
    tag = b.tag;

    return *this;
  }

  ~RoutingTableEntry() {}

  bool Equals(const RoutingTableEntry& b) const {
    return (dst.Equals(b.dst) &&
            src.Equals(b.src) &&
            gateway.Equals(b.gateway) &&
            metric == b.metric &&
            scope == b.scope &&
            from_rtnl == b.from_rtnl &&
            table == b.table &&
            type == b.type &&
            tag == b.tag);
  }

  // Print out an entry in a format similar to that of ip route.
  friend std::ostream& operator<<(std::ostream& os,
                                  const RoutingTableEntry& entry) {
    std::string dest_address =
      entry.dst.IsDefault() ? "default" : entry.dst.ToString();
    const char *dest_prefix;
    switch (entry.type) {
      case RTN_LOCAL:
        dest_prefix = "local ";
        break;
      case RTN_BROADCAST:
        dest_prefix = "broadcast ";
        break;
      case RTN_BLACKHOLE:
        dest_prefix = "blackhole";
        dest_address = "";  // Don't print the address.
        break;
      case RTN_UNREACHABLE:
        dest_prefix = "unreachable";
        dest_address = "";  // Don't print the address.
        break;
      default:
        dest_prefix = "";
        break;
    }
    std::string gateway;
    if (!entry.gateway.IsDefault()) {
      gateway = " via " + entry.gateway.ToString();
    }
    std::string src;
    if (!entry.src.IsDefault()) {
      src = " src " + entry.src.ToString();
    }

    os << base::StringPrintf(
      "%s%s%s metric %d %s table %d%s",
      dest_prefix, dest_address.c_str(), gateway.c_str(), entry.metric,
      IPAddress::GetAddressFamilyName(entry.dst.family()).c_str(),
      static_cast<int>(entry.table), src.c_str());
    return os;
  }

  IPAddress dst;
  IPAddress src;
  IPAddress gateway;
  uint32_t metric;
  unsigned char scope;
  bool from_rtnl;
  unsigned char table;
  unsigned char type;
  int tag;
};

struct RoutingPolicyEntry {
 public:
  RoutingPolicyEntry() {}

  RoutingPolicyEntry(IPAddress::Family family_in,
                     uint32_t priority_in,
                     unsigned char table_in)
      : family(family_in),
        priority(priority_in),
        table(table_in) {}

  RoutingPolicyEntry(IPAddress::Family family_in,
                     uint32_t priority_in,
                     unsigned char table_in,
                     uint32_t uidrange_start_in,
                     uint32_t uidrange_end_in)
      : family(family_in),
        priority(priority_in),
        table(table_in),
        has_uidrange(true),
        uidrange_start(uidrange_start_in),
        uidrange_end(uidrange_end_in) {}

  RoutingPolicyEntry(IPAddress::Family family_in,
                     uint32_t priority_in,
                     unsigned char table_in,
                     const std::string& interface_name_in)
      : family(family_in),
        priority(priority_in),
        table(table_in),
        interface_name(interface_name_in) {}

  RoutingPolicyEntry& operator=(const RoutingPolicyEntry& b) {
    family = b.family;
    priority = b.priority;
    table = b.table;
    invert_rule = b.invert_rule;
    has_fwmark = b.has_fwmark;
    fwmark_value = b.fwmark_value;
    fwmark_mask = b.fwmark_mask;
    has_uidrange = b.has_uidrange;
    uidrange_start = b.uidrange_start;
    uidrange_end = b.uidrange_end;
    interface_name = b.interface_name;
    dst = b.dst;
    src = b.src;

    return *this;
  }

  ~RoutingPolicyEntry() {}

  bool Equals(const RoutingPolicyEntry& b) {
    return (family == b.family &&
            priority == b.priority &&
            table == b.table &&
            invert_rule == b.invert_rule &&
            has_fwmark == b.has_fwmark &&
            fwmark_value == b.fwmark_value &&
            fwmark_mask == b.fwmark_mask &&
            has_uidrange == b.has_uidrange &&
            uidrange_start == b.uidrange_start &&
            uidrange_end == b.uidrange_end &&
            interface_name == b.interface_name &&
            dst.Equals(b.dst) &&
            src.Equals(b.src));
  }

  IPAddress::Family family{IPAddress::kFamilyUnknown};
  uint32_t priority{0};
  unsigned char table{0};

  bool invert_rule{false};

  bool has_fwmark{false};
  uint32_t fwmark_value{0};
  uint32_t fwmark_mask{0xffffffff};

  bool has_uidrange{false};
  uint32_t uidrange_start{0};
  uint32_t uidrange_end{0};

  std::string interface_name;
  IPAddress dst{IPAddress::kFamilyUnknown};
  IPAddress src{IPAddress::kFamilyUnknown};
};

}  // namespace shill

#endif  // SHILL_ROUTING_TABLE_ENTRY_H_
