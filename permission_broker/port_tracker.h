// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PERMISSION_BROKER_PORT_TRACKER_H_
#define PERMISSION_BROKER_PORT_TRACKER_H_

#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <base/macros.h>
#include <base/message_loop/message_loop.h>
#include <base/sequenced_task_runner.h>

#include "permission_broker/firewall.h"

namespace permission_broker {

class PortTracker {
 public:
  struct PortRuleKey {
    ProtocolEnum proto;
    uint16_t input_dst_port;
    std::string input_ifname;

    bool operator==(const PortRuleKey& other) const {
      return proto == other.proto && input_dst_port == other.input_dst_port &&
             input_ifname == other.input_ifname;
    }
  };

  struct PortRuleKeyHasher {
    std::size_t operator()(const PortRuleKey& k) const {
      return ((std::hash<int>()(k.proto) ^
               (std::hash<uint16_t>()(k.input_dst_port) << 1)) >>
              1) ^
             (std::hash<std::string>()(k.input_ifname) << 1);
    }
  };

  struct PortRule {
    int lifeline_fd;
    ProtocolEnum proto;
    uint16_t input_dst_port;
    std::string input_ifname;
    std::string dst_ip;
    uint16_t dst_port;
  };

  typedef std::pair<uint16_t, std::string> Hole;

  explicit PortTracker(Firewall* firewall);
  virtual ~PortTracker();

  bool AllowTcpPortAccess(uint16_t port, const std::string& iface, int dbus_fd);
  bool AllowUdpPortAccess(uint16_t port, const std::string& iface, int dbus_fd);
  bool RevokeTcpPortAccess(uint16_t port, const std::string& iface);
  bool RevokeUdpPortAccess(uint16_t port, const std::string& iface);
  bool LockDownLoopbackTcpPort(uint16_t port, int dbus_fd);
  bool ReleaseLoopbackTcpPort(uint16_t port);
  bool StartTcpPortForwarding(uint16_t input_dst_port,
                              const std::string& input_ifname,
                              const std::string& dst_ip,
                              uint16_t dst_port,
                              int dbus_fd);
  bool StartUdpPortForwarding(uint16_t input_dst_port,
                              const std::string& input_ifname,
                              const std::string& dst_ip,
                              uint16_t dst_port,
                              int dbus_fd);
  bool StopTcpPortForwarding(uint16_t input_dst_port,
                             const std::string& input_ifname);
  bool StopUdpPortForwarding(uint16_t input_dst_port,
                             const std::string& input_ifname);
  bool HasActiveRules();

  // Close all outstanding firewall holes.
  void RevokeAllPortAccess();
  // Revoke all outstanding forwarding rules.
  void RevokeAllForwardingRules();

  // Unblock all loopback ports.
  void UnblockLoopbackPorts();


 protected:
  PortTracker(scoped_refptr<base::SequencedTaskRunner> task_runner,
              Firewall* firewall);

 private:
  // Helper functions for process lifetime tracking.
  virtual int AddLifelineFd(int dbus_fd);
  virtual bool DeleteLifelineFd(int fd);
  virtual void CheckLifelineFds(bool reschedule_check);
  virtual void ScheduleLifelineCheck();

  bool PlugFirewallHole(int fd);
  bool OpenPort(const PortRuleKey& key, int dbus_fd);
  bool ClosePort(const PortRuleKey& key);
  bool AddForwardingRule(const PortRule& rule, int dbus_fd);
  bool RemoveForwardingRule(const PortRuleKey& key);

  // epoll(7) helper functions.
  virtual bool InitializeEpollOnce();

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  int epfd_;

  // For each fd (process), keep track of which hole (protocol, port, interface)
  // it requested.
  std::map<int, PortRuleKey> open_port_rules_;

  // For each hole (protocol, port, interface), keep track of which fd
  // requested it.  We need this for Release{Tcp|Udp}Port(), to avoid
  // traversing |open_port_rules_| each time.
  std::unordered_map<PortRuleKey, int, PortRuleKeyHasher> open_port_fds_;

  // For each fd (process), keep track of which loopback port it requested.
  std::map<int, uint16_t> tcp_loopback_ports_;

  // For each loopback port, keep track of which fd requested it.
  // We need this for ReleaseLoopbackTcpPort() to avoid traversing
  // |tcp_loopback_ports_| each time.
  std::map<uint16_t, int> tcp_loopback_fds_;

  // Keeps track of each forwarding rule (protocol, port, interface) and which
  // process requested it. The bidirectional maps avoid any traversal for
  // removal or for checking duplicates during insertion.
  std::unordered_map<PortRuleKey, PortRule, PortRuleKeyHasher>
      forwarding_rules_fds_;
  std::map<int, PortRuleKey> forwarding_rules_;

  // |firewall_| is owned by the PermissionBroker object owning this instance
  // of PortTracker.
  Firewall* firewall_;

  DISALLOW_COPY_AND_ASSIGN(PortTracker);
};

}  // namespace permission_broker

#endif  // PERMISSION_BROKER_PORT_TRACKER_H_
