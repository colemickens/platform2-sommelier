// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_DIAGNOSTICSD_FAKE_BROWSER_H_
#define DIAGNOSTICS_DIAGNOSTICSD_FAKE_BROWSER_H_

#include <string>

#include <base/macros.h>
#include <dbus/mock_exported_object.h>
#include <mojo/public/cpp/bindings/binding.h>

#include "diagnostics/diagnosticsd/mock_mojom_diagnosticsd_client.h"
#include "diagnostics/diagnosticsd/mojo_test_utils.h"
#include "mojo/diagnosticsd.mojom.h"

namespace diagnostics {

// Helper class that allows to test communication between the browser and the
// tested code of the diagnosticsd daemon.
class FakeBrowser final {
 public:
  using DBusMethodCallCallback = dbus::ExportedObject::MethodCallCallback;

  using MojomDiagnosticsdClient =
      chromeos::diagnosticsd::mojom::DiagnosticsdClient;
  using MojomDiagnosticsdClientPtr =
      chromeos::diagnosticsd::mojom::DiagnosticsdClientPtr;
  using MojomDiagnosticsdServicePtr =
      chromeos::diagnosticsd::mojom::DiagnosticsdServicePtr;
  using MojomDiagnosticsdServiceFactoryPtr =
      chromeos::diagnosticsd::mojom::DiagnosticsdServiceFactoryPtr;

  // |diagnosticsd_service_factory_ptr| is a pointer to the tested
  // DiagnosticsdServiceFactory instance.
  // |bootstrap_mojo_connection_dbus_method| is a fake substitute for the
  // BootstrapMojoConnection() D-Bus method.
  FakeBrowser(
      MojomDiagnosticsdServiceFactoryPtr* diagnosticsd_service_factory_ptr,
      DBusMethodCallCallback bootstrap_mojo_connection_dbus_method);
  ~FakeBrowser();

  // Returns a mock DiagnosticsdClient instance, whose methods are invoked when
  // FakeBrowser receives incoming Mojo calls from the tested code.
  MockMojomDiagnosticsdClient* diagnosticsd_client() {
    return &diagnosticsd_client_;
  }

  // Call the BootstrapMojoConnection D-Bus method. Returns whether the D-Bus
  // call returned success.
  // |mojo_fd| is the fake file descriptor.
  // |bootstrap_mojo_connection_dbus_method| is the callback that the tested
  // code exposed as the BootstrapMojoConnection D-Bus method.
  //
  // It's not allowed to call this method again after a successful completion.
  bool BootstrapMojoConnection(FakeMojoFdGenerator* fake_mojo_fd_generator);

  // Call the |SendUiMessageToDiagnosticsProcessor| Mojo method
  // on diagnosticsd daemon, which will call the |HandleMessageFromUi| gRPC
  // method on diagnostic processor.
  //
  // It simulates message sent from diagnostics UI extension to diagnostics
  // processor.
  //
  // Returns false when we were not able to copy |json_message| into shared
  // buffer.
  //
  // Must be called only after a successful invocation of
  // BootstrapMojoConnection().
  bool SendUiMessageToDiagnosticsProcessor(const std::string& json_message);

 private:
  // Calls |bootstrap_mojo_connection_dbus_method_| with a fake file descriptor.
  // Returns whether the method call succeeded (it's expected to happen
  // synchronously).
  bool CallBootstrapMojoConnectionDBusMethod(
      FakeMojoFdGenerator* fake_mojo_fd_generator);
  // Calls GetService() Mojo method on |diagnosticsd_service_factory_ptr_|,
  // initializes |diagnosticsd_service_ptr_| so that it points to the tested
  // service, registers |diagnosticsd_client_| to handle incoming Mojo requests.
  void CallGetServiceMojoMethod();

  // Unowned. Points to the tested DiagnosticsdServiceFactory instance.
  MojomDiagnosticsdServiceFactoryPtr* const diagnosticsd_service_factory_ptr_;
  // Fake substitute for the BootstrapMojoConnection() D-Bus method.
  DBusMethodCallCallback bootstrap_mojo_connection_dbus_method_;

  // Mock DiagnosticsdClient instance. After an invocation of
  // CallGetServiceMojoMethod() it becomes registered to receive incoming Mojo
  // requests from the tested code.
  MockMojomDiagnosticsdClient diagnosticsd_client_;
  // Mojo binding that is associated with |diagnosticsd_client_|.
  mojo::Binding<MojomDiagnosticsdClient> diagnosticsd_client_binding_;

  // Mojo interface pointer to the DiagnosticsdService service exposed by the
  // tested code. Gets initialized after a call to CallGetServiceMojoMethod().
  MojomDiagnosticsdServicePtr diagnosticsd_service_ptr_;

  DISALLOW_COPY_AND_ASSIGN(FakeBrowser);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_DIAGNOSTICSD_FAKE_BROWSER_H_
