# Testing Shill

## Introduction to shill testing

We test shill using unit tests and integration tests. The unit tests are built
using [Google Test](https://github.com/google/googletest) and [Google
Mock](https://github.com/google/googletest/tree/master/googlemock). The
integration tests use [Tast] and [Autotest].

### Running unit tests for Chrome OS

Shill unit tests are run just like any other unit test in the platform2
directory. See the [platform2 unittest docs] for more information.

### Running integration tests

There are a variety of integration tests for shill, using either [Tast] or
Autotest. WiFi autotests mostly require a special test AP setup (see [Autotest
wificell documentation]).

-   Debugging test failures:
    -   Look at `/var/log/net.log`.
    -   `$ grep shill /var/log/messages` for log messages. Error messages from
        shill/wpa\_supplicant also get logged to /var/log/messages.
    -   `grep wpa_supplicant /var/log/messages` for supplicant log messages.
    -   `wpa_debug debug` to increase supplicant log level.
    -   `ff_debug --level -2` to increase shill log level to VERBOSE (levels go
        -1 to -4).
    -   `ff_debug +ethernet` adds scope `ethernet` to the list of scopes where
        logging is on.
    -   To examine Autotest log files:
        -   Check how far the test got before it failed:

            ```bash
            $ grep -a ': step ' <test output>/<suite name>/<suite name>.<test name>/debug/<suite name>.<test name>.INFO
            # Example
            (chroot)  grep -a ': step ' /tmp/test_that_latest/network_WiFiRoaming/network_WiFiRoaming.002Suspend/debug/network_WiFiRoaming.002Suspend.INFO
            ```

        -   Read the log file:

            ```bash
            (chroot)  LESSOPEN= less /tmp/test_that_latest/network_WiFiRoaming/network_WiFiRoaming.002Suspend/debug/network_WiFiRoaming.002Suspend.INFO
            ```

            `LESSOPEN=` prevents less from misinterpreting the logs as binary files, and piping them through `hexdump`.

[platform2 unittest docs]: https://chromium.googlesource.com/chromiumos/docs/+/master/platform2_primer.md#running-unit-tests
[Tast]: https://chromium.googlesource.com/chromiumos/platform/tast/
[Autotest]: https://dev.chromium.org/chromium-os/testing/autotest-developer-faq
[Autotest wificell documentation]: https://chromium.googlesource.com/chromiumos/third_party/autotest/+/master/docs/wificell.md
