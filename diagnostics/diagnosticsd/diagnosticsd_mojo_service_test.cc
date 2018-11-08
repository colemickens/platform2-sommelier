// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include <base/strings/string_piece.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mojo/public/cpp/system/buffer.h>

#include "diagnostics/diagnosticsd/diagnosticsd_mojo_service.h"
#include "diagnostics/diagnosticsd/mojo_test_utils.h"

using testing::_;
using testing::StrictMock;

namespace diagnostics {

namespace {

class MockDiagnosticsdMojoServiceDelegate
    : public DiagnosticsdMojoService::Delegate {
 public:
  MOCK_METHOD1(SendGrpcUiMessageToDiagnosticsProcessor,
               void(base::StringPiece json_message));
};

// Tests for the DiagnosticsdMojoService class.
class DiagnosticsdMojoServiceTest : public testing::Test {
 protected:
  void SendJsonMessage(const std::string& json_message) {
    std::unique_ptr<mojo::ScopedSharedBufferHandle> shared_buffer =
        helper::WriteToSharedBuffer(json_message);
    ASSERT_TRUE(shared_buffer);
    service_.SendUiMessageToDiagnosticsProcessorWithSize(
        std::move(*shared_buffer.get()), json_message.length(),
        mojo::Callback<void()>());
  }

  StrictMock<MockDiagnosticsdMojoServiceDelegate> delegate_;
  DiagnosticsdMojoService service_{&delegate_};
};

}  // namespace

// TODO(crbug.com/893756): Causing failures on pre-cq due to unavailability of
// /dev/shm.
TEST_F(DiagnosticsdMojoServiceTest,
       DISABLED_SendUiMessageToDiagnosticsProcessorWithSize) {
  std::string json_message("{\"message\": \"Hello world!\"}");
  EXPECT_CALL(delegate_, SendGrpcUiMessageToDiagnosticsProcessor(
                             base::StringPiece(json_message)));
  SendJsonMessage(json_message);
}

// TODO(crbug.com/893756): Causing failures on pre-cq due to unavailability of
// /dev/shm.
TEST_F(DiagnosticsdMojoServiceTest,
       DISABLED_SendUiMessageToDiagnosticsProcessorWithSizeInvalidJSON) {
  std::string json_message("{\'message\': \'Hello world!\'}");
  EXPECT_CALL(delegate_, SendGrpcUiMessageToDiagnosticsProcessor(_)).Times(0);
  SendJsonMessage(json_message);
}

}  // namespace diagnostics
