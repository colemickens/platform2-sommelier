// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_ARP_CLIENT_H_
#define SHILL_ARP_CLIENT_H_

#include <base/basictypes.h>
#include <base/memory/scoped_ptr.h>

namespace shill {

class ArpPacket;
class ByteString;
class Sockets;
class ScopedSocketCloser;

// ArpClient task of creating ARP-capable sockets, as well as
// transmitting requests on and receiving responses from such
// sockets.
class ArpClient {
 public:
  explicit ArpClient(int interface_index);
  virtual ~ArpClient();

  // Create a socket for tranmission and reception.  Returns true
  // if successful, false otherwise.
  virtual bool Start();

  // Destroy the client socket.
  virtual void Stop();

  // Receive an ARP reply and parse its contents into |packet|.  Also
  // return the sender's MAC address (which may be different from the
  // MAC address in the ARP response) in |sender|.  Returns true on
  // succes, false otherwise.
  virtual bool ReceiveReply(ArpPacket *packet, ByteString *sender) const;

  // Send a formatted ARP request from |packet|.  Returns true on
  // success, false otherwise.
  virtual bool TransmitRequest(const ArpPacket &packet) const;

  virtual int socket() const { return socket_; }

 private:
  friend class ArpClientTest;

  // Offset of the ARP OpCode within a captured ARP packet.
  static const size_t kArpOpOffset;

  // The largest packet we expect to receive as an ARP client.
  static const size_t kMaxArpPacketLength;

  bool CreateSocket();

  const int interface_index_;
  scoped_ptr<Sockets> sockets_;
  scoped_ptr<ScopedSocketCloser> socket_closer_;
  int socket_;

  DISALLOW_COPY_AND_ASSIGN(ArpClient);
};

}  // namespace shill

#endif  // SHILL_ARP_CLIENT_H_
