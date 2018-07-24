// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_ARP_CLIENT_H_
#define SHILL_MOCK_ARP_CLIENT_H_

#include "shill/arp_client.h"

#include <gmock/gmock.h>

#include "shill/arp_packet.h"

namespace shill {

class MockArpClient : public ArpClient {
 public:
  MockArpClient();
  ~MockArpClient() override;

  MOCK_METHOD0(StartReplyListener, bool());
  MOCK_METHOD0(StartRequestListener, bool());
  MOCK_METHOD0(Stop, void());
  MOCK_CONST_METHOD2(ReceivePacket, bool(ArpPacket* packet,
                                         ByteString* sender));
  MOCK_CONST_METHOD1(TransmitRequest, bool(const ArpPacket& packet));
  MOCK_CONST_METHOD0(socket, int());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockArpClient);
};

}  // namespace shill

#endif  // SHILL_MOCK_ARP_CLIENT_H_
