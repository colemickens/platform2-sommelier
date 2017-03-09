# Chrome OS Screen Brightness Behavior

At boot, the panel backlight's brightness is set to 40% (computed linearly) of
its maximum level by the `boot-splash` Upstart job. This happens before the boot
splash animation is displayed by frecon.

After powerd starts, it chooses an initial backlight brightness based on the
power source (either AC or battery) and the ambient light level (either very
bright, i.e. direct sunlight, or something dimmer than that). Note that the
brightness percentages appearing in powerd's log are not computed linearly (see
`policy::InternalBacklightController::PercentToLevel()` for the implementation).
Given a hardware-specific minimum-visible level and maximum level, the mapping
from a percentage to a hardware level is defined as:

```
fraction = (percent - 6.25) / (100 - 6.25)
level = min_visible_level + fraction² * (max_level - min_visible_level)
```

The automatically-chosen levels are as follows:

| Power source | Direct sunlight (>= 400 lux) | Normal ambient light |
|--------------|------------------------------|----------------------|
| AC           | 100% (100% linear)           | 80% (~62% linear)    |
| Battery      | 80% (~62% linear)            | 63% (~37% linear)    |

Devices that lack ambient light sensors just use the "normal ambient light"
levels listed above. Note that these levels may be set differently for different
devices.

In the past, powerd made continuous adjustments to the screen brightness based
on the ambient light level. This was distracting to users and also generally
ineffective: the majority of indoor environments occupy the bottom end of the
range reported by our ambient light sensors. Due to the coarse readings within
this range, the automatically-chosen brightness levels were frequently
undesirable. We decided to switch to just two levels: one that would work well
in most indoor environments, and a very-bright level for outdoor environments.

When the power source or ambient light level changes, powerd transitions to the
new brightness level. Before the user has touched a brightness key, the
brightness will update automatically between when AC power is connected or
disconnected. When the user presses the brightness-up or brightness-down keys,
powerd animates to the requested level and stops making further
ambient-light-triggered or power-source-triggered automated adjustments until
the system is rebooted.

A single user-configured brightness is tracked for both AC and battery power;
once the user has adjusted the brightness via the brightness keys, the
brightness remains at that level until the next time the system boots. (Prior to
M36, separate user-configured levels were maintained for AC and battery power --
see [issue 360042](https://crbug.com/360042).) There are 16 user-selectable
brightness steps, divided evenly between the full non-linear percentage-based
range (i.e. each button press moves the brightness by 100 / 16 = 6.25%). The
brightness popup that appears when a button is pressed actually contains a
draggable slider that can be used to select a brightness percentage that doesn't
match one of the pre-defined steps.

In the past, the previous user-configured level was restored at boot, but this
was deliberately removed. A given device is frequently used in many different
environments: dark rooms, well-lit rooms with lots of ambient light, etc. We
decided that always booting with a reasonable default brightness was preferable
to sometimes restoring a blindingly-high brightness when booting in a dark room
or restoring an extremely-dim brightness when booting in a bright room.

When the user is inactive for an extended period of time, the screen is dimmed
to 10% of its maximum level (computed linearly) and then turned off. The screen
is turned back on in response to user activity (which is interpreted broadly:
keyboard or touchpad activity, power source change, external display being
connected or disconnected, etc.).

Users may reduce the backlight brightness to 0% using the brightness-down (F6)
key; this may be desirable to conserve battery power while streaming music. The
backlight is automatically increased to a low-but-visible level when user input
is observed after the brightness has been manually set to 0%. Brightness and
volume key events do not trigger this auto-increasing behavior, so that users
can adjust the volume while keeping the screen off and also e.g. not see the
screen turn back on if brightness-down is pressed while the screen is off.

As of M35, Chromeboxes' brightness keys (or F6 and F7 keys) attempt to use
[DDC/CI](https://en.wikipedia.org/wiki/Display_Data_Channel#DDC.2FCI) to
increase or decrease external displays' brightness ([issue
315371](https://crbug.com/315371)).
