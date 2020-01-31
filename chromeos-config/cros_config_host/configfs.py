# -*- coding: utf-8 -*-
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for generating ChromeOS ConfigFS private data file."""

from __future__ import print_function

import json
import os
import subprocess
import tempfile


def Serialize(obj):
  """Convert a string, integer, bytes, or bool to its file representation.

  Args:
    obj: The string, integer, bytes, or bool to serialize.

  Returns:
    The bytes representation of the object suitable for dumping into a file.
  """
  if isinstance(obj, bytes):
    return obj
  if isinstance(obj, bool):
    return b'true' if obj else b'false'
  return str(obj).encode('utf-8')


def WriteConfigFSFiles(config, base_path):
  """Recursive function to write ConfigFS data out to files and directories.

  Args:
    config: The configuration item (dict, list, str, int, or bool).
    base_path: The path to write out to.
  """
  if isinstance(config, dict):
    iterator = config.items()
  elif isinstance(config, list):
    iterator = enumerate(config)
  else:
    iterator = None

  if iterator is not None:
    os.makedirs(base_path, mode=0o755)
    for name, entry in iterator:
      path = os.path.join(base_path, str(name))
      WriteConfigFSFiles(entry, path)
  else:
    with open(os.open(base_path, os.O_CREAT | os.O_WRONLY, 0o644), 'wb') as f:
      f.write(Serialize(config))


def WriteIdentityInfo(config, output_file):
  """Write out the data file needed to provide system identification.

  This data file is used at runtime by libcros_config.  Currently, JSON is used
  as the output format to remain compatible with the existing code in
  libcros_config.  However, this function may be updated if libcros_config
  adopts a new format, so do not rely on the output being JSON.

  Args:
    config: The configuration dictionary (containing "chromeos").
    output_file: A file-like object to write to.
  """
  minified_configs = []
  minified_config = {'chromeos': {'configs': minified_configs}}
  for device_config in config['chromeos']['configs']:
    minified_configs.append({'identity': device_config['identity']})
  json.dump(minified_config, output_file)


def GenerateConfigFSData(config, output_fs):
  """Generate the ConfigFS private data.

  Args:
    config: The configuration dictionary.
    output_fs: The file name to write the SquashFS image at.
  """
  with tempfile.TemporaryDirectory(prefix='configfs.') as configdir:
    os.chmod(configdir, 0o755)
    WriteConfigFSFiles(config, os.path.join(configdir, 'v1'))
    with open(os.path.join(configdir, 'v1/identity.json'), 'w') as f:
      WriteIdentityInfo(config, f)
    subprocess.run(['mksquashfs', configdir, output_fs, '-no-xattrs',
                    '-noappend', '-all-root'], check=True,
                   stdout=subprocess.PIPE)
