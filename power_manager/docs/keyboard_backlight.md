# Chrome OS Keyboard Backlight Behavior

On devices that possess backlit keyboards, powerd is responsible for adjusting
the backlight brightness.

Powerd reads raw percentages from `keyboard_backlight_user_steps` preference,
scales the first step as 0%, second step as 10% and last step as 100%, and
calculates the rest of the scaled percentages linearly.

If an ambient light sensor is present, powerd uses its readings to determine the
keyboard backlight brightness level. In a well-lit environment, the backlight is
turned off. In a dark environment, the backlight is turned on at a moderate
level (pursuant to user activity, as described below). The ambient light ranges
and corresponding backlight brightness percentages are read from the
`keyboard_backlight_als_steps` preference. The percentages in this preference
should be scaled percentages.

If no ambient light sensor is present, powerd reads a single brightness
percentage from the `keyboard_backlight_no_als_brightness` preference and uses
that instead when the backlight is turned on. The percentage in this preference
should be scaled percentage.

When fullscreen video is detected, powerd forces the keyboard backlight off so
as to not distract the user. The keyboard backlight is also dimmed or turned off
in lockstep with the display backlight in response to user inactivity or the
system suspending or shutting down.

If the device's touchpad is capable of detecting when the user's hands are
hovering over it, powerd turns the backlight on when hovering is detected and
turns the backlight off soon after hovering ceases (the duration is supplied by
the `keyboard_backlight_keep_on_ms` preference, which defaults to 30 seconds).
Additionally, hovering forces the keyboard backlight on even while fullscreen
video is detected.

If the device is incapable of detecting hovering, the backlight is turned on
only in response to user activity (e.g. keyboard or touchpad events) instead.
This is controlled by the `keyboard_backlight_turn_on_for_user_activity`
preference, enabled by default.

The user is able to adjust the keyboard backlight brightness by holding Alt
while pressing the Brightness Up or Brightness Down keys. The brightness moves
between the raw percentage steps in the `keyboard_backlight_user_steps`
preference. On the UI, the keyboard backlight brightness controller bar moves
between the scaled percentage steps. Once the user has manually adjusted the
brightness, powerd refrains from making any automated adjustments until the
system reboots.
