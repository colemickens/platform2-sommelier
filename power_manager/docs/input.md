# Chrome OS Power Manager Input

powerd uses user input to determine when the system should turn its backlight
off or suspend in response to inactivity. powerd does not listen for this input
directly; rather, it receives periodic `HandleUserActivity` D-Bus method calls
from Chrome while the user is active. These method calls include a
[UserActivityType] enum describing the type of activity that was observed,
allowing powerd to e.g. avoid turning the screen back on if the user presses the
Brightness Down key while the screen is already off.

`powerd/system/input.cc` uses the kernel's input subsystem to observe power
button and lid switch events. (ACPI power button events may be additionally
received by Chrome as standard keyboard input, but they are ignored there since
button releases are not reported correctly.) These events are reported to Chrome
via `InputEvent` D-Bus signals containing [InputEvent] protocol buffers; Chrome
uses the power button notifications to display screen-lock and shutdown
animations.

## Power button behavior

The behavior of the power button is largely controlled by software and may
change, but the general expectations for devices with keyboard-integrated power
buttons (i.e. most Chromebooks) are:

-   An initial press of the power button turns the system on.
-   Holding the power button at the login screen or in guest mode begins a
    brief, interruptible-by-releasing-the-button animation where the screen
    fades to white, after which the system is shut down.
-   Holding the power button while signed in starts a screen-lock animation and
    then (if the button is still held) shuts the system down.
-   Holding the power button while the screen is locked shuts the system down.
-   Tapping the power button while the system is suspended wakes it.
-   If the display is off due to user inactivity or manually setting the screen
    brightness to zero, the power button turns the display back on rather than
    locking the screen or shutting down the system.

The behavior of tablet-style power buttons on the sides of convertible
Chromebooks is slightly different:

-   Tapping the power button turns the display on or off.
-   Holding the power button shuts the system down.

Devices with power buttons that are separate from their keyboards (e.g.
Chromeboxes and Chromebases) generally behave similarly but to Chromebooks but
lack the interactive animations described above: just tapping the power button
locks or shuts down the system.

[UserActivityType]: https://chromium.googlesource.com/chromiumos/platform/system_api/+/master/dbus/power_manager/dbus-constants.h
[InputEvent]: https://chromium.googlesource.com/chromiumos/platform/system_api/+/master/dbus/power_manager/input_event.proto
