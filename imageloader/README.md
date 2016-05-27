# src/platform/imageloader

This aims to provide a generic utility to load (mount) and unload (unmount)
verified disk images through DBus IPC.

# Binaries

* `imageloader`
* `imageloadclient`

`imageloader` can be run as root and can handle mounting and unmounting of
disk images. `imageloadclient` is a simple client (intended to be run as
chronos) that can talk to `imageloader` and ask it to mount and unmount stuff.
When `imageloader` is not running, DBus can invoke it via the one time
run option (`imageloader -o`) and get the task done.
