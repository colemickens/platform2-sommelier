// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include <base/message_loop/message_loop.h>
#include <base/strings/string_piece.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mojo/public/cpp/bindings/binding.h>
#include <mojo/public/cpp/bindings/interface_ptr.h>
#include <mojo/public/cpp/system/buffer.h>

#include "diagnostics/diagnosticsd/diagnosticsd_mojo_service.h"
#include "diagnostics/diagnosticsd/mock_mojom_diagnosticsd_client.h"
#include "diagnostics/diagnosticsd/mojo_test_utils.h"
#include "diagnostics/diagnosticsd/mojo_utils.h"

#include "mojo/diagnosticsd.mojom.h"

using testing::_;
using testing::StrictMock;

using MojomDiagnosticsdClient =
    chromeos::diagnosticsd::mojom::DiagnosticsdClient;
using MojomDiagnosticsdClientPtr =
    chromeos::diagnosticsd::mojom::DiagnosticsdClientPtr;
using MojomDiagnosticsdServiceRequest =
    chromeos::diagnosticsd::mojom::DiagnosticsdServiceRequest;

namespace diagnostics {

namespace {

void EmptySendUiMessageToDiagnosticsProcessorWithSizeCallback(
    mojo::ScopedHandle response_json_message,
    int64_t response_json_message_size) {}

class MockDiagnosticsdMojoServiceDelegate
    : public DiagnosticsdMojoService::Delegate {
 public:
  MOCK_METHOD2(
      SendGrpcUiMessageToDiagnosticsProcessor,
      void(base::StringPiece json_message,
           const SendGrpcUiMessageToDiagnosticsProcessorCallback& callback));
};

// Tests for the DiagnosticsdMojoService class.
class DiagnosticsdMojoServiceTest : public testing::Test {
 protected:
  DiagnosticsdMojoServiceTest() {
    // Obtain Mojo interface pointer that talks to |mojo_client_| - the
    // connection between them will be maintained by |mojo_client_binding_|.
    MojomDiagnosticsdClientPtr mojo_client_interface_ptr;
    mojo_client_binding_ =
        std::make_unique<mojo::Binding<MojomDiagnosticsdClient>>(
            &mojo_client_, &mojo_client_interface_ptr);
    DCHECK(mojo_client_interface_ptr);

    service_ = std::make_unique<DiagnosticsdMojoService>(
        &delegate_,
        MojomDiagnosticsdServiceRequest() /* self_interface_request */,
        std::move(mojo_client_interface_ptr));
  }

  MockDiagnosticsdMojoServiceDelegate* delegate() { return &delegate_; }

  // TODO(lamzin@google.com): Extract the response JSON message and verify its
  // value.
  void SendJsonMessage(base::StringPiece json_message) {
    mojo::ScopedHandle handle =
        CreateReadOnlySharedMemoryMojoHandle(json_message);
    ASSERT_TRUE(handle.is_valid());
    service_->SendUiMessageToDiagnosticsProcessorWithSize(
        std::move(handle), json_message.length(),
        base::Bind(&EmptySendUiMessageToDiagnosticsProcessorWithSizeCallback));
  }

 private:
  base::MessageLoop message_loop_;
  StrictMock<MockMojomDiagnosticsdClient> mojo_client_;
  std::unique_ptr<mojo::Binding<MojomDiagnosticsdClient>> mojo_client_binding_;
  StrictMock<MockDiagnosticsdMojoServiceDelegate> delegate_;
  std::unique_ptr<DiagnosticsdMojoService> service_;
};

}  // namespace

TEST_F(DiagnosticsdMojoServiceTest,
       SendUiMessageToDiagnosticsProcessorWithSize) {
  base::StringPiece json_message("{\"message\": \"Hello world!\"}");
  EXPECT_CALL(*delegate(),
              SendGrpcUiMessageToDiagnosticsProcessor(json_message, _));
  ASSERT_NO_FATAL_FAILURE(SendJsonMessage(json_message));
}

TEST_F(DiagnosticsdMojoServiceTest,
       SendUiMessageToDiagnosticsProcessorWithSizeInvalidJSON) {
  base::StringPiece json_message("{'message': 'Hello world!'}");
  EXPECT_CALL(*delegate(), SendGrpcUiMessageToDiagnosticsProcessor(_, _))
      .Times(0);
  ASSERT_NO_FATAL_FAILURE(SendJsonMessage(json_message));
}

}  // namespace diagnostics
