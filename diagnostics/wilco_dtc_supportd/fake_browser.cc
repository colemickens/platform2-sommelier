// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/wilco_dtc_supportd/fake_browser.h"

#include <memory>
#include <utility>

#include <base/bind.h>
#include <base/strings/string_piece.h>
#include <dbus/message.h>
#include <dbus/diagnosticsd/dbus-constants.h>
#include <mojo/public/cpp/bindings/interface_request.h>
#include <mojo/public/cpp/system/buffer.h>

#include "diagnostics/wilco_dtc_supportd/mojo_utils.h"

namespace diagnostics {

using MojomDiagnosticsdService =
    chromeos::diagnosticsd::mojom::DiagnosticsdService;

FakeBrowser::FakeBrowser(
    MojomDiagnosticsdServiceFactoryPtr* wilco_dtc_supportd_service_factory_ptr,
    DBusMethodCallCallback bootstrap_mojo_connection_dbus_method)
    : wilco_dtc_supportd_service_factory_ptr_(
          wilco_dtc_supportd_service_factory_ptr),
      bootstrap_mojo_connection_dbus_method_(
          bootstrap_mojo_connection_dbus_method),
      wilco_dtc_supportd_client_binding_(
          &wilco_dtc_supportd_client_ /* impl */) {
  DCHECK(wilco_dtc_supportd_service_factory_ptr);
  DCHECK(!bootstrap_mojo_connection_dbus_method.is_null());
}

FakeBrowser::~FakeBrowser() = default;

bool FakeBrowser::BootstrapMojoConnection(
    FakeMojoFdGenerator* fake_mojo_fd_generator) {
  if (!CallBootstrapMojoConnectionDBusMethod(fake_mojo_fd_generator))
    return false;
  CallGetServiceMojoMethod();
  return true;
}

bool FakeBrowser::SendUiMessageToDiagnosticsProcessor(
    const std::string& json_message,
    const base::Callback<void(mojo::ScopedHandle)>& callback) {
  mojo::ScopedHandle handle =
      CreateReadOnlySharedMemoryMojoHandle(base::StringPiece(json_message));
  if (!handle.is_valid()) {
    return false;
  }
  wilco_dtc_supportd_service_ptr_->SendUiMessageToDiagnosticsProcessor(
      std::move(handle), callback);
  return true;
}

void FakeBrowser::NotifyConfigurationDataChanged() {
  wilco_dtc_supportd_service_ptr_->NotifyConfigurationDataChanged();
}

bool FakeBrowser::CallBootstrapMojoConnectionDBusMethod(
    FakeMojoFdGenerator* fake_mojo_fd_generator) {
  // Prepare input data for the D-Bus call.
  const int kFakeMethodCallSerial = 1;
  dbus::MethodCall method_call(kDiagnosticsdServiceInterface,
                               kDiagnosticsdBootstrapMojoConnectionMethod);
  method_call.SetSerial(kFakeMethodCallSerial);
  dbus::MessageWriter message_writer(&method_call);
  message_writer.AppendFileDescriptor(fake_mojo_fd_generator->MakeFd().get());

  // Storage for the output data returned by the D-Bus call.
  std::unique_ptr<dbus::Response> response;
  const auto response_writer_callback = base::Bind(
      [](std::unique_ptr<dbus::Response>* response,
         std::unique_ptr<dbus::Response> passed_response) {
        *response = std::move(passed_response);
      },
      &response);

  // Call the D-Bus method and extract its result.
  if (bootstrap_mojo_connection_dbus_method_.is_null())
    return false;
  bootstrap_mojo_connection_dbus_method_.Run(&method_call,
                                             response_writer_callback);
  return response && response->GetMessageType() != dbus::Message::MESSAGE_ERROR;
}

void FakeBrowser::CallGetServiceMojoMethod() {
  // Queue a Mojo GetService() method call that allows to establish full-duplex
  // Mojo communication with the tested Mojo service.
  // After this call, |wilco_dtc_supportd_service_ptr_| can be used for requests
  // to the tested service and |wilco_dtc_supportd_client_| for receiving
  // requests made by the tested service. Note that despite that GetService() is
  // an asynchronous call, it's actually allowed to use
  // |wilco_dtc_supportd_service_ptr_| straight away, before the call completes.
  MojomDiagnosticsdClientPtr wilco_dtc_supportd_client_proxy;
  wilco_dtc_supportd_client_binding_.Bind(
      mojo::MakeRequest(&wilco_dtc_supportd_client_proxy));
  (*wilco_dtc_supportd_service_factory_ptr_)
      ->GetService(mojo::MakeRequest(&wilco_dtc_supportd_service_ptr_),
                   std::move(wilco_dtc_supportd_client_proxy), base::Closure());
}

}  // namespace diagnostics
