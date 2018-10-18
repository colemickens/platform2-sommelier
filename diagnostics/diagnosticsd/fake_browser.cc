// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/diagnosticsd/fake_browser.h"

#include <memory>
#include <utility>

#include <base/bind.h>
#include <brillo/bind_lambda.h>
#include <dbus/message.h>
#include <dbus/diagnosticsd/dbus-constants.h>

namespace diagnostics {

FakeBrowser::FakeBrowser(
    MojomDiagnosticsdServicePtr* diagnosticsd_service_ptr,
    DBusMethodCallCallback bootstrap_mojo_connection_dbus_method)
    : diagnosticsd_service_ptr_(diagnosticsd_service_ptr),
      bootstrap_mojo_connection_dbus_method_(
          bootstrap_mojo_connection_dbus_method) {}

FakeBrowser::~FakeBrowser() = default;

bool FakeBrowser::BootstrapMojoConnection(
    FakeMojoFdGenerator* fake_mojo_fd_generator) {
  // Prepare input data for the call.
  const int kFakeMethodCallSerial = 1;
  dbus::MethodCall method_call(kDiagnosticsdServiceInterface,
                               kDiagnosticsdBootstrapMojoConnectionMethod);
  method_call.SetSerial(kFakeMethodCallSerial);
  dbus::MessageWriter message_writer(&method_call);
  message_writer.AppendFileDescriptor(fake_mojo_fd_generator->MakeFd().get());

  // Storage for the output data returned by the call.
  std::unique_ptr<dbus::Response> response;
  const auto response_writer_callback = base::Bind(
      [](std::unique_ptr<dbus::Response>* response,
         std::unique_ptr<dbus::Response> passed_response) {
        *response = std::move(passed_response);
      },
      &response);

  // Call the tested method and extract its result.
  if (bootstrap_mojo_connection_dbus_method_.is_null())
    return false;
  bootstrap_mojo_connection_dbus_method_.Run(&method_call,
                                             response_writer_callback);
  return response && response->GetMessageType() != dbus::Message::MESSAGE_ERROR;
}

bool FakeBrowser::SendMessageToDiagnosticsProcessor(
    const std::string& json_message) {
  std::unique_ptr<mojo::ScopedSharedBufferHandle> shared_buffer =
      helper::WriteToSharedBuffer(json_message);
  if (!shared_buffer) {
    return false;
  }
  diagnosticsd_service_ptr_->get()->SendUiMessageToDiagnosticsProcessorWithSize(
      std::move(*shared_buffer.get()), json_message.length(),
      mojo::Callback<void()>());
  return true;
}

}  // namespace diagnostics
