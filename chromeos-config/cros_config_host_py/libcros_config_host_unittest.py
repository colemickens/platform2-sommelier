#!/usr/bin/env python2
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The unit test suite for the libcros_config_host.py library"""

from __future__ import print_function

import os
import unittest

import fdt_util
from libcros_config_host import CrosConfig

DTS_FILE = '../libcros_config/test.dts'
MODELS = sorted(['pyro', 'caroline', 'reef', 'broken'])
PYRO_BUCKET = ('gs://chromeos-binaries/HOME/bcs-reef-private/'
               'overlay-reef-private/chromeos-base/chromeos-firmware-pyro')
PYRO_FIRMWARE_FILES = sorted(['/Reef_EC.9042.87.1.tbz2',
                              '/Reef_PD.9042.87.1.tbz2',
                              '/Reef.9042.87.1.tbz2',
                              '/Reef.9042.110.0.tbz2'])


class CrosConfigHostTest(unittest.TestCase):
  """The unit test suite for the libcros_config_host.py library"""
  def setUp(self):
    path = os.path.join(os.path.dirname(__file__), DTS_FILE)
    (self.file, self.temp_file) = fdt_util.EnsureCompiled(path)

  def tearDown(self):
    if self.temp_file is not None:
      os.remove(self.temp_file.name)

  def testGoodDtbFile(self):
    self.assertIsNotNone(CrosConfig(self.file))

  def testDneDbtFile(self):
    with self.assertRaises(IOError):
      CrosConfig('does_not_exist.dtb')

  def testModels(self):
    config = CrosConfig(self.file)
    model_names = sorted([n for n, _ in config.models.iteritems()])
    self.assertSequenceEqual(model_names, MODELS)

  def testNodeSubnames(self):
    config = CrosConfig(self.file)
    for name, model in config.models.iteritems():
      self.assertEqual(name, model.name)

  def testProperties(self):
    config = CrosConfig(self.file)
    pyro = config.models['pyro']
    self.assertEqual(pyro.properties['wallpaper'].value, 'default')

  def testChildNodeFromPath(self):
    config = CrosConfig(self.file)
    self.assertIsNotNone(config.models['pyro'].ChildNodeFromPath('/firmware'))

  def testBadChildNodeFromPath(self):
    config = CrosConfig(self.file)
    self.assertIsNone(config.models['pyro'].ChildNodeFromPath('/dne'))

  def testChildPropertyFromPath(self):
    config = CrosConfig(self.file)
    pyro = config.models['pyro']
    ec_image = pyro.ChildPropertyFromPath('/firmware', 'ec-image')
    self.assertEqual(ec_image.value, 'bcs://Reef_EC.9042.87.1.tbz2')

  def testBadChildPropertyFromPath(self):
    config = CrosConfig(self.file)
    pyro = config.models['pyro']
    self.assertIsNone(pyro.ChildPropertyFromPath('/firmware', 'dne'))
    self.assertIsNone(pyro.ChildPropertyFromPath('/dne', 'ec-image'))

  def testGetFirmwareUris(self):
    config = CrosConfig(self.file)
    firmware_uris = sorted(config.models['pyro'].GetFirmwareUris())
    self.assertSequenceEqual(firmware_uris, [PYRO_BUCKET + fname for
                                             fname in PYRO_FIRMWARE_FILES])

if __name__ == '__main__':
  unittest.main()
