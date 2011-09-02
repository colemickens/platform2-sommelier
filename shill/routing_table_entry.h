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
      : dst(IPAddress::kFamilyUnknown),
        src(IPAddress::kFamilyUnknown),
        gateway(IPAddress::kFamilyUnknown),
        metric(0),
        scope(0),
        from_rtnl(false) {}

  RoutingTableEntry(const IPAddress &dst_in,
                    const IPAddress &src_in,
                    const IPAddress &gateway_in,
                    uint32 metric_in,
                    unsigned char scope_in,
                    bool from_rtnl_in)
      : dst(dst_in),
        src(src_in),
        gateway(gateway_in),
        metric(metric_in),
        scope(scope_in),
        from_rtnl(from_rtnl_in) {}

  RoutingTableEntry(const RoutingTableEntry &b)
      : dst(b.dst),
        src(b.src),
        gateway(b.gateway),
        metric(b.metric),
        scope(b.scope),
        from_rtnl(b.from_rtnl) {}

  RoutingTableEntry &operator=(const RoutingTableEntry &b) {
    dst = b.dst;
    src = b.src;
    gateway = b.gateway;
    metric = b.metric;
    scope = b.scope;
    from_rtnl = b.from_rtnl;

    return *this;
  }

  ~RoutingTableEntry() {}

  bool Equals(const RoutingTableEntry &b) {
    return (dst.Equals(b.dst) &&
            src.Equals(b.src) &&
            gateway.Equals(b.gateway) &&
            metric == b.metric &&
            scope == b.scope &&
            from_rtnl == b.from_rtnl);
  }

  IPAddress dst;
  IPAddress src;
  IPAddress gateway;
  uint32 metric;
  unsigned char scope;
  bool from_rtnl;
};

}  // namespace shill


#endif  // SHILL_ROUTING_TABLE_ENTRY_
