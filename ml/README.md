# CrosML: Chrome OS Machine Learning Service

## Summary

The machine learing service provides a common runtime for evaluating machine
learning models on device.  The service wraps the TensorFlow runtime which has
been optimized to support the set of built-in machine learning models with are
installed on rootfs.  See [go/chromeos-ml-service] for a design overview.

## Mojo interface bootstrapped over D-Bus

The Mojo connections are bootstrapped by passing the file descriptor for the
IPC pipe over D-Bus.  The life-cycles for the loaded machine learning models
and the runtime eval session are matched to the life-cycles of the Mojo
interfaces.  See [go/chromeos-ml-service-mojo] for a description of the
Mojo interfaces and [go/chromeos-ml-service-impl] for details on bootstrapping
the connection over D-Bus.

[go/chromeos-ml-service]: http://go/chromeos-ml-service
[go/chromeos-ml-service-mojo]: http://go/chromeos-ml-service-mojo
[go/chromeos-ml-service-impl]: http://go/chromeos-ml-service-impl
