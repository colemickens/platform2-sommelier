# Pid 1 (init)

`maitred` provides init-like functionality for processes inside the VM.

## Early setup
`maitred` performs some early setup before it begins listening for rpcs from the
host.  This includes mounting various filesystems (like `proc`, `sysfs`, and
`cgroups`).  Additionally `maitred` mounts a `tmpfs` on the `/tmp` and `/run`
directories so that applications can have temporary runtime storage.

## Launching processes

New processes are spawned by sending `maitred` a `LaunchProcess` rpc.  This rpc
takes a `LaunchProcessRequest` message as its argument, which can be found in
the [guest.proto](../proto/guest.proto) file.

`maitred` will then follow the lifetime of this process until it exits or is
killed by a signal.  If the `LaunchProcessRequest` message indicated that the
process should be respawned, then `maitred` will launch a new instance of that
process.  However, processes that respawn more than 10 times in 30 seconds will
be stopped.  These processes can only be restarted by sending another
`LaunchProcess` rpc.

### Process Privileges

Processes launched by `maitred` run as root with full privileges.  If the sender
of the `LaunchProcess` rpc does not want that process to have full root access,
then they should ensure that the program either uses `libminijail` to drop
privileges or launch the program using `minijail0` with the appropriate flags.

## Shutting down

When `maitred` receives a `Shutdown` rpc, it sends a `SIGTERM` signal to all
processes running on the VM.  After 5 seconds it terminates any remaining
processes by sending them a `SIGKILL` signal.

`maitred` then shuts down the system by issuing a `reboot` system call.

### Cleaning up during shutdown

Some processes may wish to perform some clean up before the system is shut down.
For example `vm_syslog` will want to flush any buffered logs before shut down.
These processes should catch the SIGTERM signal sent out by `maitred`, perform
any clean up, and then exit.
