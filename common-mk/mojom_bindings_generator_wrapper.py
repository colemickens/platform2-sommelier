#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Thin wrapper of Mojo's mojom_bindings_generator.py.

To generate C++ files from mojom, it is necessary to run
mojom_bindings_generator.py twice (with and without
--generate_non_variant_code) for each mojom file.

However, gyp's "rule" does not support multiple "action"s. So, instead,
use this simple python wrapper.

Usage:
  python mojom_bindings_generator_wrapper.py ${MOJOM_BINDINGS_GENERATOR} \
    [... and more args/flags to be passed to the mojom_bindings_generator.py]
"""

from __future__ import print_function

import subprocess
import sys


def main(argv):
  subprocess.check_call(argv[1:])
  subprocess.check_call(argv[1:] + ['--generate_non_variant_code'])


if __name__ == '__main__':
  main(sys.argv)
