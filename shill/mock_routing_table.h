// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_ROUTING_TABLE_H_
#define SHILL_MOCK_ROUTING_TABLE_H_

#include <gmock/gmock.h>

#include "shill/routing_table.h"

namespace shill {

class MockRoutingTable : public RoutingTable {
 public:
  MOCK_METHOD0(Start, void());
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD2(AddRoute, bool(int interface_index,
                              const RoutingTableEntry &entry));
  MOCK_METHOD3(GetDefaultRoute, bool(int interface_index,
                                     IPAddress::Family family,
                                     RoutingTableEntry *entry));
  MOCK_METHOD3(SetDefaultRoute, bool(int interface_index,
                                     const IPConfigRefPtr &ipconfig,
                                     uint32 metric));
  MOCK_METHOD1(FlushRoutes, void(int interface_index));
  MOCK_METHOD1(ResetTable, void(int interface_index));
  MOCK_METHOD2(SetDefaultMetric, void(int interface_index, uint32 metric));
};

}  // namespace shill

#endif  // SHILL_MOCK_ROUTING_TABLE_H_
