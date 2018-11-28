# Overview

Runtime Probe is essentially a command line tool that consumes the
[probe syntax](https://chromium.googlesource.com/chromiumos/platform/factory/+/master/py/probe/README.md#detail-usage-the-syntax-of-a-probe-config-file)
and outputs the
[probe result](https://chromium.googlesource.com/chromiumos/platform/factory/+/master/py/probe/README.md#output-format).

This command line tool will gradually replace fields in HWID (i.e. less fields
will be encoded into HWID) as it reflects the status of a device in a more
timely approach.

Currently, the reported data are purely hardware information. For example, the
model of storage, the remaining capacity of battery, the screen resolution..etc.

To serve clients that are not able to call command line directly, we offer
a simple, restricted DBus interface with only one method (with `minijail0`,
`seccomp` policy and `dbus/org.chromium.RuntimeProbe.conf`). As this CLI tool
poses itself as a one-shot call tool, just like `ping` command, the DBus
interface is on-demand and expected to exit after one call. There is a long-term
solution effort in parallel. Once that is ready, we might turn off the DBus
interface, this is tracked in [b/120826476].

# Motivation

To better reflect hardware configuration on users’ system, we need a tool in
release image that is able to reflect the truth of the moment in the field.

Traditionally, this need has been satisfied by
[factory's probe framework](https://chromium.googlesource.com/chromiumos/platform/factory/+/master/py/probe/README.md)
, however, we have seen more and more requests from partners to switch component
after devices left factory process.

It is related to the evolution of HWID. However, instead of going into details
on the complicated plan, we would like to use an example to let the reader get
a high level concept.

Let's look into a typical scenario here: After years' love into the Chromebook,
a user has one of the DRAM broken in slots, and would like to replace it.
However, the original part is already EOL (End Of Life), ODM has no choice but
to pick another part from the
[AVL ( Approved Vendor List )](https://www.google.com/chromeos/partner/fe/#avl)
. While this new DRAM is installed, the original probe result in factory is
violated. Hence, even after factory process, we would like to get the probe
result to reflect the truth of the moment under release image.
Runtime is used to convey the concept of dynamic.

# Usage
The code is still underdevelopment, will update when it is ready for use.

[b/120826476]: https://issuetracker.google.com/120826476
