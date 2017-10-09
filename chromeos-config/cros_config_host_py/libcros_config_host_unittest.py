#!/usr/bin/env python2
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The unit test suite for the libcros_config_host.py library"""

from __future__ import print_function

import os
import unittest

import fdt_util
from libcros_config_host import CrosConfig, TouchFile

DTS_FILE = '../libcros_config/test.dts'
MODELS = ['pyro', 'caroline', 'reef', 'broken']
PYRO_BUCKET = ('gs://chromeos-binaries/HOME/bcs-reef-private/'
               'overlay-reef-private/chromeos-base/chromeos-firmware-pyro/')
PYRO_FIRMWARE_FILES = ['Reef_EC.9042.87.1.tbz2',
                       'Reef_PD.9042.87.1.tbz2',
                       'Reef.9042.87.1.tbz2',
                       'Reef.9042.110.0.tbz2']


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
    self.assertSequenceEqual([n for n, _ in config.models.iteritems()], MODELS)

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
    firmware_uris = config.models['pyro'].GetFirmwareUris()
    self.assertSequenceEqual(firmware_uris, [PYRO_BUCKET + fname for
                                             fname in PYRO_FIRMWARE_FILES])

  def testGetTouchFirmwareFiles(self):
    config = CrosConfig(self.file)
    touch_files = config.models['pyro'].GetTouchFirmwareFiles()
    self.assertSequenceEqual(
        touch_files,
        {'touchscreen': TouchFile('elan/0a97_1012.bin', 'elants_i2c_0a97.bin'),
         'stylus': TouchFile('wacom/4209.hex', 'wacom_firmware_PYRO.bin')})
    touch_files = config.models['reef'].GetTouchFirmwareFiles()

    # This checks that duplicate processing works correct, since both models
    # have the same wacom firmware
    self.assertSequenceEqual(touch_files, {
        'stylus': TouchFile('wacom/4209.hex', 'wacom_firmware_REEF.bin'),
        'touchpad': TouchFile('elan/97.0_6.0.bin', 'elan_i2c_97.0.bin'),
        'touchscreen@0': TouchFile('elan/3062_5602.bin',
                                   'elants_i2c_3062.bin'),
        'touchscreen@1': TouchFile('elan/306e_5611.bin',
                                   'elants_i2c_306e.bin')})

if __name__ == '__main__':
  unittest.main()
