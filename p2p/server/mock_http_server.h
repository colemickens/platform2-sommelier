// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef P2P_SERVER_MOCK_HTTP_SERVER_H_
#define P2P_SERVER_MOCK_HTTP_SERVER_H_

#include "p2p/server/fake_http_server.h"

#include <string>

#include <gmock/gmock.h>

#include <base/callback.h>
#include <base/files/file_path.h>
#include <base/macros.h>

namespace p2p {

namespace server {

class MockHttpServer : public HttpServer {
 public:
  MockHttpServer() {
    // Delegate all calls to the fake instance
    ON_CALL(*this, Start())
        .WillByDefault(testing::Invoke(&fake_, &FakeHttpServer::Start));
    ON_CALL(*this, Stop())
        .WillByDefault(testing::Invoke(&fake_, &FakeHttpServer::Stop));
    ON_CALL(*this, IsRunning())
        .WillByDefault(testing::Invoke(&fake_, &FakeHttpServer::IsRunning));
    ON_CALL(*this, Port())
        .WillByDefault(testing::Invoke(&fake_, &FakeHttpServer::Port));
    ON_CALL(*this, SetNumConnectionsCallback(testing::_)).WillByDefault(
        testing::Invoke(&fake_, &FakeHttpServer::SetNumConnectionsCallback));
  }

  MOCK_METHOD0(Start, bool());
  MOCK_METHOD0(Stop, bool());
  MOCK_METHOD0(IsRunning, bool());
  MOCK_METHOD0(Port, uint16_t());
  MOCK_METHOD1(SetNumConnectionsCallback, void(NumConnectionsCallback));

  FakeHttpServer& fake() { return fake_; }

 private:
  FakeHttpServer fake_;

  DISALLOW_COPY_AND_ASSIGN(MockHttpServer);
};

}  // namespace server

}  // namespace p2p

#endif  // P2P_SERVER_MOCK_HTTP_SERVER_H_
