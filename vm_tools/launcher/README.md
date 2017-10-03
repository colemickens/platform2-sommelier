# `vm_launcher`

## Overview
`vm_launcher` is a frontend to crosvm that simplifies launching a VM and
managing its lifecycle. This includes allocating resources that must be
handled at a higher level than the VM hypervisor, such as IPv4 address
allocation.

## Usage
Each VM must be given a name when started. Each VM will have a runtime
directory under `/run/vm/<name>` which contains the specific resources
it has allocated, as well as the control socket for that crosvm instance.

`vm_launcher` uses the following subcommands:

* `run`     - Run a VM with serial console in the foreground.
* `start`   - Start a headless VM in the background.
* `stop`    - Stop a running VM and clean up any resources.
* `getname` - Get the VM name associated with a pid.
* `help`    - Get `vm_launcher` usage help.

For more detail, run `vm_launcher help`
