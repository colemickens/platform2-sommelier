// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_ICMP_H_
#define SHILL_ICMP_H_

#include <base/memory/scoped_ptr.h>

namespace shill {

class IPAddress;
class ScopedSocketCloser;
class Sockets;

// The Icmp class encapsulates the task of sending ICMP frames.
class Icmp {
 public:
  explicit Icmp();
  virtual ~Icmp();

  // Create a socket for tranmission of ICMP frames.
  virtual bool Start();

  // Destroy the transmit socket.
  virtual void Stop();

  // Returns whether an ICMP socket is open.
  virtual bool IsStarted() const;

  // Send an ICMP Echo Request (Ping) packet to |destination|.
  virtual bool TransmitEchoRequest(const IPAddress &destinaton);

 private:
  friend class IcmpTest;

  scoped_ptr<Sockets> sockets_;
  scoped_ptr<ScopedSocketCloser> socket_closer_;
  int socket_;

  DISALLOW_COPY_AND_ASSIGN(Icmp);
};

}  // namespace shill

#endif  // SHILL_ICMP_H_
