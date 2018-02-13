// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef P2P_HTTP_SERVER_MOCK_SERVER_H_
#define P2P_HTTP_SERVER_MOCK_SERVER_H_

#include <base/macros.h>
#include <gmock/gmock.h>

#include "p2p/http_server/server_interface.h"

namespace p2p {

namespace http_server {

class MockServer : public ServerInterface {
 public:
  MockServer() {}

  MOCK_METHOD0(Start, bool());
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD1(SetMaxDownloadRate, void(int64_t));
  MOCK_METHOD0(Port, uint16_t());
  MOCK_METHOD0(NumConnections, int());
  MOCK_METHOD0(Clock, p2p::common::ClockInterface*());
  MOCK_METHOD1(ConnectionTerminated,
               void(ConnectionDelegateInterface*)); // NOLINT
  MOCK_METHOD2(ReportServerMessage,
               void(p2p::util::P2PServerMessageType, int64_t));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockServer);
};

}  // namespace http_server

}  // namespace p2p

#endif  // P2P_HTTP_SERVER_MOCK_SERVER_H_
