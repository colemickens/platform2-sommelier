// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include <base/message_loop/message_loop.h>
#include <base/time/time.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "diagnostics/telem/async_grpc_client_adapter.h"
#include "diagnostics/telem/telem_connection.h"
#include "diagnostics/telem/telemetry_item_enum.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::StrictMock;
using ::testing::WithArg;

namespace diagnostics {

namespace {

using ProcDataCallback = base::Callback<void(
    std::unique_ptr<grpc_api::GetProcDataResponse> response)>;

class MockAsyncGrpcClientAdapter : public AsyncGrpcClientAdapter {
 public:
  void Connect(const std::string& target_uri) override {}

  MOCK_CONST_METHOD0(IsConnected, bool());
  MOCK_METHOD1(Shutdown, void(const base::Closure& on_shutdown));
  MOCK_METHOD2(GetProcData,
               void(const grpc_api::GetProcDataRequest& request,
                    ProcDataCallback callback));
};

}  // namespace

class TelemConnectionTest : public ::testing::Test {
 public:
  TelemConnectionTest() {
    EXPECT_CALL(mock_adapter_, Shutdown(_))
        .WillOnce(Invoke(
            [](const base::Closure& on_shutdown) { on_shutdown.Run(); }));
    EXPECT_CALL(mock_adapter_, GetProcData(_, _))
        .WillOnce(WithArg<1>(
            Invoke([](ProcDataCallback callback) { callback.Run(nullptr); })));
  }

  void SetConnection(std::unique_ptr<TelemConnection> connection) {
    connection_ = std::move(connection);
  }

  TelemConnection* connection() { return connection_.get(); }

 private:
  base::MessageLoopForIO message_loop_;
  StrictMock<MockAsyncGrpcClientAdapter> mock_adapter_;
  std::unique_ptr<TelemConnection> connection_ =
      std::make_unique<TelemConnection>(&mock_adapter_);

  DISALLOW_COPY_AND_ASSIGN(TelemConnectionTest);
};

// Test that we catch an error response.
TEST_F(TelemConnectionTest, NullptrResponse) {
  EXPECT_EQ(connection()->GetItem(TelemetryItemEnum::kMemTotalMebibytes,
                                  base::TimeDelta::FromSeconds(0)),
            nullptr);
}

}  // namespace diagnostics
