#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=import-error

"""Extracts the `perf-data` element from a feedback logs archive.

Example:
  perf_data_extract.py system_logs.zip /tmp/perf-data.proto
"""

from __future__ import print_function

import base64
import sys
import tempfile
import zipfile

def get_perf_data(zip_file):
  """Extract the base64 blob of perf-data from a zip archive.

  Args:
    zip_file: (str) file path to system logs archive.

  Returns:
    str: base64-encoded perf-data
  """
  tmp_dir = tempfile.TemporaryDirectory()

  result = None
  zipfile.ZipFile(zip_file).extractall(path=tmp_dir.name)
  system_logs_txt = open(tmp_dir.name + '/' + 'system_logs.txt')
  prefix = 'perf-data='
  blob_start = '<base64>:'

  line = system_logs_txt.readline()
  while line:
    # Match the line starting with 'perf-data='
    if line.startswith(prefix):
      # perf-data is a multiline entry. Match the line that contains the
      # base64-encoded data.
      while line.find(blob_start) == -1:
        line = system_logs_txt.readline()

      result = line[line.index(blob_start) + len(blob_start):].lstrip()
      break

    line = system_logs_txt.readline()

  tmp_dir.cleanup()
  return result

def parse_args():
  """Parse arguments for this client.

  Returns:
    Object: parsed args:
      system_logs_zip: path to input system logs zip archive
      perf_data_proto: path to save the extracted perf data proto to
  """
  import argparse
  parser = argparse.ArgumentParser(
      description='Extract perf data from system_logs.zip.')
  parser.add_argument('system_logs_zip', help='System logs zip file.')
  parser.add_argument('perf_data_proto',
                      help='Saved perf data (PerfDataProto format).')

  args = parser.parse_args()
  return args

def decompress_lzma(lzma_data):
  """Decompresses LZMA data.

  Args:
    lzma_data: lzma compressed data as bytes.

  Returns:
    Decompressed data as bytes.
  """
  import lzma
  return lzma.LZMADecompressor().decompress(lzma_data)

def ensure_version():
  """Ensure this script runs with Python 3.3 or greater."""
  if sys.version_info[:2] < (3, 3):
    v_major, v_minor = sys.version_info[:2]
    print("Python version 3.3 or greater required for LZMA decompression, "
          "but you have Python %d.%d" % (v_major, v_minor))
    sys.exit(-2)

if __name__ == '__main__':
  ensure_version()

  parsed_args = parse_args()
  # Extract perf-data binary blob from the zip archive.
  perf_data_base64 = get_perf_data(parsed_args.system_logs_zip)

  if not perf_data_base64:
    print("Error: perf-data not found in the system logs")
    sys.exit(-1)

  # Base64-decode and then xz-decompress.
  binary_data = base64.b64decode(perf_data_base64)
  perf_data = decompress_lzma(binary_data)

  # Save the PerfDataProto file.
  open(parsed_args.perf_data_proto, 'wb').write(perf_data)
