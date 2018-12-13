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

constexpr char kFakeMeminfoFileContents[] =
    "MemTotal:      3906320 kB\nMemFree:      873180 kB\n";
constexpr int kFakeMemTotalMebibytes = 3814;
constexpr int kFakeMemFreeMebibytes = 852;

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
 protected:
  TelemConnectionTest() {
    EXPECT_CALL(mock_adapter_, Shutdown(_))
        .WillOnce(Invoke(
            [](const base::Closure& on_shutdown) { on_shutdown.Run(); }));
  }

  void SetConnection(std::unique_ptr<TelemConnection> connection) {
    connection_ = std::move(connection);
  }

  // Sets the MockAsyncGrpcClientAdapter to run the next callback with
  // a null pointer as its response.
  void SetNullptrProcDataResponse() {
    EXPECT_CALL(mock_adapter_, GetProcData(_, _))
        .WillOnce(WithArg<1>(
            Invoke([](ProcDataCallback callback) { callback.Run(nullptr); })));
  }

  // Sets the MockAsyncGrpcClientAdapter to run the next callback with a
  // fake /proc/meminfo file dump as its response.
  void SetMeminfoProcDataResponse() {
    std::unique_ptr<grpc_api::GetProcDataResponse> response =
        std::make_unique<grpc_api::GetProcDataResponse>();
    // response->mutable_file_dump()->Add()
    EXPECT_CALL(mock_adapter_, GetProcData(_, _))
        .WillRepeatedly(
            Invoke([](::testing::Unused, ProcDataCallback callback) mutable {
              std::unique_ptr<grpc_api::GetProcDataResponse> reply =
                  std::make_unique<grpc_api::GetProcDataResponse>();
              grpc_api::FileDump fake_file_dump;
              fake_file_dump.set_contents(kFakeMeminfoFileContents);
              reply->mutable_file_dump()->Add()->Swap(&fake_file_dump);
              callback.Run(std::move(reply));
            }));
  }

  // Checks whether the response matches the expected response from a
  // GetItem(TelemetryItemEnum::kMemTotalMebibytes) command.
  void CheckMemTotalItemResponse(
      const base::Optional<base::Value>& actual_response) {
    int val;

    ASSERT_TRUE(actual_response);
    ASSERT_TRUE(actual_response.value().GetAsInteger(&val));
    EXPECT_EQ(val, kFakeMemTotalMebibytes);
  }

  // Checks whether the response matches the expected response
  // from a GetGroup(TelemetryGroupEnum::kDisk) command.
  void CheckDiskGroupResponse(
      const std::vector<
          std::pair<TelemetryItemEnum, const base::Optional<base::Value>>>&
          actual_response) {
    // Indices where we expect MemTotal and MemFree to occur in the response.
    constexpr int kMemTotalIndex = 0;
    constexpr int kMemFreeIndex = 1;
    // Number of items in the kDisk group.
    constexpr int kNumDiskItems = 2;

    int val;

    EXPECT_EQ(actual_response.size(), kNumDiskItems);
    ASSERT_EQ(actual_response[kMemTotalIndex].first,
              TelemetryItemEnum::kMemTotalMebibytes);
    ASSERT_TRUE(actual_response[kMemTotalIndex].second);
    ASSERT_TRUE(
        actual_response[kMemTotalIndex].second.value().GetAsInteger(&val));
    EXPECT_EQ(val, kFakeMemTotalMebibytes);
    EXPECT_EQ(actual_response[kMemFreeIndex].first,
              TelemetryItemEnum::kMemFreeMebibytes);
    ASSERT_TRUE(actual_response[kMemFreeIndex].second);
    ASSERT_TRUE(
        actual_response[kMemFreeIndex].second.value().GetAsInteger(&val));
    EXPECT_EQ(val, kFakeMemFreeMebibytes);
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
  SetNullptrProcDataResponse();

  EXPECT_EQ(connection()->GetItem(TelemetryItemEnum::kMemTotalMebibytes,
                                  base::TimeDelta::FromSeconds(0)),
            base::nullopt);
}

// Test that we can retrieve a telemetry item.
TEST_F(TelemConnectionTest, GetMemTotal) {
  SetMeminfoProcDataResponse();

  auto mem_total = connection()->GetItem(TelemetryItemEnum::kMemTotalMebibytes,
                                         base::TimeDelta::FromSeconds(0));
  CheckMemTotalItemResponse(mem_total);
}

// Test that we can retrieve a group of telemetry items.
TEST_F(TelemConnectionTest, GetDiskGroup) {
  SetMeminfoProcDataResponse();

  auto disk_group = connection()->GetGroup(TelemetryGroupEnum::kDisk,
                                           base::TimeDelta::FromSeconds(0));
  CheckDiskGroupResponse(disk_group);
}

}  // namespace diagnostics
