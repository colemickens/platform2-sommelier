// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_INCLUDE_WEAVE_TEST_MOCK_NETWORK_H_
#define LIBWEAVE_INCLUDE_WEAVE_TEST_MOCK_NETWORK_H_

#include <weave/network.h>

#include <string>

#include <gmock/gmock.h>

namespace weave {
namespace test {

class MockNetwork : public Network {
 public:
  MockNetwork() {}
  ~MockNetwork() override = default;

  MOCK_METHOD1(AddOnConnectionChangedCallback,
               void(const OnConnectionChangedCallback&));
  MOCK_METHOD4(ConnectToService,
               bool(const std::string&,
                    const std::string&,
                    const base::Closure&,
                    ErrorPtr*));
  MOCK_CONST_METHOD0(GetConnectionState, NetworkState());
  MOCK_METHOD1(EnableAccessPoint, void(const std::string&));
  MOCK_METHOD0(DisableAccessPoint, void());

  MOCK_METHOD2(MockOpenSocketBlocking, Stream*(const std::string&, uint16_t));
  MOCK_METHOD2(MockCreateTlsStream, Stream*(Stream*, const std::string&));

  std::unique_ptr<Stream> OpenSocketBlocking(const std::string& host,
                                             uint16_t port) override {
    return std::unique_ptr<Stream>{MockOpenSocketBlocking(host, port)};
  }

  void CreateTlsStream(
      std::unique_ptr<Stream> socket,
      const std::string& host,
      const base::Callback<void(std::unique_ptr<Stream>)>& success_callback,
      const base::Callback<void(const Error*)>& error_callback) override {
    success_callback.Run(
        std::unique_ptr<Stream>{MockCreateTlsStream(socket.get(), host)});
  }
};

}  // namespace test
}  // namespace weave

#endif  // LIBWEAVE_INCLUDE_WEAVE_TEST_MOCK_NETWORK_H_
