// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PERMISSION_BROKER_FIREWALL_H_
#define PERMISSION_BROKER_FIREWALL_H_

#include <stdint.h>

#include <set>
#include <string>
#include <utility>
#include <vector>

#include <base/macros.h>
#include <brillo/errors/error.h>
#include <gtest/gtest_prod.h>

namespace permission_broker {

extern const char kIpTablesPath[];
extern const char kIp6TablesPath[];

enum ProtocolEnum { kProtocolTcp, kProtocolUdp };

class Firewall {
 public:
  typedef std::pair<uint16_t, std::string> Hole;

  Firewall() = default;
  ~Firewall() = default;

  bool AddAcceptRules(ProtocolEnum protocol,
                      uint16_t port,
                      const std::string& interface);
  bool DeleteAcceptRules(ProtocolEnum protocol,
                         uint16_t port,
                         const std::string& interface);

 private:
  friend class FirewallTest;
  virtual bool AddAcceptRule(const std::string& executable_path,
                             ProtocolEnum protocol,
                             uint16_t port,
                             const std::string& interface);
  virtual bool DeleteAcceptRule(const std::string& executable_path,
                                ProtocolEnum protocol,
                                uint16_t port,
                                const std::string& interface);

  // Even though permission_broker runs as a regular user, it can still add
  // other restrictions when launching 'iptables'.
  virtual int RunInMinijail(const std::vector<std::string>& argv);

  DISALLOW_COPY_AND_ASSIGN(Firewall);
};

}  // namespace permission_broker

#endif  // PERMISSION_BROKER_FIREWALL_H_
