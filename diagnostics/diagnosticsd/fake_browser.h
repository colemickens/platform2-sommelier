// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_DIAGNOSTICSD_FAKE_BROWSER_H_
#define DIAGNOSTICS_DIAGNOSTICSD_FAKE_BROWSER_H_

#include <string>

#include <base/macros.h>
#include <dbus/mock_exported_object.h>

#include "diagnostics/diagnosticsd/mojo_test_utils.h"
#include "mojo/diagnosticsd.mojom.h"

namespace diagnostics {

// Helper class that allows to test communication between
// browser and diagnostics daemon.
class FakeBrowser final {
 public:
  using DBusMethodCallCallback = dbus::ExportedObject::MethodCallCallback;

  using MojomDiagnosticsdServicePtr =
      mojo::InterfacePtr<chromeos::diagnostics::mojom::DiagnosticsdService>;

  FakeBrowser(MojomDiagnosticsdServicePtr* diagnosticsd_service_ptr,
              DBusMethodCallCallback bootstrap_mojo_connection_dbus_method);
  ~FakeBrowser();

  // Call the BootstrapMojoConnection D-Bus method. Returns whether the D-Bus
  // call returned success.
  // |mojo_fd| is the fake file descriptor.
  // |bootstrap_mojo_connection_dbus_method| is the callback that the tested
  // code exposed as the BootstrapMojoConnection D-Bus method.
  bool BootstrapMojoConnection(FakeMojoFdGenerator* fake_mojo_fd_generator);

  // Call the |SendUiMessageToDiagnosticsProcessorWithSize| Mojo method
  // on diagnosticsd daemon, which will call the |HandleMessageFromUi| gRPC
  // method on diagnostic processor.
  //
  // It simulates message sent from diagnostics UI extension to diagnostic
  // processor.
  //
  // Returns false when we were not able to copy |json_message| into shared
  // buffer.
  bool SendMessageToDiagnosticsProcessor(const std::string& json_message);

 private:
  MojomDiagnosticsdServicePtr* const diagnosticsd_service_ptr_;

  DBusMethodCallCallback bootstrap_mojo_connection_dbus_method_;

  DISALLOW_COPY_AND_ASSIGN(FakeBrowser);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_DIAGNOSTICSD_FAKE_BROWSER_H_
