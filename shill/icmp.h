//
// Copyright (C) 2013 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef SHILL_ICMP_H_
#define SHILL_ICMP_H_

#if defined(__ANDROID__)
#include <linux/icmp.h>
#else
#include <netinet/ip_icmp.h>
#endif  // __ANDROID__

#include <memory>

#include <base/macros.h>

#include "shill/net/ip_address.h"

namespace shill {

class IPAddress;
class ScopedSocketCloser;
class Sockets;

// The Icmp class encapsulates the task of sending ICMP frames.
class Icmp {
 public:
  static const int kIcmpEchoCode;

  Icmp();
  virtual ~Icmp();

  // Create a socket for transmission of ICMP frames.
  virtual bool Start(const IPAddress& destination, int interface_index);

  // Destroy the transmit socket.
  virtual void Stop();

  // Returns whether an ICMP socket is open.
  virtual bool IsStarted() const;

  // Send an ICMP Echo Request (Ping) packet to |destination_|. The ID and
  // sequence number fields of the echo request will be set to |id| and
  // |seq_num| respectively.
  virtual bool TransmitEchoRequest(uint16_t id, uint16_t seq_num);

  // IPv4 and IPv6 implementations of TransmitEchoRequest().
  bool TransmitV4EchoRequest(uint16_t id, uint16_t seq_num);
  bool TransmitV6EchoRequest(uint16_t id, uint16_t seq_num);

  int socket() { return socket_; }
  const IPAddress& destination() { return destination_; }
  int interface_index() { return interface_index_; }

 private:
  friend class IcmpSessionTest;
  friend class IcmpTest;

  // Compute the checksum for Echo Request |hdr| of length |len| according to
  // specifications in RFC 792.
  static uint16_t ComputeIcmpChecksum(const struct icmphdr& hdr, size_t len);

  std::unique_ptr<Sockets> sockets_;
  std::unique_ptr<ScopedSocketCloser> socket_closer_;
  int socket_;
  IPAddress destination_;
  int interface_index_;

  DISALLOW_COPY_AND_ASSIGN(Icmp);
};

}  // namespace shill

#endif  // SHILL_ICMP_H_
