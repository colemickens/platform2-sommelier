#!/usr/bin/env python2
# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Copies proto files into the destination directory with proper modification.

Usage: path/to/external_proto_generator.py -o output-directory input-files

For example, `./external_proto_generator.py -o foo a.proto b.proto` will
generate foo/a.proto and foo/b.proto .
"""

from __future__ import print_function

import sys
import os

from chromite.lib import commandline

parser = commandline.ArgumentParser()
parser.add_argument('-o', dest='out_dir', help='output directory',
                    required=True)
parser.add_argument('inputs', nargs='*')
options = parser.parse_args(sys.argv[1:])

for input_path in options.inputs:
  output_path = os.path.join(options.out_dir, os.path.basename(input_path))
  with open(input_path, 'r') as f:
    code = f.read()
  code = code.replace('LITE_RUNTIME', 'CODE_SIZE')
  with open(output_path, 'w') as f:
    f.write(code)
