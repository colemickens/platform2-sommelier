# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Chrome OS Configuration access library.

Provides build-time access to the master configuration on the host. It is used
for reading from the master configuration. Consider using cros_config_host.py
for CLI access to this library.
"""

from __future__ import print_function

import os
import sys

import libcros_config_host_fdt
import libcros_config_host_json

UNIBOARD_CONFIG_INSTALL_DIR = 'usr/share/chromeos-config'

# We support two configuration file format
(FORMAT_FDT, FORMAT_YAML) = range(2)

def CrosConfig(fname=None):
  """Create a new CrosConfigBaseImpl object

  This is in a separate function to allow us to (in the future) support YAML,
  which will have a different means of creating the impl class.

  Args:
    fname: Filename of config file
  """
  if fname and ('.yaml' in fname or '.json' in fname):
    config_format = FORMAT_YAML
  else:
    config_format = FORMAT_FDT
  if not fname:
    if 'SYSROOT' not in os.environ:
      raise ValueError('No master configuration is available outside the '
                       'ebuild environemnt. You must specify one')
    fname = os.path.join(
        os.environ['SYSROOT'],
        UNIBOARD_CONFIG_INSTALL_DIR,
        'yaml',
        'config.yaml')
    if os.path.exists(fname):
      config_format = FORMAT_YAML
    else:
      fname = os.path.join(
          os.environ['SYSROOT'], UNIBOARD_CONFIG_INSTALL_DIR, 'config.dtb')
      config_format = FORMAT_FDT
  if fname == '-':
    infile = sys.stdin
  else:
    infile = open(fname)

  if config_format == FORMAT_FDT:
    return libcros_config_host_fdt.CrosConfigFdt(infile)
  elif config_format == FORMAT_YAML:
    return libcros_config_host_json.CrosConfigJson(infile)
  else:
    raise ValueError("Invalid config format '%s' requested" % config_format)
