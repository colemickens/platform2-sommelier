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

With the better ambient light sensor in Pixelbook, auto-brightness levels are
more finely-tuned and consist of 7 levels as follows:

|Situation          |Lux Step Down|Lux Step Up|Brightness UI|Brightness Linear|
|-------------------|------------:|----------:|------------:|----------------:|
|Low light          |   *N/A*     |      90   |   36.14%    |      10.75%     |
|Indoor - normal    |      40     |     250   |   47.62%    |      20.00%     |
|Indoor - bright    |     180     |     360   |   60.57%    |      34.00%     |
|Outdoor - dark     |     250     |     500   |   71.65%    |      49.00%     |
|Outdoor - overcast |     350     |    1700   |   85.83%    |      72.24%     |
|Outdoor - clear sky|    1100     |    7000   |   93.27%    |      86.25%     |
|Direct sunlight    |    5000     |   *N/A*   |  100.00%    |     100.00%     |

Also, as of M66 Pixelbook performs simple exponential smoothing to raw values
read from the ambient light sensor as a low-pass filter to remove noise from
the data to avoid adjusting the brightness too frequently in some lighting
conditions such as an overhead source of warm white LED lighting. [issue 826968]

A single user-configured brightness is tracked for both AC and battery power;
once the user has adjusted the brightness via the brightness keys, the
brightness remains at that level until the next time the system boots. (Prior to
M36, separate user-configured levels were maintained for AC and battery power --
see [issue 360042].) There are 16 user-selectable
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
is observed after the brightness has been manually set to 0%.

As of M35, Chromeboxes' brightness keys (or F6 and F7 keys) attempt to use
[DDC/CI] to increase or decrease external displays' brightness ([issue 315371]).

[issue 360042]: https://crbug.com/360042
[DDC/CI]: https://en.wikipedia.org/wiki/Display_Data_Channel#DDC.2FCI
[issue 315371]: https://crbug.com/315371
[issue 826968]: https://crbug.com/826968
