# diagnosticsd daemon

Please see ../README.md for general information.

## IPC mechanisms

This daemon uses three IPC mechanisms:

* gRPC - for talking to the `diagnostics_processor` daemon.
* Mojo - for talking to the browser.
* D-Bus - for receiving Mojo bootstrap requests from the browser.

## Class structure

    `DiagnosticsdDaemon`
     ^
     |
     v
    `DiagnosticsdCore`
     ^
     |
     |   // gRPC-related members:
     +-> `DiagnosticsdGrpcService`
     |       (handles incoming gRPC requests)
     +-> `AsyncGrpcServer<grpc_api::Diagnosticsd>`
     |       (connects `DiagnosticsdGrpcService` with the actual gRPC
     |        pipe)
     +-> `AsyncGrpcClient<grpc_api::DiagnosticsProcessor>`
     |       (sends outgoing gRPC requests through the actual gRPC pipe)
     |
     |   // Mojo-related members:
     +-> `DiagnosticsdMojoService`
     |       (handles incoming Mojo requests and sends outgoing ones)
     +-> `mojo::Binding<mojom::DiagnosticsdService>`
     |       (connects `DiagnosticsdMojoService` with the actual Mojo pipe)
     |
     |   // D-Bus-related members:
     +-> `DiagnosticsdDBusService`
     |       (handles incoming D-Bus requests)
     +-> `brillo::dbus_utils::DBusObject`
             (connects `DiagnosticsdDBusService` with the actual D-Bus pipe)

Classes are generally organized such that they don't know about their owners or
siblings in this graph. Instead, these classes are parameterized with
delegate(s), which implement these cross-class calls.
This allows to test each individual piece of logic separately.
