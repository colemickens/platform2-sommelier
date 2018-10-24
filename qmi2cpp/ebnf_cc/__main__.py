# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Main entry point for the ebnf_cc compiler-compiler."""

from ebnf_cc import mainlib
import sys

if __name__ == '__main__':
    sys.exit(mainlib.main(sys.argv))
