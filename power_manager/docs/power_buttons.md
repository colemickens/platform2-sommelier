# Chrome OS Power Buttons

[TOC]

## User Experience

The intended power button behavior differs across Chrome OS devices.

### Convertible Chromebooks

Convertible Chromebooks have 360-degree hinges that allow them to be used in
both "clamshell" (i.e. traditional laptop) and tablet modes. As such, they
typically feature power buttons located on their sides that are accessible in
all configurations.

Regardless of whether a user is logged in or not, tapping the power button once
turns the screen off. If a user is logged in and the "Show lock screen when
waking from sleep" screen lock setting is enabled, the screen is additionally
locked. Tapping the power button a second time after the screen has been turned
off turns the screen back on. This is intended to give users an easy way to turn
the screen off before carrying the device while it's in tablet mode.

If the power button is held for two seconds or longer, the system shuts down.
This delay is 3.5 seconds if the screen was already turned off when the button
was held.

The power button behaves identically regardless of whether the device is in
tablet or clamshell mode.

### Clamshell-only Chromebooks

Clamshell-only Chromebooks have hinges that stop before reaching 180 degrees,
supporting only traditional laptop form factors. Their power buttons are
typically located at the top-right corner of the keyboard (i.e. to the right of
Volume Up and just above Backspace).

While a user is logged in and the screen is unlocked:

*   Holding the power button for less than 400 milliseconds does nothing
    (besides starting the lock animation). This is done in an attempt to ignore
    accidental presses.
*   Holding the power button for 400 milliseconds or longer locks the screen.
    Browser windows lift above the desktop and become transparent, the wallpaper
    is blurred and dimmed, and the lock screen appears.
*   Holding the power button for roughly two seconds or longer powers off the
    Chromebook. The screen fades to white before turning off.

When no user is logged in, the guest user is logged in, or the screen is already
locked, holding the power button for one second or longer powers off the
Chromebook.

If the display is off due to user inactivity or manually setting the screen
brightness to zero, the power button turns the display back on rather than
locking the screen or shutting down the system.

### Pixelbook (2017)

Although the Pixelbook is a convertible device with a side-mounted power button,
its power button behavior is closer to that of clamshell Chromebooks. The one
difference is that it turns its screen off just 3 seconds after it has been
locked using the power button (rather than the 40 seconds described in
[Inactivity Delays]) in order to make it easier to carry the device while it's
in tablet mode. See [bug 756601] and [bug 758574] for more details.

### Chromeboxes and Chromebases

Chromeboxes, Chromebases, and related devices that lack integrated keyboards
typically feature ACPI-style power buttons. Power button release events are not
reported properly to powerd, so the system instead takes action immediately when
the power button is pressed.

While a user is logged in and the screen is unlocked, pressing the power button
locks the screen. If the screen is already locked, no user is logged in, or a
guest user is logged in, pressing the power button shuts the system down.

### Chromebits

Chromebits don't have power buttons.

### Universal Behavior

Some power button behavior is consistent across all Chrome OS devices to date:

*   An initial press of the power button turns the system on.
*   Pressing the power button while the system is asleep wakes it.
*   Holding the power button for a full 8 seconds forces a hard power-off in
    firmware. This may result in data loss.

## Implementation

Various mechanisms are used to determine which behavior should be used:

*   By default, clamshell-style power button behavior is used.
*   Convertible-style behavior is enabled by setting the `touchview` USE flag in
    a Portage overlay to pass the `--enable-touchview` flag to Chrome, which
    tells `ash::PowerButtonController` that it should initialize
    `ash::TabletPowerButtonController` when an accelerometer event is received.
*   The Pixelbook behavior is enabled by passing the
    `--force-clamshell-power-button` flag to Chrome.
*   Chromebox- and Chromebase-style behavior is enabled by setting the
    `legacy_power_button` USE flag in a Portage overlay, which causes powerd's
    `legacy_power_button` pref to be set and the `--aura-legacy-power-button`
    flag to be passed to Chrome.

powerd receives power button events from the kernel's input subsystem and
reports them to Chrome via D-Bus as described in the [Input] document.

User-facing behavior power button behavior is spread across multiple classes in
Chrome:

*   `chromeos::PowerManagerClient` receives D-Bus notifications about power
    button events from powerd.
*   `ash::PowerButtonController` initiates action in response to power button
    events received from powerd on clamshell devices and on the Pixelbook.
*   `ash::TabletPowerButtonController` initiates action on convertible devices.
*   `ash::LockStateController` contains the high-level logic for transitioning
    between different animations and performing actions when they complete.
*   `ash::SessionStateAnimator` displays animations and contains durations.
*   `ash::PowerButtonDisplayController` contains logic related to forcing the
    display off in response to power button events.

[Inactivity Delays]: inactivity_delays.md
[bug 756601]: https://crbug.com/756601
[bug 758574]: https://crbug.com/758574
[Input]: input.md
