# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from setuptools import setup

setup(
    name='cros_config',
    version='1',
    author='Simon Glass',
    author_email='sjg@chromium.org',
    url='README.md',
    packages=['cros_config_host_py'],
    py_modules=['cros_config_host_py.fdt',
                'cros_config_host_py.libcros_config_host',
                'cros_config_host_py.cros_config_host'],
    description='Access to the master configuration from the host',
)
