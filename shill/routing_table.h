// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_ROUTING_TABLE_
#define SHILL_ROUTING_TABLE_

#include <deque>
#include <string>
#include <vector>

#include <base/callback.h>
#include <base/hash_tables.h>
#include <base/lazy_instance.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>

#include "shill/ip_address.h"
#include "shill/refptr_types.h"
#include "shill/rtnl_message.h"

namespace shill {

class RTNLHandler;
class RTNLListener;
class RoutingTableEntry;

// This singleton maintains an in-process copy of the routing table on
// a per-interface basis.  It offers the ability for other modules to
// make modifications to the routing table, centered around setting the
// default route for an interface or modifying its metric (priority).
class RoutingTable {
 public:
  struct Query {
    // Callback::Run(interface_index, entry)
    typedef base::Callback<void(int, const RoutingTableEntry &)> Callback;

    Query() : sequence(0), tag(0) {}
    Query(uint32 sequence_in,
          int tag_in,
          Callback callback_in)
        : sequence(sequence_in),
          tag(tag_in),
          callback(callback_in) {}

    uint32 sequence;
    int tag;
    Callback callback;
  };

  virtual ~RoutingTable();

  static RoutingTable *GetInstance();

  virtual void Start();
  virtual void Stop();

  // Add an entry to the routing table.
  virtual bool AddRoute(int interface_index, const RoutingTableEntry &entry);

  // Get the default route associated with an interface of a given addr family.
  // The route is copied into |*entry|.
  virtual bool GetDefaultRoute(int interface_index,
                               IPAddress::Family family,
                               RoutingTableEntry *entry);

  // Set the default route for an interface with index |interface_index|,
  // given the IPAddress of the gateway |gateway_address| and priority
  // |metric|.
  virtual bool SetDefaultRoute(int interface_index,
                               const IPAddress &gateway_address,
                               uint32 metric);

  // Configure routing table entries from the "routes" portion of |ipconifg|.
  // Returns true if all routes were installed successfully, false otherwise.
  virtual bool ConfigureRoutes(int interface_index,
                               const IPConfigRefPtr &ipconfig,
                               uint32 metric);

  // Remove routes associated with interface.
  // Route entries are immediately purged from our copy of the routing table.
  virtual void FlushRoutes(int interface_index);

  // Iterate over all routing tables removing routes tagged with |tag|.
  // Route entries are immediately purged from our copy of the routing table.
  virtual void FlushRoutesWithTag(int tag);

  // Flush the routing cache for all interfaces.
  virtual bool FlushCache();

  // Reset local state for this interface.
  virtual void ResetTable(int interface_index);

  // Set the metric (priority) on existing default routes for an interface.
  virtual void SetDefaultMetric(int interface_index, uint32 metric);

  // Get the default route to |destination| through |interface_index| and create
  // a host route to that destination.  When creating the route, tag our local
  // entry with |tag|, so we can remove it later.  Connections use their
  // interface index as the tag, so that as they are destroyed, they can remove
  // all their dependent routes.  If |callback| is not null, it will be invoked
  // when the request-route response is received and the add-route request has
  // been sent successfully.
  virtual bool RequestRouteToHost(const IPAddress &destination,
                                  int interface_index,
                                  int tag,
                                  const Query::Callback &callback);

  // Cancels all outstanding query |callback| instances.
  void CancelQueryCallback(const Query::Callback &callback);

 protected:
  RoutingTable();

 private:
  friend struct base::DefaultLazyInstanceTraits<RoutingTable>;
  friend class RoutingTableTest;

  static bool ParseRoutingTableMessage(const RTNLMessage &message,
                                       int *interface_index,
                                       RoutingTableEntry *entry);
  void RouteMsgHandler(const RTNLMessage &msg);
  bool ApplyRoute(uint32 interface_index,
                  const RoutingTableEntry &entry,
                  RTNLMessage::Mode mode,
                  unsigned int flags);
  // Get the default route associated with an interface of a given addr family.
  // A pointer to the route is placed in |*entry|.
  virtual bool GetDefaultRouteInternal(int interface_index,
                               IPAddress::Family family,
                               RoutingTableEntry **entry);

  void ReplaceMetric(uint32 interface_index,
                     RoutingTableEntry *entry,
                     uint32 metric);

  static const char kRouteFlushPath4[];
  static const char kRouteFlushPath6[];

  base::hash_map<int, std::vector<RoutingTableEntry> > tables_; // NOLINT
  // NOLINT above: hash_map from base, no need to #include <hash_map>.

  base::Callback<void(const RTNLMessage &)> route_callback_;
  scoped_ptr<RTNLListener> route_listener_;
  std::deque<Query> route_queries_;

  // Cache singleton pointer for performance and test purposes.
  RTNLHandler *rtnl_handler_;

  DISALLOW_COPY_AND_ASSIGN(RoutingTable);
};

}  // namespace shill

#endif  // SHILL_ROUTING_TABLE_
