# arc-codec-test: ARC++ Video Codec End-to-end Test

## Manually run the test

You need to be running the Android container on your Chromebook to run the
tests. Make sure the device under test (DUT) can be connected adb.

1.  Connect to the DUT via adb
    ```
    (Outside CrOS chroot)
    $ adb connect <chromebook_ip>:22
    ```

2.  Build the e2e test
    ```
    (In CrOS chroot)
    $ emerge-<BOARD> arc-codec-test
    ```

3.  Push the binary to DUT
    ```
    (Outside CrOS chroot)
    $ adb push \
      <CHROMIUMOS_ROOT>/chroot/build/<BOARD>/opt/google/containers/android/vendor/bin/arcvideoencoder_test_<abi> \
      /data/local/tmp/
    ```

    Hint: The path of the binary can be found by:
    ```
    (In CrOS chroot)
    $ equery-<BOARD> files arc-codec-test
    ````

4.  Push the test stream to DUT
    ```
    $ adb push test-320x180.yuv /data/local/tmp/
    ```

5.  Run the test binary
    ```
    $ adb shell "cd /data/local/tmp; \
                 ./arcvideoencoder_test_<abi> --test_stream_data=test-320x180.yuv:320:180:1:out1.h264:100000:30"
    ```
