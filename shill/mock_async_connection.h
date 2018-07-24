// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_ASYNC_CONNECTION_H_
#define SHILL_MOCK_ASYNC_CONNECTION_H_

#include <base/macros.h>
#include <gmock/gmock.h>

#include "shill/async_connection.h"

namespace shill {

class MockAsyncConnection : public AsyncConnection {
 public:
  MockAsyncConnection();
  ~MockAsyncConnection() override;

  MOCK_METHOD2(Start, bool(const IPAddress& address, int port));
  MOCK_METHOD0(Stop, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAsyncConnection);
};

}  // namespace shill

#endif  // SHILL_MOCK_ASYNC_CONNECTION_H_
