#!/usr/bin/env python2
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The unit test suite for the fdt.py library"""

from __future__ import print_function

import os
import unittest

import fdt
import fdt_util

DTS_FILE = '../libcros_config/test.dts'
PYRO_FIRMWARE_NAMES = ['bcs-overlay', 'ec-image', 'pd-image', 'main-image',
                       'main-rw-image']


class FdtLibTest(unittest.TestCase):
  """The unit test suite for the fdt.py library"""
  def setUp(self):
    path = os.path.join(os.path.dirname(__file__), DTS_FILE)
    try:
      (fname, temp_file) = fdt_util.EnsureCompiled(path)
      self.test_fdt = fdt.Fdt(fname)
    finally:
      if temp_file is not None:
        os.remove(temp_file.name)

  def testFdtScan(self):
    self.test_fdt.Scan()
    self.assertIsNotNone(self.test_fdt.GetRoot())

  def testGetModels(self):
    self.test_fdt.Scan()
    models_node = self.test_fdt.GetNode('/chromeos/models')
    models = [m.name for m in models_node.subnodes]
    self.assertSequenceEqual(models, ['pyro', 'caroline', 'reef', 'broken'])

  def testPropertyOrder(self):
    self.test_fdt.Scan()
    firmware = self.test_fdt.GetNode('/chromeos/models/pyro/firmware')

    self.assertSequenceEqual(firmware.props.keys(), PYRO_FIRMWARE_NAMES)

  def testGetStringProperty(self):
    self.test_fdt.Scan()
    firmware = self.test_fdt.GetNode('/chromeos/models/pyro/firmware')
    bcs_overlay = firmware.props['bcs-overlay'].value
    self.assertEqual(bcs_overlay, 'overlay-reef-private')


if __name__ == '__main__':
  unittest.main()
