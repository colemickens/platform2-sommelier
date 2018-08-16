# Device Telemetry and Diagnostics

This implements daemons and libraries providing device telemetry and
diagnostics.

## diagnosticsd

The daemon that collects telemetry information and exposes APIs that
allow to access it. This daemon also acts as a proxy to the more heavily
isolated `diagnostics_processor` daemon.

## diagnostics_processor

This daemon will process the telemetry information provided by the
`diagnosticsd` daemon. Exposes an API that allows to obtain the output
of the telemetry processing.

## APIs between diagnosticsd and browser

The bidirectional API between `diagnosticsd` and the browser is based on
Mojo. The bootstrapping of the Mojo connection is performed via D-Bus -
specifically, by the browser calling the BootstrapMojoConnection method.
