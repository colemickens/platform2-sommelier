// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_HTTP_REQUEST_H_
#define SHILL_MOCK_HTTP_REQUEST_H_

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/http_request.h"
#include "shill/http_url.h"  // MOCK_METHOD3() call below needs sizeof(HTTPURL).

namespace shill {

class MockHTTPRequest : public HTTPRequest {
 public:
  MockHTTPRequest(ConnectionRefPtr connection);
  virtual ~MockHTTPRequest();

  MOCK_METHOD3(Start, HTTPRequest::Result(
      const HTTPURL &url,
      Callback1<int>::Type *read_event_callback,
      Callback1<Result>::Type *result_callback));
  MOCK_METHOD0(Stop, void());
  MOCK_CONST_METHOD0(response_data, const ByteString &());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockHTTPRequest);
};

}  // namespace shill

#endif  // SHILL_MOCK_HTTP_REQUEST_H_
