//
// Copyright (C) 2011 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef SHILL_ROUTING_TABLE_ENTRY_H_
#define SHILL_ROUTING_TABLE_ENTRY_H_

#include <linux/rtnetlink.h>

#include <string>

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
