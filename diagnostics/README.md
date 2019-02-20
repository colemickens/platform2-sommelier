# Device Telemetry and Diagnostics

This implements daemons and libraries providing device telemetry and
diagnostics.

## wilco_dtc_supportd

The daemon that collects telemetry information and exposes APIs that allow to
access it. This daemon also acts as a proxy to the more heavily isolated
`wilco_dtc` daemon (Wilco DTC - wilco diagnostics and telemetry controller).

## wilco_dtc

This daemon will process the telemetry information provided by the
`wilco_dtc_supportd` daemon. Exposes an API that allows to obtain the output of
the telemetry processing.

## APIs between wilco_dtc_supportd and browser

The bidirectional API between `wilco_dtc_supportd` and the browser is based on
Mojo. The bootstrapping of the Mojo connection is performed via D-Bus -
specifically, by the browser calling the BootstrapMojoConnection method.

## APIs between wilco_dtc_supportd and wilco_dtc

The bidirectional API between `wilco_dtc_supportd` and `wilco_dtc` is based on
gRPC running over Unix domain sockets.
