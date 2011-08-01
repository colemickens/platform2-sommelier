// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_ROUTING_TABLE_ENTRY_
#define SHILL_ROUTING_TABLE_ENTRY_

#include <base/basictypes.h>

#include "shill/ip_address.h"

namespace shill {

// Holds table entries for routing.  These are held in an STL vector
// in the RoutingTable object, hence the need for copy contructor and
// operator=.
struct RoutingTableEntry {
 public:
  RoutingTableEntry()
      : dst(IPAddress::kAddressFamilyUnknown),
        dst_prefix(0),
        src(IPAddress::kAddressFamilyUnknown),
        src_prefix(0),
        gateway(IPAddress::kAddressFamilyUnknown),
        metric(0),
        scope(0),
        from_rtnl(false) {}

  RoutingTableEntry(const IPAddress &dst_in,
                    unsigned char dst_prefix_in,
                    const IPAddress &src_in,
                    unsigned char src_prefix_in,
                    const IPAddress &gateway_in,
                    uint32 metric_in,
                    unsigned char scope_in,
                    bool from_rtnl_in)
      : dst(dst_in.family(), dst_in.address()),
        dst_prefix(dst_prefix_in),
        src(src_in.family(), src_in.address()),
        src_prefix(src_prefix_in),
        gateway(gateway_in.family(), gateway_in.address()),
        metric(metric_in),
        scope(scope_in),
        from_rtnl(from_rtnl_in) {}

  RoutingTableEntry(const RoutingTableEntry &b)
      : dst(b.dst.family(), b.dst.address()),
        dst_prefix(b.dst_prefix),
        src(b.src.family(), b.src.address()),
        src_prefix(b.src_prefix),
        gateway(b.gateway.family(), b.gateway.address()),
        metric(b.metric),
        scope(b.scope),
        from_rtnl(b.from_rtnl) {}

  RoutingTableEntry &operator=(const RoutingTableEntry &b) {
    dst.Clone(b.dst);
    dst_prefix = b.dst_prefix;
    src.Clone(b.src);
    src_prefix = b.src_prefix;
    gateway.Clone(b.gateway);
    metric = b.metric;
    scope = b.scope;
    from_rtnl = b.from_rtnl;

    return *this;
  }

  ~RoutingTableEntry() {}

  bool Equals(const RoutingTableEntry &b) {
    return (dst.Equals(b.dst) &&
            dst_prefix == b.dst_prefix &&
            src.Equals(b.src) &&
            src_prefix == b.src_prefix &&
            gateway.Equals(b.gateway) &&
            metric == b.metric &&
            scope == b.scope &&
            from_rtnl == b.from_rtnl);
  }

  IPAddress dst;
  unsigned char dst_prefix;
  IPAddress src;
  unsigned char src_prefix;
  IPAddress gateway;
  uint32 metric;
  unsigned char scope;
  bool from_rtnl;
};

}  // namespace shill


#endif  // SHILL_ROUTING_TABLE_ENTRY_
