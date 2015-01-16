// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCHROMEOS_CHROMEOS_HTTP_MOCK_TRANSPORT_H_
#define LIBCHROMEOS_CHROMEOS_HTTP_MOCK_TRANSPORT_H_

#include <memory>
#include <string>

#include <base/macros.h>
#include <chromeos/http/http_transport.h>
#include <gmock/gmock.h>

namespace chromeos {
namespace http {

class MockTransport : public Transport {
 public:
  MockTransport() = default;

  MOCK_METHOD6(CreateConnection,
               std::shared_ptr<Connection>(const std::string&,
                                           const std::string&,
                                           const HeaderList&,
                                           const std::string&,
                                           const std::string&,
                                           chromeos::ErrorPtr*));
  MOCK_METHOD2(RunCallbackAsync,
               void(const tracked_objects::Location&, const base::Closure&));
  MOCK_METHOD3(StartAsyncTransfer,
               int(Connection*, const SuccessCallback&, const ErrorCallback&));
  MOCK_METHOD1(CancelRequest, bool(int));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockTransport);
};

}  // namespace http
}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_HTTP_MOCK_TRANSPORT_H_
