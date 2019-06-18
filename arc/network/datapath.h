// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_NETWORK_DATAPATH_H_
#define ARC_NETWORK_DATAPATH_H_

#include <string>

#include <base/macros.h>

#include "arc/network/minijailed_process_runner.h"

namespace arc_networkd {

// ARC networking data path configuration utility.
class Datapath {
 public:
  // |process_runner| must not be null; it is not owned.
  explicit Datapath(MinijailedProcessRunner* process_runner);
  virtual ~Datapath() = default;

  // The following are iptables methods.
  // When specified, |ipv4_addr| is always singlar dotted-form (a.b.c.d)
  // IPv4 address (not a CIDR representation).

  // Create (or flush and delete) pre-routing rules supporting legacy (ARC N)
  // single networking DNAT configuration.
  virtual bool AddLegacyIPv4DNAT(const std::string& ipv4_addr);
  virtual void RemoveLegacyIPv4DNAT();

  // Create (or delete) pre-routing rules allowing direct ingress on |ifname|
  // to guest desintation |ipv4_addr|.
  virtual bool AddInboundIPv4DNAT(const std::string& ifname,
                                  const std::string& ipv4_addr);
  virtual void RemoveInboundIPv4DNAT(const std::string& ifname,
                                     const std::string& ipv4_addr);

  // Create (or delete) a forwarding rule for |ifname|.
  virtual bool AddOutboundIPv4(const std::string& ifname);
  virtual void RemoveOutboundIPv4(const std::string& ifname);

 private:
  MinijailedProcessRunner* process_runner_;

  DISALLOW_COPY_AND_ASSIGN(Datapath);
};

}  // namespace arc_networkd

#endif  // ARC_NETWORK_DATAPATH_H_
