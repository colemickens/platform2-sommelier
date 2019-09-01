#!/usr/bin/env python2
# -*- coding: utf-8 -*-
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=module-missing-docstring,class-missing-docstring

from __future__ import print_function

import os
import unittest
import tempfile

import generate_schema_doc

this_dir = os.path.dirname(__file__)


class SchemaTests(unittest.TestCase):

  def testActualSchemaAgainstReadme(self):
    output = tempfile.mktemp()
    generate_schema_doc.Main(
        os.path.join(this_dir, 'cros_config_schema.yaml'),
        output)
    with open(output, 'r') as output_stream:
      output_lines = output_stream.readlines()
      with open(os.path.join(this_dir, '../README.md')) as readme_stream:
        readme_lines = readme_stream.readlines()
        readme_schema_lines = []
        in_section = False
        for line in readme_lines:
          if 'begin_definitions' in line:
            in_section = True

          if in_section:
            readme_schema_lines.append(line)

          if 'end_definitions' in line:
            break

        self.assertEqual(
            output_lines,
            readme_schema_lines,
            'Schema does not match README.\n'
            'Please run: python2 -m cros_config_host.generate_schema_doc '
            '-o README.md')

    os.remove(output)


if __name__ == '__main__':
  unittest.main()
