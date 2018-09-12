#!/usr/bin/env python2
# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Python script to output the given list as a space-delimited string to a file.

path/to/write_args.py --output filename -- arg1 arg2 ...
will output the args to the file.

This script is used from common-mk/deps.gni .
TODO(oka): Remove this file and replace the invocation with write_file if
space-delimiting option is added to it.
https://bugs.chromium.org/p/gn/issues/detail?id=9
"""

from __future__ import print_function

import sys

from chromite.lib import commandline

parser = commandline.ArgumentParser()
parser.add_argument('--output', help='the output file name', required=True)
parser.add_argument('args', nargs='*')
options = parser.parse_args(sys.argv[1:])

with open(options.output, 'w') as f:
  f.write(' '.join(options.args) + '\n')
