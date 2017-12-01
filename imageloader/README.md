# imageloader

This aims to provide a generic utility to verify and load (mount) signed disk
images through DBUS IPC.

# Binaries

* `imageloader`

`imageloader` handles the mounting of disk images. `imageloader` should
be executed via the `imageloader_wrapper` script, which ensures that
imageloader's storage exists and is owned by `imageloaderd` user.
When `imageloader` is not running, DBus will automatically invoke it. After 20
seconds of inactivity, the service exits.
