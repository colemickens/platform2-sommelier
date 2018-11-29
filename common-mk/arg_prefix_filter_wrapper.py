#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper of pkg-config like command line to format output for gn.

Parses the output and filter the flags with a certain prefix like "-l", "-L",
so that it can be used in GN files easily.

Usage:
  arg_prefix_filter_wrapper.py --prefix=-l gtest-config --libs
"""

from __future__ import print_function

import argparse
import shlex
import sys
import subprocess


def main(cmd_list, prefix):
  prefix_len = len(prefix)
  flags = shlex.split(subprocess.check_output(cmd_list))
  filtered = [flag[prefix_len:] for flag in flags if flag.startswith(prefix)]
  print('\n'.join(filtered))
  return


if __name__ == '__main__':
  parser = argparse.ArgumentParser()
  parser.add_argument('--prefix', type=str, default="-l")
  args, cmd_list = parser.parse_known_args()
  sys.exit(main(cmd_list, args.prefix))
