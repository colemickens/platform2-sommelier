// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_ROUTING_TABLE_
#define SHILL_ROUTING_TABLE_

#include <string>

#include <base/callback_old.h>
#include <base/hash_tables.h>
#include <base/lazy_instance.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>

#include "shill/ip_address.h"
#include "shill/refptr_types.h"
#include "shill/rtnl_message.h"

namespace shill {

class RoutingTableEntry;
class RTNLListener;

// This singleton maintains an in-process copy of the routing table on
// a per-interface basis.  It offers the ability for other modules to
// make modifications to the routing table, centered around setting the
// default route for an interface or modifying its metric (priority).
class RoutingTable {
 public:
  virtual ~RoutingTable();

  static RoutingTable *GetInstance();

  virtual void Start();
  virtual void Stop();

  // Add an entry to the routing table
  virtual bool AddRoute(int interface_index, const RoutingTableEntry &entry);

  // Get the default route associated with an interface of a given addr family
  virtual bool GetDefaultRoute(int interface_index,
                               IPAddress::Family family,
                               RoutingTableEntry *entry);

  // Set the default route for an interface, given an ipconfig entry
  virtual bool SetDefaultRoute(int interface_index,
                               const IPConfigRefPtr &ipconfig,
                               uint32 metric);

  // Remove all routes associated with interface
  virtual void FlushRoutes(int interface_index);

  // Reset local state for this interface
  virtual void ResetTable(int interface_index);

  // Set the metric (priority) on existing default routes for an interface
  virtual void SetDefaultMetric(int interface_index, uint32 metric);

 protected:
  RoutingTable();

 private:
  friend struct base::DefaultLazyInstanceTraits<RoutingTable>;
  friend class RoutingTableTest;

  void RouteMsgHandler(const RTNLMessage &msg);
  bool ApplyRoute(uint32 interface_index,
                  const RoutingTableEntry &entry,
                  RTNLMessage::MessageMode mode,
                  unsigned int flags);
  bool FlushCache();

  static const char kRouteFlushPath4[];
  static const char kRouteFlushPath6[];

  base::hash_map<int, std::vector<RoutingTableEntry> > tables_;
  scoped_ptr<Callback1<const RTNLMessage &>::Type> route_callback_;
  scoped_ptr<RTNLListener> route_listener_;

  DISALLOW_COPY_AND_ASSIGN(RoutingTable);
};

}  // namespace shill

#endif  // SHILL_ROUTING_TABLE_
