[TOC]

# vm_tools - Utilities for Virtual Machine (VM) orchestration

This directory contains various tools for managing the lifetime of VM instances
and for providing any services those VMs may need while they are running.

## vm_launcher

`vm_launcher` is responsible for starting up a new VM instance.  It first
allocates some pooled resources for the VM (like a mac address, IPv4 subnet,
unique identifier, and virtual socket host id) and then invokes the userspace
hypervisor (`crosvm` or `lkvm`) to actually start running the VM.

## maitred

`maitred` is the agent running inside the VM responsible for managing
the VM instance.  It acts as the init system, starting up system services,
mounting file systems, and launching the container with the actual application
that the user wants to run.  It is responsible for shutting down the VM once the
user's application exits or if requested by a process on the host.

See [docs/init.md](docs/init.md) for more details on the duties maitred carries
out as pid 1.

## vm_syslog

`vm_syslog` is the syslog daemon that runs inside the VM.  It is automatically
started by maitred and provides a socket at `/dev/log` for applications to send
it log records.  `vm_syslog` aggregates the log records and then forwards them
outside the VM to the logging service provided by `vm_launcher`.  `vm_launcher`
then tags these records with the unique identifier assigned to that VM instance
before forwarding the logs to the syslog daemon.

## crash_collector

`crash_collector` is responsible for collecting crash reports of applications
running inside the VM and forwarding them out to the crash collector service
provided by `vm_launcher`.  When `maitred` first starts up it configures
`/proc/sys/kernel/core_pattern` to start the `crash_collector` program and send
the core dump over a pipe to that program.  `crash_collector` then parses the
core dump and converts it to a minidump before sending it out to `vm_launcher`.
`vm_launcher` passes the report on to `crash-reporter`, which takes care of
uploading it to Google servers.

## VM <-> host communication

All communication between `vm_launcher` and the applications inside the VM
happen over a [vsock](https://lwn.net/Articles/695981/) transport. The actual
RPC communication uses the [gRPC](http://grpc.io) framework. Every `maitred`
instance listens on a known port in the vsock namespace (port 8888).
`vm_launcher` instances allocate a port number via a shared pool and communicate
that port number to the VM via the linux kernel command line.

### Authentication

Since each `maitred` instance listens on a known port number and vsock is a
global namespace it would be possible for a malicious application in one VM to
talk to the `maitred` instance in another VM and attempt to exploit it.  To deal
with this maitred authenticates every call it receives to make sure that it is
coming from a trusted source.  `vm_launcher` generates an access token and
forwards it to maitred on the VM kernel command line.  Only requests that include
this access token are accepted and processed by `maitred`.  TODO(chirantan): Work
with chromeos-security to flesh out the details so everyone is happy with this
solution.

### Wire format

gRPC uses [protocol buffers](https://developers.google.com/protocol-buffers) as
the serialization format for messages sent over the vsock transport.  The
[proto](proto/) directory holds the definitions for all the messages sent and
services provided between the host and the VM.
