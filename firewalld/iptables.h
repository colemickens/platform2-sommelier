// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FIREWALLD_IPTABLES_H_
#define FIREWALLD_IPTABLES_H_

#include <stdint.h>

#include <set>
#include <string>
#include <utility>

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

 protected:
  // Test-only.
  explicit IpTables(const std::string& path);

 private:
  friend class IpTablesTest;

  bool PunchHole(uint16_t port,
                 const std::string& interface,
                 std::set<Hole>* holes,
                 enum ProtocolEnum protocol);
  bool PlugHole(uint16_t port,
                const std::string& interface,
                std::set<Hole>* holes,
                enum ProtocolEnum protocol);

  void PlugAllHoles();

  bool AddAllowRule(enum ProtocolEnum protocol,
                    uint16_t port,
                    const std::string& interface);
  bool DeleteAllowRule(enum ProtocolEnum protocol,
                       uint16_t port,
                       const std::string& interface);

  std::string executable_path_;

  // Keep track of firewall holes to avoid adding redundant firewall rules.
  std::set<Hole> tcp_holes_;
  std::set<Hole> udp_holes_;

  DISALLOW_COPY_AND_ASSIGN(IpTables);
};

}  // namespace firewalld

#endif  // FIREWALLD_IPTABLES_H_
