# Testing Shill

## Introduction to shill testing

We test shill using unit tests and integration tests. The unit tests are built
using [Google Test]( http://code.google.com/p/googletest/)
and [Google Mock](http://code.google.com/p/googlemock/). The integration tests
use [autotest](http://autotest.kernel.org/), also explained at
[the chromium.org autotest page](http://www.chromium.org/chromium-os/testing/autotest-developer-faq).

### Running unit tests for Chrome OS

Here `${BOARD}` is a valid board name, like octopus, scarlet or x86-generic.

- Build the shill_unittest target.

  ```bash
  (chroot) $cros_workon_start --board ${BOARD} chromeos-base/shill
  # The below only builds shill code without tests
  (chroot) $cros_workon_make --board ${BOARD} chromeos-base/shill
  # The below builds shill code with tests and runs the unit-tests
  (chroot) $cros_workon_make --board ${BOARD} chromeos-base/shill
  ```

- One can also do the above with emerge.

  ```bash
  (chroot) $cros_workon_start --board ${BOARD} chromeos-base/shill
  (chroot)$ FEATURES=test emerge-${BOARD} shill
  ```

- One can run the unit tests from your host machine under gdb.

  ```bash
  (chroot)$ FEATURES=test emerge-${BOARD} shill
  (chroot)$ gdb_x86_local --board ${BOARD} /build/${BOARD}/var/cache/portage/chromeos-base/platform2/out/Default/shill_unittest
  ```

- The emerge workflow given above is incremental. It uses ninja to rebuild only
  relevant objects in the shill target.

- You can restrict the test runs to only shill unittests by using:

  ```bash
  (chroot)$ P2_TEST_FILTER="shill::WiFi*" FEATURES=test emerge-${BOARD} shill
  ```

  The filter can be made more specific to include googletest filters like
  `shill::CellularTest.StartGSMRegister`.

- If you want to set a breakpoint in gdb, make sure to include the shill
  namespace. i.e. Below is the correct way:

  ```bash
  (cros-gdb) b shill::EthernetService::GetStorageIdentifier
  Breakpoint 2 at 0x5555563cc270: file ethernet_service.cc, line 63.
  ```

  The below is incorrect:

  ```bash
  (cros-gdb) b EthernetService::GetStorageIdentifier
  Function "EthernetService::GetStorageIdentifier" not defined.
  Make breakpoint pending on future shared library load? (y or [n]) n
  ```

- Alternate build command:
  - To see the actual compiler commands that are run:

    ```bash
    (chroot)$ CFLAGS="-print-cmdline" cros_workon_make --reconf --board=${BOARD} shill
    ```

- To abort compilation on the first error:

  ```bash
  (chroot)$ MAKEFLAGS="--stop" cros_workon_make --test --board=${BOARD} --reconf shill
  ```

### Running unit tests for Chrome OS with the address sanitizer

```bash
(chroot) USE="asan clang wimax" TEST_FILTER="shill::*" emerge-${BOARD} shill
```

This also turns on `wimax` and its tests, since this is disabled on most platforms.

### Running integration tests

- Build a test image, suitable for a VM.

  ```bash
  (chroot) src/scripts$ ./build_packages --board=${BOARD}
  (chroot) src/scripts$ ./build_image --board=${BOARD} --noenable_rootfs_verification test
  (chroot) src/scripts$ ./image_to_vm.sh --board=${BOARD} --test_image
  ```

- Start the VM.

  ```bash
  (host)$ sudo kvm -m 2048 -vga std -pidfile /tmp/kvm.pid \
              -net nic,model=virtio -net user,hostfwd=tcp::9222-:22 \
              -hda <path to chroot>/src/build/images/${BOARD}/latest/chromiumos_qemu_image.bin
  ```

- DO NOT log-in on the console. Doing so will load a user profile onto shill's profile stack; this
  interferes with the test profile that we use in the autotests.

- If you have modified the source after building the image, update shill on the image, and then restart
  shill:

  ```bash
  (chroot) src/scripts$ ./start_devserver
  (chroot) src/scripts$ ssh-keygen -R '[127.0.0.1]:9222'
  (chroot) src/scripts$ emerge-${BOARD} platform2
  (chroot) src/scripts$ cros deploy 127.0.0.1:9222 platform2

  (chroot) src/scripts$ ssh -p 9222 root@127.0.0.1
  localhost / # restart shill
  ```

- Run the tests:

  ```bash
  (chroot) src/scripts$ test_that 127.0.0.1 WiFiManager
                                  --args="config_file=wifi_vm_config"
                                  --ssh_options="-p 9222"
  ```

  To run a specific test out of the test suite, use `test_pat` option to `--args`.

  ```bash
  # Example: To just run the 035CheckWEPKeySyntax test:
  (chroot) src/scripts$ test_that 127.0.0.1 WiFiManager
                                  --args="config_file=wifi_vm_config test_pat=035CheckWEPKeySyntax"
                                  --ssh_options="-p 9222"
  ```

- Configuration note: if you use a different port (e.g.`hostfwd=tcp::9223-:22`),
  you'll need to change the `ssh_port` argument to `test_that` and the port
  numbers in `<chroot>/third_party/autotest/files/client/config/wifi_vm_config`.

- Debugging test failures:
  - Look  at `/var/log/net.log`.
  - `$ grep shill /var/log/messages` for log messages. Error messages from
     shill/wpa\_supplicant also get logged to /var/log/messages.
  - `grep wpa_supplicant /var/log/messages` for supplicant log messages.
  - `wpa_debug debug` to increase supplicant log level.
  - `ff_debug --level -2` to increase shill log level to VERBOSE (levels go -1 to -4).
  - `ff_debug +ethernet` adds scope `ethernet` to the list of scopes where logging is on.
  - Try resetting the test infrastructure:

    ```bash
    modprobe -r mac80211_hwsim mac80211 cfg80211
    restart wpasupplicant
    rm /tmp/hostapd-test.control/*
    ```

  - To examine autotest log files:
    - Check how far the test got before it failed:

      ```bash
      $ grep -a ': step ' <test output>/<suite name>/<suite name>.<test name>/debug/<suite name>.<test name>.INFO
      # Example
      (chroot) $ grep -a ': step ' /tmp/test_that_latest/network_WiFiRoaming/network_WiFiRoaming.002Suspend/debug/network_WiFiRoaming.002Suspend.INFO
      ```

    - Read the log file:

      ```bash
      (chroot) $ LESSOPEN= less /tmp/test_that_latest/network_WiFiRoaming/network_WiFiRoaming.002Suspend/debug/network_WiFiRoaming.002Suspend.INFO
      ```

      `LESSOPEN=` prevents less from misinterpreting the logs as binary files, and piping them through `hexdump`.

- Additional test suites: We have a number of other WiFi test suites (in addition to WiFiManager).
  These are: WiFiMatFunc, WiFiPerf, WiFiRoaming, WiFiSecMat.
  The WiFiPerf tests are probably not too relevant to shill (just yet), but the rest probably are.
