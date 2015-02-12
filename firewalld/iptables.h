// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FIREWALLD_IPTABLES_H_
#define FIREWALLD_IPTABLES_H_

#include <stdint.h>

#include <set>
#include <string>
#include <utility>
#include <vector>

#include <base/macros.h>
#include <chromeos/errors/error.h>

#include "firewalld/dbus_adaptor/org.chromium.Firewalld.h"

namespace firewalld {

enum ProtocolEnum { kProtocolTcp, kProtocolUdp };

class IpTables : public org::chromium::FirewalldInterface {
 public:
  typedef std::pair<uint16_t, std::string> Hole;

  IpTables();
  ~IpTables();

  // D-Bus methods.
  bool PunchTcpHole(uint16_t in_port, const std::string& in_interface) override;
  bool PunchUdpHole(uint16_t in_port, const std::string& in_interface) override;
  bool PlugTcpHole(uint16_t in_port, const std::string& in_interface) override;
  bool PlugUdpHole(uint16_t in_port, const std::string& in_interface) override;

  bool RequestVpnSetup(const std::vector<std::string>& usernames,
                       const std::string& interface) override;
  bool RemoveVpnSetup(const std::vector<std::string>& usernames,
                      const std::string& interface) override;

 protected:
  // Test-only.
  explicit IpTables(const std::string& ip4_path, const std::string& ip6_path);

 private:
  friend class IpTablesTest;

  bool PunchHole(uint16_t port,
                 const std::string& interface,
                 std::set<Hole>* holes,
                 ProtocolEnum protocol);
  bool PlugHole(uint16_t port,
                const std::string& interface,
                std::set<Hole>* holes,
                ProtocolEnum protocol);

  void PlugAllHoles();

  bool AddAcceptRules(ProtocolEnum protocol,
                      uint16_t port,
                      const std::string& interface);
  bool DeleteAcceptRules(ProtocolEnum protocol,
                         uint16_t port,
                         const std::string& interface);
  bool AddAcceptRule(const std::string& executable_path,
                     ProtocolEnum protocol,
                     uint16_t port,
                     const std::string& interface);
  bool DeleteAcceptRule(const std::string& executable_path,
                        ProtocolEnum protocol,
                        uint16_t port,
                        const std::string& interface);

  bool ApplyVpnSetup(const std::vector<std::string>& usernames,
                     const std::string& interface,
                     bool add);

  bool ApplyMasquerade(const std::string& interface,
                       bool add);

  bool ApplyMarkForUserTraffic(const std::string& user_name,
                               bool add);

  bool ApplyRuleForUserTraffic(bool add);

  std::string ip4_exec_path_;
  std::string ip6_exec_path_;

  // Keep track of firewall holes to avoid adding redundant firewall rules.
  std::set<Hole> tcp_holes_;
  std::set<Hole> udp_holes_;

  DISALLOW_COPY_AND_ASSIGN(IpTables);
};

}  // namespace firewalld

#endif  // FIREWALLD_IPTABLES_H_
