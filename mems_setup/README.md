# Chrome OS MEMS Setup Code

## `/usr/sbin/mems_setup`

This tool handles the boot-time setup of cros_ec sensors on the device, such as
accelerometers, gyroscopes, and potentially other sensors in the future.

The goals of introducing this tool vs. using a shell script include:
 - improved testability;
 - more readable;
 - better performance.

This tool is based on libiio, and exposes a small wrapper on top of the library
to aid in writing test cases.

## Class hierarchy

At the root of the hierarchy, there exists the `IioContext`, which represents
the IIO devices currently available on the system. These can be retrieved by
name and inspected, via instances of `IioDevice`.

An `IioDevice` allows reading and writing attributes of an IIO device via
type-safe helper APIs. It also offers support for configuring the buffer
and trigger of an IIO device, which we use in order to allow the Chrome UI
to read accelerometer data and support screen rotation.

An `IioDevice` also exposes a list of `IioChannel`s, which can individually be
enabled and disabled.

## Configuration

Currently, the actions taken by the tool are statically determined by
the type of sensor being initialized, and the only values that are discovered
at runtime are the VPD calibrations. If your use case needs tweaking the
behavior of this tool in other ways, please file a bug.
