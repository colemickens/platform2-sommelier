# Passing Chrome flags from `session_manager`

Chrome sometimes needs to behave differently on different Chrome OS devices.
It's preferable to test for the hardware features that you care about directly
within Chrome: if you want to do something special on Chromebooks that have
accelerometers, just check if an accelerometer device is present.

Sometimes it's not possible to check for these features from within Chrome,
though. In that case, the recommended approach is to add a command-line flag to
Chrome and update `session_manager` to pass it with the appropriate value (if
any).

Chrome's command line is constructed by [chrome_setup.cc]. This file uses the
[ChromiumCommandBuilder] class from `libchromeos-ui` to create directories
needed by Chrome, configure its environment, and build its command line.

`ChromiumCommandBuilder` reads a subset of the Portage USE flags that were set
when the system was built from `/etc/ui_use_flags.txt`; these can be used to
determine which flags should be passed. To start using a new USE flag (including
a board name), add it to the [libchromeos-use-flags] ebuild file. (Relegating
this file to a tiny dedicated package allows us to use the same prebuilt
`chromeos-chrome` and `chromeos-login` packages on devices that have different
sets of USE flags.)

Configuration that would apply both to the Chrome browser and to other products
that could be built using the Chromium codebase (e.g. a simple shell that runs a
dedicated web app) should be placed in `ChromiumCommandBuilder`. This includes
most compositor- and audio-related flags.

Configuration that is specific to the Chrome browser should instead be placed in
[chrome_setup.cc]. This includes most flags that are implemented within Chrome's
`//ash` and `//chrome` directories.

## Making quick changes

[/etc/chrome_dev.conf] can be modified on dev-mode Chrome OS systems (after
making the root partition writable) to add or remove flags from Chrome's command
line or modify Chrome's environment. The file contains documentation about its
format.

[chrome_setup.cc]: ../chrome_setup.cc
[ChromiumCommandBuilder]: https://chromium.googlesource.com/chromiumos/platform2/+/master/libchromeos-ui/chromeos/ui/chromium_command_builder.h
[libchromeos-use-flags]: https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/+/master/chromeos-base/libchromeos-use-flags/libchromeos-use-flags-0.0.1.ebuild
[/etc/chrome_dev.conf]: ../chrome_dev.conf
