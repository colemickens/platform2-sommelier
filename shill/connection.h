// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CONNECTION_
#define SHILL_CONNECTION_

#include <string>
#include <vector>

#include <base/memory/ref_counted.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/refptr_types.h"
#include "shill/technology.h"

namespace shill {

class DeviceInfo;
class IPAddress;
class Resolver;
class RoutingTable;
class RTNLHandler;

// The Conneciton maintains the implemented state of an IPConfig, e.g,
// the IP address, routing table and DNS table entries.
class Connection : public base::RefCounted<Connection> {
 public:
  Connection(int interface_index,
             const std::string &interface_name,
             Technology::Identifier technology_,
             const DeviceInfo *device_info);
  virtual ~Connection();

  // Add the contents of an IPConfig reference to the list of managed state.
  // This will replace all previous state for this address family.
  virtual void UpdateFromIPConfig(const IPConfigRefPtr &config);

  // Sets the current connection as "default", i.e., routes and DNS entries
  // should be used by all system components that don't select explicitly.
  virtual bool is_default() const { return is_default_; }
  virtual void SetIsDefault(bool is_default);

  virtual const std::string &interface_name() const { return interface_name_; }
  virtual const std::vector<std::string> &dns_servers() const {
    return dns_servers_;
  }

  // Request to accept traffic routed to this connection even if it is not
  // the default.  This request is ref-counted so the caller must call
  // ReleaseRouting() when they no longer need this facility.
  virtual void RequestRouting();
  virtual void ReleaseRouting();

  // Request a host route through this connection.
  virtual bool RequestHostRoute(const IPAddress &destination);

 private:
  friend class ConnectionTest;
  FRIEND_TEST(ConnectionTest, AddConfig);
  FRIEND_TEST(ConnectionTest, AddConfigReverse);
  FRIEND_TEST(ConnectionTest, AddConfigWithBrokenNetmask);
  FRIEND_TEST(ConnectionTest, AddConfigWithPeer);
  FRIEND_TEST(ConnectionTest, Destructor);
  FRIEND_TEST(ConnectionTest, InitState);

  static const uint32 kDefaultMetric;
  static const uint32 kNonDefaultMetricBase;

  // Work around misconfigured servers which provide a gateway address that
  // is unreachable with the provided netmask.
  static void FixGatewayReachability(IPAddress *local,
                                     const IPAddress &gateway);
  uint32 GetMetric(bool is_default);

  bool is_default_;
  int routing_request_count_;
  int interface_index_;
  const std::string interface_name_;
  Technology::Identifier technology_;
  std::vector<std::string> dns_servers_;
  std::vector<std::string> dns_domain_search_;

  // Store cached copies of singletons for speed/ease of testing
  const DeviceInfo *device_info_;
  Resolver *resolver_;
  RoutingTable *routing_table_;
  RTNLHandler *rtnl_handler_;

  DISALLOW_COPY_AND_ASSIGN(Connection);
};

}  // namespace shill

#endif  // SHILL_CONNECTION_
