# g2ftool : U2F command line interface tool

g2ftool is a command line tool to interact with U2FHID devices, whether
physical devices or the virtual one provided by u2fd.

## Usage

The tool can send basic commands such as `ping`, raw messages, as well as
more complex commands such as register and authenticate.

For all commands, a device must be specified, this will typically be
something like `/dev/hidraw3`.

### Basic Command Examples

To send a U2F_PING command:

```
g2ftool --dev=/dev/hidraw<n> --ping
```

You may like to increase verbosity to see details of the messages sent:

```
g2ftool --dev=/dev/hidraw<n> --v=3 --ping
```
