// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_ROUTING_TABLE_ENTRY_H_
#define SHILL_ROUTING_TABLE_ENTRY_H_

#include <iostream>
#include <string>

#include "shill/net/ip_address.h"

namespace shill {

// Represents a single entry in a routing table.
struct RoutingTableEntry {
  static const int kDefaultTag;

  RoutingTableEntry();

  static RoutingTableEntry Create(IPAddress::Family family);
  static RoutingTableEntry Create(const IPAddress& dst_in,
                                  const IPAddress& src_in,
                                  const IPAddress& gateway_in);

  RoutingTableEntry& SetMetric(uint32_t metric_in);
  RoutingTableEntry& SetScope(unsigned char scope_in);
  RoutingTableEntry& SetTable(uint8_t table_in);
  RoutingTableEntry& SetType(unsigned char type_in);
  RoutingTableEntry& SetTag(uint8_t tag_in);

  bool operator==(const RoutingTableEntry& b) const;

  IPAddress dst;
  IPAddress src;
  IPAddress gateway;
  uint32_t metric;
  unsigned char scope;
  bool from_rtnl;
  uint8_t table;
  unsigned char type;
  unsigned char protocol;
  int tag;
};

// Print out an entry in a format similar to that of ip route.
std::ostream& operator<<(std::ostream& os, const RoutingTableEntry& entry);

// Represents a single policy routing rule.
struct RoutingPolicyEntry {
  RoutingPolicyEntry() = default;

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

  bool operator==(const RoutingPolicyEntry& b) const {
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
            dst == b.dst &&
            src == b.src);
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
