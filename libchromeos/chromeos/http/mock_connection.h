// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCHROMEOS_CHROMEOS_HTTP_MOCK_CONNECTION_H_
#define LIBCHROMEOS_CHROMEOS_HTTP_MOCK_CONNECTION_H_

#include <memory>
#include <string>

#include <base/macros.h>
#include <chromeos/http/http_connection.h>
#include <gmock/gmock.h>

namespace chromeos {
namespace http {

class MockConnection : public Connection {
 public:
  using Connection::Connection;

  MOCK_METHOD2(SendHeaders, bool(const HeaderList&, ErrorPtr*));
  MOCK_METHOD2(MockSetRequestData, bool(DataReaderInterface*, ErrorPtr*));
  MOCK_METHOD1(FinishRequest, bool(ErrorPtr*));
  MOCK_METHOD2(FinishRequestAsync,
               int(const SuccessCallback&, const ErrorCallback&));
  MOCK_CONST_METHOD0(GetResponseStatusCode, int());
  MOCK_CONST_METHOD0(GetResponseStatusText, std::string());
  MOCK_CONST_METHOD0(GetProtocolVersion, std::string());
  MOCK_CONST_METHOD1(GetResponseHeader, std::string(const std::string&));
  MOCK_CONST_METHOD0(GetResponseDataSize, uint64_t());
  MOCK_METHOD4(ReadResponseData, bool(void*, size_t, size_t*, ErrorPtr*));

 private:
  bool SetRequestData(std::unique_ptr<DataReaderInterface> data_reader,
                      chromeos::ErrorPtr* error) override {
    return MockSetRequestData(data_reader.get(), error);
  }

  DISALLOW_COPY_AND_ASSIGN(MockConnection);
};

}  // namespace http
}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_HTTP_MOCK_CONNECTION_H_
