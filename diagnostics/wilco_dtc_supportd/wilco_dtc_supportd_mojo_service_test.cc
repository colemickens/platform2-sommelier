// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/message_loop/message_loop.h>
#include <base/optional.h>
#include <base/run_loop.h>
#include <base/strings/string_piece.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mojo/edk/embedder/embedder.h>
#include <mojo/public/cpp/bindings/binding.h>
#include <mojo/public/cpp/bindings/interface_ptr.h>
#include <mojo/public/cpp/system/buffer.h>

#include "diagnostics/wilco_dtc_supportd/mock_mojom_wilco_dtc_supportd_client.h"
#include "diagnostics/wilco_dtc_supportd/mojo_test_utils.h"
#include "diagnostics/wilco_dtc_supportd/mojo_utils.h"
#include "diagnostics/wilco_dtc_supportd/wilco_dtc_supportd_mojo_service.h"

#include "mojo/diagnosticsd.mojom.h"

using testing::_;
using testing::StrictMock;

using MojomDiagnosticsdClient =
    chromeos::diagnosticsd::mojom::DiagnosticsdClient;
using MojomDiagnosticsdClientPtr =
    chromeos::diagnosticsd::mojom::DiagnosticsdClientPtr;
using MojomDiagnosticsdServiceRequest =
    chromeos::diagnosticsd::mojom::DiagnosticsdServiceRequest;
using MojomDiagnosticsdWebRequestStatus =
    chromeos::diagnosticsd::mojom::DiagnosticsdWebRequestStatus;
using MojomDiagnosticsdWebRequestHttpMethod =
    chromeos::diagnosticsd::mojom::DiagnosticsdWebRequestHttpMethod;

namespace diagnostics {

namespace {

constexpr char kHttpsUrl[] = "https://www.google.com";
constexpr int kHttpStatusOk = 200;
constexpr char kFakeBody[] = "fake response/request body";

void EmptySendUiMessageToDiagnosticsProcessorCallback(
    mojo::ScopedHandle response_json_message) {}

class MockWilcoDtcSupportdMojoServiceDelegate
    : public WilcoDtcSupportdMojoService::Delegate {
 public:
  MOCK_METHOD2(SendGrpcUiMessageToWilcoDtc,
               void(base::StringPiece json_message,
                    const SendGrpcUiMessageToWilcoDtcCallback& callback));
};

// Tests for the WilcoDtcSupportdMojoService class.
class WilcoDtcSupportdMojoServiceTest : public testing::Test {
 protected:
  WilcoDtcSupportdMojoServiceTest() {
    mojo::edk::Init();
    // Obtain Mojo interface pointer that talks to |mojo_client_| - the
    // connection between them will be maintained by |mojo_client_binding_|.
    MojomDiagnosticsdClientPtr mojo_client_interface_ptr;
    mojo_client_binding_ =
        std::make_unique<mojo::Binding<MojomDiagnosticsdClient>>(
            &mojo_client_, &mojo_client_interface_ptr);
    DCHECK(mojo_client_interface_ptr);

    service_ = std::make_unique<WilcoDtcSupportdMojoService>(
        &delegate_,
        MojomDiagnosticsdServiceRequest() /* self_interface_request */,
        std::move(mojo_client_interface_ptr));
  }

  MockWilcoDtcSupportdMojoServiceDelegate* delegate() { return &delegate_; }
  MockMojomWilcoDtcSupportdClient* mojo_client() { return &mojo_client_; }

  // TODO(lamzin@google.com): Extract the response JSON message and verify its
  // value.
  void SendJsonMessage(base::StringPiece json_message) {
    mojo::ScopedHandle handle =
        CreateReadOnlySharedMemoryMojoHandle(json_message);
    ASSERT_TRUE(handle.is_valid());
    service_->SendUiMessageToDiagnosticsProcessor(
        std::move(handle),
        base::Bind(&EmptySendUiMessageToDiagnosticsProcessorCallback));
  }

  void PerformWebRequest(MojomDiagnosticsdWebRequestHttpMethod http_method,
                         const std::string& url,
                         const std::vector<std::string>& headers,
                         const std::string& request_body,
                         MojomDiagnosticsdWebRequestStatus expected_status,
                         int expected_http_status) {
    base::RunLoop run_loop;
    // According to the implementation of MockMojomWilcoDtcSupportdClient
    // response_body is equal to request_body.
    service_->PerformWebRequest(
        http_method, url, headers, request_body,
        base::Bind(
            [](const base::Closure& quit_closure,
               MojomDiagnosticsdWebRequestStatus expected_status,
               int expected_http_status, std::string expected_response_body,
               MojomDiagnosticsdWebRequestStatus status, int http_status,
               base::StringPiece response_body) {
              EXPECT_EQ(expected_status, status);
              EXPECT_EQ(expected_http_status, http_status);
              EXPECT_EQ(expected_response_body, response_body);
              quit_closure.Run();
            },
            run_loop.QuitClosure(), expected_status, expected_http_status,
            request_body));
    run_loop.Run();
  }

 private:
  base::MessageLoop message_loop_;
  StrictMock<MockMojomWilcoDtcSupportdClient> mojo_client_;
  std::unique_ptr<mojo::Binding<MojomDiagnosticsdClient>> mojo_client_binding_;
  StrictMock<MockWilcoDtcSupportdMojoServiceDelegate> delegate_;
  std::unique_ptr<WilcoDtcSupportdMojoService> service_;
};

}  // namespace

TEST_F(WilcoDtcSupportdMojoServiceTest, SendUiMessageToWilcoDtc) {
  base::StringPiece json_message("{\"message\": \"Hello world!\"}");
  EXPECT_CALL(*delegate(), SendGrpcUiMessageToWilcoDtc(json_message, _));
  ASSERT_NO_FATAL_FAILURE(SendJsonMessage(json_message));
}

TEST_F(WilcoDtcSupportdMojoServiceTest, SendUiMessageToWilcoDtcInvalidJSON) {
  base::StringPiece json_message("{'message': 'Hello world!'}");
  EXPECT_CALL(*delegate(), SendGrpcUiMessageToWilcoDtc(_, _)).Times(0);
  ASSERT_NO_FATAL_FAILURE(SendJsonMessage(json_message));
}

TEST_F(WilcoDtcSupportdMojoServiceTest, PerformWebRequest) {
  EXPECT_CALL(
      *mojo_client(),
      PerformWebRequestImpl(MojomDiagnosticsdWebRequestHttpMethod::kGet,
                            kHttpsUrl, std::vector<std::string>(), kFakeBody));
  ASSERT_NO_FATAL_FAILURE(
      PerformWebRequest(MojomDiagnosticsdWebRequestHttpMethod::kGet, kHttpsUrl,
                        std::vector<std::string>(), kFakeBody,
                        MojomDiagnosticsdWebRequestStatus::kOk, kHttpStatusOk));
}

}  // namespace diagnostics
