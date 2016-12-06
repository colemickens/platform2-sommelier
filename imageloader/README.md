# src/platform/imageloader

This aims to provide a generic utility to verify and load (mount) signed disk
images through DBUS IPC.

# Binaries

* `imageloader`
* `imageloadclient`

`imageloader` handles the mounting of disk images. `imageloader` should
be executed via the `imageloader_wrapper` script, which applies a minijail
sandbox to the `imageloader` binary, and runs it as an unprivileged user.
When `imageloader` is not running, DBus can invoke it via the one time
run option (`imageloader -o`) and get the task done.

`imageloadclient` is a simple client (intended to be run as
chronos) that can talk to `imageloader` and ask it to mount images. It is not
installed by default as it is for testing and debugging only.
