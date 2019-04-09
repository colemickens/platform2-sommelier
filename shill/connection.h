// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CONNECTION_H_
#define SHILL_CONNECTION_H_

#include <deque>
#include <memory>
#include <string>
#include <vector>

#include <base/memory/ref_counted.h>
#include <base/memory/weak_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/ipconfig.h"
#include "shill/net/ip_address.h"
#include "shill/refptr_types.h"
#include "shill/technology.h"

namespace shill {

class ControlInterface;
class DeviceInfo;
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
    Binder(const std::string& name, const base::Closure& disconnect_callback);
    ~Binder();

    // Binds to |to_connection|. Unbinds the previous bound connection, if
    // any. Pass nullptr to just unbind this Binder.
    void Attach(const ConnectionRefPtr& to_connection);

    const std::string& name() const { return name_; }
    bool IsBound() const { return connection_ != nullptr; }
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

  // The routing metric used for the default service, either physical or VPN.
  static const uint32_t kDefaultMetric;
  // A unique metric used temporarily for the soon-to-be default service during
  // transitions, which ensures that the old default service and the new
  // default service each have a unique metric.
  static const uint32_t kNewDefaultMetric;
  // All other services use a metric that starts at |kNonDefaultMetricBase|
  // and counts up.
  static const uint32_t kNonDefaultMetricBase;

  Connection(int interface_index,
             const std::string& interface_name,
             bool fixed_ip_params,
             Technology::Identifier technology_,
             const DeviceInfo* device_info,
             ControlInterface* control_interface);

  // Add the contents of an IPConfig reference to the list of managed state.
  // This will replace all previous state for this address family.
  virtual void UpdateFromIPConfig(const IPConfigRefPtr& config);

  // Update the metric on the default route in |config|, if any.  This
  // should be called after the kernel notifies shill that a new IPv6
  // address+gateway have been configured.
  virtual void UpdateGatewayMetric(const IPConfigRefPtr& config);


  // Adds |interface_name| to the whitelisted input interfaces that are
  // allowed to use the connection and updates the routing table.
  virtual void AddInputInterfaceToRoutingTable(
      const std::string& interface_name);
  // Removes |interface_name| from the whitelisted input interfaces and
  // updates the routing table.
  virtual void RemoveInputInterfaceFromRoutingTable(
      const std::string& interface_name);

  // The interface metric is a positive integer used by the kernel to
  // determine which interface to use for outbound packets if there are
  // multiple overlapping routes.  The lowest metric wins; the connection
  // with the lowest metric is referred to as the "default connection."

  // Updates the kernel's routing table so that routes associated with
  // this connection will use |metric|, updates the systemwide DNS
  // configuration if necessary, and triggers captive portal detection
  // if the connection has transitioned from non-default to default.
  virtual void SetMetric(uint32_t metric);

  // Returns true if this connection is currently the systemwide default.
  virtual bool IsDefault();

  // Determines whether this connection controls the system DNS settings.
  // This should only be true for one connection at a time.
  virtual void SetUseDNS(bool enable);

  // Update and apply the new DNS servers setting to this connection.
  virtual void UpdateDNSServers(const std::vector<std::string>& dns_servers);

  virtual const std::string& interface_name() const { return interface_name_; }
  virtual int interface_index() const { return interface_index_; }
  virtual const std::vector<std::string>& dns_servers() const {
    return dns_servers_;
  }
  virtual uint8_t table_id() const { return table_id_; }

  virtual const std::string& ipconfig_rpc_identifier() const {
    return ipconfig_rpc_identifier_;
  }

  // Flush and (re)create routing policy rules for the connection.  If
  // |allowed_uids_| or |allowed_iifs_| is set, rules will be created
  // to restrict traffic to the whitelisted UIDs or input interfaces.
  // Otherwise, all system traffic will be allowed to use the connection.
  // The rule priority will be set to |metric_| so that Manager's service
  // sort ranking is respected.
  virtual void UpdateRoutingPolicy();

  // Request to accept traffic routed to this connection even if it is not
  // the default.  This request is ref-counted so the caller must call
  // ReleaseRouting() when they no longer need this facility.
  virtual void RequestRouting();
  virtual void ReleaseRouting();

