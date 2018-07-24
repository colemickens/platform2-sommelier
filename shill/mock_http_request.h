// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_HTTP_REQUEST_H_
#define SHILL_MOCK_HTTP_REQUEST_H_

#include <base/macros.h>
#include <gmock/gmock.h>

#include "shill/http_request.h"
#include "shill/http_url.h"  // MOCK_METHOD3() call below needs sizeof(HTTPURL).

namespace shill {

class MockHTTPRequest : public HTTPRequest {
 public:
  explicit MockHTTPRequest(ConnectionRefPtr connection);
  ~MockHTTPRequest() override;

  MOCK_METHOD3(Start, HTTPRequest::Result(
      const HTTPURL& url,
      const base::Callback<void(const ByteString&)>& read_event_callback,
      const base::Callback<void(Result, const ByteString&)>& result_callback));
  MOCK_METHOD0(Stop, void());
  MOCK_CONST_METHOD0(response_data, const ByteString& ());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockHTTPRequest);
};

}  // namespace shill

#endif  // SHILL_MOCK_HTTP_REQUEST_H_
