// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CONNECTION_
#define SHILL_CONNECTION_

#include <deque>
#include <string>
#include <vector>

#include <base/memory/ref_counted.h>
#include <base/memory/weak_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/ipconfig.h"
#include "shill/refptr_types.h"
#include "shill/technology.h"

namespace shill {

class DeviceInfo;
class IPAddress;
class RTNLHandler;
class Resolver;
class RoutingTable;
struct RoutingTableEntry;

// The Conneciton maintains the implemented state of an IPConfig, e.g,
// the IP address, routing table and DNS table entries.
class Connection : public base::RefCounted<Connection> {
 public:
  // Clients can instantiate and use Binder to bind to a Connection and get
  // notified when the bound Connection disconnects. Note that the client's
  // disconnect callback will be executed at most once, and only if the bound
  // Connection is destroyed or signals disconnect. The Binder unbinds itself
  // from the underlying Connection when the Binder instance is destructed.
  class Binder {
   public:
    Binder(const std::string &name, const base::Closure &disconnect_callback);
    ~Binder();

    // Binds to |to_connection|. Unbinds the previous bound connection, if
    // any. Pass NULL to just unbind this Binder.
    void Attach(const ConnectionRefPtr &to_connection);

    bool IsBound() const { return connection_ != NULL; }
    ConnectionRefPtr connection() const { return connection_.get(); }

   private:
    friend class Connection;
    FRIEND_TEST(ConnectionTest, Binder);

    // Invoked by |connection_|.
    void OnDisconnect();

    const std::string name_;
    base::WeakPtr<Connection> connection_;
    const base::Closure client_disconnect_callback_;

    DISALLOW_COPY_AND_ASSIGN(Binder);
  };

  Connection(int interface_index,
             const std::string &interface_name,
             Technology::Identifier technology_,
             const DeviceInfo *device_info);

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

  virtual const std::string &ipconfig_rpc_identifier() const {
    return ipconfig_rpc_identifier_;
  }

  // Request to accept traffic routed to this connection even if it is not
  // the default.  This request is ref-counted so the caller must call
  // ReleaseRouting() when they no longer need this facility.
  virtual void RequestRouting();
  virtual void ReleaseRouting();

  // Request a host route through this connection.
  virtual bool RequestHostRoute(const IPAddress &destination);

 protected:
  friend class base::RefCounted<Connection>;

  virtual ~Connection();

 private:
  friend class ConnectionTest;
  FRIEND_TEST(ConnectionTest, AddConfig);
  FRIEND_TEST(ConnectionTest, AddConfigReverse);
  FRIEND_TEST(ConnectionTest, AddConfigWithBrokenNetmask);
  FRIEND_TEST(ConnectionTest, AddConfigWithPeer);
  FRIEND_TEST(ConnectionTest, Binder);
  FRIEND_TEST(ConnectionTest, Binders);
  FRIEND_TEST(ConnectionTest, Destructor);
  FRIEND_TEST(ConnectionTest, FixGatewayReachability);
  FRIEND_TEST(ConnectionTest, InitState);
  FRIEND_TEST(ConnectionTest, OnRouteQueryResponse);
  FRIEND_TEST(ConnectionTest, RequestHostRoute);

  static const uint32 kDefaultMetric;
  static const uint32 kNonDefaultMetricBase;

  // Work around misconfigured servers which provide a gateway address that
  // is unreachable with the provided netmask.
  static bool FixGatewayReachability(IPAddress *local,
                                     const IPAddress &gateway,
                                     const IPAddress &peer);
  uint32 GetMetric(bool is_default);
  bool PinHostRoute(const IPConfig::Properties &config);

  void OnRouteQueryResponse(int interface_index,
                            const RoutingTableEntry &entry);

  void AttachBinder(Binder *binder);
  void DetachBinder(Binder *binder);
  void NotifyBindersOnDisconnect();

  void OnLowerDisconnect();

  base::WeakPtrFactory<Connection> weak_ptr_factory_;

  bool is_default_;
  int routing_request_count_;
  int interface_index_;
  const std::string interface_name_;
  Technology::Identifier technology_;
  std::vector<std::string> dns_servers_;
  std::vector<std::string> dns_domain_search_;
  std::string ipconfig_rpc_identifier_;

  // A binder to a lower Connection that this Connection depends on, if any.
  Binder lower_binder_;

  // Binders to clients -- usually to upper connections or related services and
  // devices.
  std::deque<Binder *> binders_;

  // Store cached copies of singletons for speed/ease of testing
  const DeviceInfo *device_info_;
  Resolver *resolver_;
  RoutingTable *routing_table_;
  RTNLHandler *rtnl_handler_;

  DISALLOW_COPY_AND_ASSIGN(Connection);
};

}  // namespace shill

#endif  // SHILL_CONNECTION_
