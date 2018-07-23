# Smogcheck I2C Library

C library to communicate with I2C bus directly.

Notes:
1.  The goal of this module is to enable direct I/O operations on I2C bus
    with minimal overhead. Although i2c-tools has been made available in
    Chromium OS, it still carries much overhead (as revealed by strace).

2.  The original source code was posted to Linux Kernel Documentation
    (http://www.mjmwired.net/kernel/Documentation/i2c/dev-interface) and
    has been modified and adopted here. According to this source, there are
    two versions of i2c-dev.h, one distributed with the Linux kernel and the
    other with i2c-tools. This module depends on the kernel version.

3.  The included Makefile creates a C shared library object (.so file), which
    can then be imported into Python via ctypes for developing new Autotest
    test cases for I2C devices.

Source for i2c-tools: http://www.lm-sensors.org/wiki/I2CTools
