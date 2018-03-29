# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Crome OS Configuration access library.

Provides build-time access to the master configuration on the host. It is used
for reading from the master configuration. Consider using cros_config_host.py
for CLI access to this library.
"""

from __future__ import print_function

import json

from .cros_config_schema import TransformConfig
from ..libcros_config_host import CrosConfigBaseImpl

UNIBOARD_JSON_INSTALL_PATH = 'usr/share/chromeos-config/config.json'


class CrosConfigJson(CrosConfigBaseImpl):
  """JSON specific impl of CrosConfig"""

  def __init__(self, infile):
    self._json = json.loads(TransformConfig(infile.read()))
