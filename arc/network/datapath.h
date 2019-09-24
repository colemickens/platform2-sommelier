// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_NETWORK_DATAPATH_H_
#define ARC_NETWORK_DATAPATH_H_

#include <string>

#include <base/macros.h>

#include "arc/network/minijailed_process_runner.h"

namespace arc_networkd {

// Returns for given interface name the host name of a ARC veth pair.
std::string ArcVethHostName(std::string ifname);

// Returns for given interface name the peer name of a ARC veth pair.
std::string ArcVethPeerName(std::string ifname);

// ARC networking data path configuration utility.
// IPV4 addresses are always specified in singular dotted-form (a.b.c.d)
// (not in CIDR representation
class Datapath {
 public:
  // |process_runner| must not be null; it is not owned.
  explicit Datapath(MinijailedProcessRunner* process_runner);
  virtual ~Datapath() = default;

  virtual bool AddBridge(const std::string& ifname,
                         const std::string& ipv4_addr);
  virtual void RemoveBridge(const std::string& ifname);

  // Creates a virtual interface and adds one side to bridge |br_ifname} and
  // returns the name of the other side.
  virtual std::string AddVirtualBridgedInterface(const std::string& ifname,
                                                 const std::string& mac_addr,
                                                 const std::string& br_ifname);
  virtual void RemoveInterface(const std::string& ifname);

  // Inject an interace into the ARC++ container namespace.
  virtual bool AddInterfaceToContainer(int ns,
                                       const std::string& src_ifname,
                                       const std::string& dst_ifname,
                                       const std::string& dst_ipv4,
                                       bool fwd_multicast);

  // Create (or flush and delete) pre-routing rules supporting legacy (ARC N)
  // single networking DNAT configuration.
  virtual bool AddLegacyIPv4DNAT(const std::string& ipv4_addr);
  virtual void RemoveLegacyIPv4DNAT();

  // Enable ingress traffic from a specific physical device to the legacy
  // single networkng DNAT configuration.
  virtual bool AddLegacyIPv4InboundDNAT(const std::string& ifname);
  virtual void RemoveLegacyIPv4InboundDNAT();

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