  // Return the subnet name for this connection.
  virtual std::string GetSubnetName() const;

  virtual const IPAddress& local() const { return local_; }
  virtual const IPAddress& gateway() const { return gateway_; }
  virtual Technology::Identifier technology() const { return technology_; }
  virtual const std::string& tethering() const { return tethering_; }
  void set_tethering(const std::string& tethering) { tethering_ = tethering; }

  // Return true if this is an IPv6 connection.
  virtual bool IsIPv6();

 protected:
  friend class base::RefCounted<Connection>;

  virtual ~Connection();

 private:
  friend class ConnectionTest;
  FRIEND_TEST(ConnectionTest, AddConfig);
  FRIEND_TEST(ConnectionTest, AddConfigUserTrafficOnly);
  FRIEND_TEST(ConnectionTest, Binder);
  FRIEND_TEST(ConnectionTest, Binders);
  FRIEND_TEST(ConnectionTest, BlackholeIPv6);
  FRIEND_TEST(ConnectionTest, Destructor);
  FRIEND_TEST(ConnectionTest, FixGatewayReachability);
  FRIEND_TEST(ConnectionTest, InitState);
  FRIEND_TEST(ConnectionTest, SetMTU);
  FRIEND_TEST(ConnectionTest, UpdateDNSServers);
  FRIEND_TEST(VPNServiceTest, OnConnectionDisconnected);

  // Work around misconfigured servers which provide a gateway address that
  // is unreachable with the provided netmask.
  bool FixGatewayReachability(const IPAddress& local,
                              IPAddress* peer,
                              IPAddress* gateway,
                              const IPAddress& trusted_ip);
  bool SetupExcludedRoutes(const IPConfig::Properties& properties,
                           const IPAddress& gateway,
                           IPAddress* trusted_ip);
  void SetMTU(int32_t mtu);

  void AttachBinder(Binder* binder);
  void DetachBinder(Binder* binder);
  void NotifyBindersOnDisconnect();

  // Send our DNS configuration to the resolver.
  void PushDNSConfig();

  base::WeakPtrFactory<Connection> weak_ptr_factory_;

  bool use_dns_;
  uint32_t metric_;
  bool has_broadcast_domain_;
  int routing_request_count_;
  int interface_index_;
  const std::string interface_name_;
  Technology::Identifier technology_;
  std::vector<std::string> dns_servers_;
  std::vector<std::string> dns_domain_search_;
  std::vector<std::string> excluded_ips_cidr_;
  std::string dns_domain_name_;
  std::string ipconfig_rpc_identifier_;

  // True if this device has its own dedicated routing table.  False if
  // this device uses the global routing table.
  bool per_device_routing_;
  // If |allowed_uids_| and/or |allowed_iifs_| is set, IP policy rules will
  // be created so that only traffic from the whitelisted UIDs and/or
  // input interfaces can use this connection.  If neither is set,
  // all system traffic can use this connection.
  std::vector<uint32_t> allowed_uids_;
  std::vector<std::string> allowed_iifs_;
  std::vector<uint32_t> blackholed_uids_;
  TimeoutSet<IPAddress>* blackholed_addrs_;

  // Do not reconfigure the IP addresses, subnet mask, broadcast, etc.
  bool fixed_ip_params_;
  uint8_t table_id_;
  uint8_t blackhole_table_id_;
  IPAddress local_;
  IPAddress gateway_;

  // Track the tethering status of the Service associated with this connection.
  // This property is set by a service as it takes ownership of a connection,
  // and is read by services that are bound through this connection.
  std::string tethering_;

  // Binders to clients -- usually to related services and devices.
  std::deque<Binder*> binders_;

  // Store cached copies of singletons for speed/ease of testing
  const DeviceInfo* device_info_;
  Resolver* resolver_;
  RoutingTable* routing_table_;
  RTNLHandler* rtnl_handler_;

  ControlInterface* control_interface_;

  DISALLOW_COPY_AND_ASSIGN(Connection);
};

}  // namespace shill

#endif  // SHILL_CONNECTION_H_
