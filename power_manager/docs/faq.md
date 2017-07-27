# Chrome OS Power Management FAQ

## How do I change power management timeouts for development or testing?

There are several different techniques that can be used to temporarily override
the power manager's default behavior:

### `set_power_policy`

This utility program was added in R26 to exercise the code path that Chrome uses
to override the default power management policy (which was needed for
enterprise). Run it with `--help` to see the available fields or without any
arguments to restore the default policy. Note that Chrome may override any
policy that you manually set; this happens when the related Chrome preferences
are changed or when Chrome is restarted, for instance.

### powerd preferences

powerd's default preferences are stored on the read-only partition in
`/usr/share/power_manager` and `/usr/share/power_manager/board_specific`. These
preferences can be overridden by identically-named files on the stateful
partition in `/var/lib/power_manager`. In most cases, preference changes won't
take effect until the powerd process is restarted. The
`set_short_powerd_timeouts` script can be used to quickly set inactivity
timeouts to low values and restart powerd. To disable the power manager's
timeouts more permanently, run the following as root:

``` sh
echo 1 >/var/lib/power_manager/ignore_external_policy
for i in {,un}plugged_{dim,off,suspend}_ms; do
  echo 0 >/var/lib/power_manager/$i
done
restart powerd
```

To revert to the normal behavior, remove the files that you just created from
`/var/lib/power_manager` and restart the powerd job again.

To temporarily change prefs in an autotest, use `[PowerPrefChanger]`.

## How do I trigger a suspend manually?

The `powerd_dbus_suspend` program can be run from crosh or an SSH session to
exercise the normal suspend path; it sends a D-Bus message to powerd asking it
to suspend the system. See also `memory_suspend_test` and `suspend_stress_test`.

## How do I disable automatic sleep via a Chrome Extension?

The [Keep Awake extension] can be used to prevent the system from automatically
sleeping in response to user inactivity.

See the [chrome.power API] for more details.

## How do I make my code run before the system suspends or after it resumes?

The power manager gives other daemons an opportunity to do any preparation that
they need to just before the system is suspended. See [suspend.proto] for a
detailed description of the process, along with the definitions of the protocol
buffers that are passed over D-Bus, and [suspend_delay_sample] for example
usage.

[PowerPrefChanger]: https://chromium.googlesource.com/chromiumos/third_party/autotest/+/master/client/cros/power_utils.py
[suspend.proto]: https://chromium.googlesource.com/chromiumos/platform/system_api/+/master/dbus/power_manager/suspend.proto
[suspend_delay_sample]: https://chromium.googlesource.com/chromiumos/platform2/+/master/power_manager/tools/suspend_delay_sample.cc
[Keep Awake extension]: https://chrome.google.com/webstore/detail/keep-awake-extension/bijihlabcfdnabacffofojgmehjdielb
[chrome.power API]: https://developer.chrome.com/extensions/power
