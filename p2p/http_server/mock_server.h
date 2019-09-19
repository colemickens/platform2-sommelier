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
  MockServer() = default;

  MOCK_METHOD(bool, Start, (), (override));
  MOCK_METHOD(void, Stop, (), (override));
  MOCK_METHOD(void, SetMaxDownloadRate, (int64_t), (override));
  MOCK_METHOD(uint16_t, Port, (), (override));
  MOCK_METHOD(int, NumConnections, (), (override));
  MOCK_METHOD(p2p::common::ClockInterface*, Clock, (), (override));
  MOCK_METHOD(void,
              ConnectionTerminated,
              (ConnectionDelegateInterface*),
              (override));
  MOCK_METHOD(void,
              ReportServerMessage,
              (p2p::util::P2PServerMessageType, int64_t),
              (override));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockServer);
};

}  // namespace http_server

}  // namespace p2p

#endif  // P2P_HTTP_SERVER_MOCK_SERVER_H_
