#!/usr/bin/env python2
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The unit test suite for libcros_config_host_json"""

from __future__ import print_function

import os
import unittest

from cros_config_host.v2.libcros_config_host_json import CrosConfigJson
from ..libcros_config_host import BaseFile, TouchFile

YAML_FILE = 'cros_config_schema_example.yaml'


class CrosConfigHostJsonTest(unittest.TestCase):
  """The unit test suite for the CrosConfigHost CLI tool."""
  def setUp(self):
    path = os.path.join(os.path.dirname(__file__), YAML_FILE)
    self.file = open(path)
    self.config = CrosConfigJson(self.file)

  def tearDown(self):
    pass

  def testListModels(self):
    models = self.config.GetModelList()
    self.assertListEqual(['basking', 'electro'], models)

  def testGetFirmwareUris(self):
    expected_base_path = (
        'gs://chromeos-binaries/HOME/bcs-reef-private/overlay-reef-private/'
        'chromeos-base/chromeos-firmware-reef/%s'
    )
    fw_uris = self.config.GetFirmwareUris()
    self.assertEquals(3, len(fw_uris))
    self.assertIn(expected_base_path % 'Reef.9042.110.0.tbz2', fw_uris)
    self.assertIn(expected_base_path % 'Reef.9042.87.1.tbz2', fw_uris)
    self.assertIn(expected_base_path % 'Reef_EC.9042.87.1.tbz2', fw_uris)

  # Disable this test since it doesn't work with the new schema. This test will
  # be dropped anyway when we rework the CrosConfig logic to be shared by the
  # YAML impl.
  def disabled_testGetTouchFirmwareFiles(self):
    touch_files = self.config.GetTouchFirmwareFiles()
    self.assertEquals(7, len(touch_files))
    self.assertIn(
        TouchFile('elan/154.0_2.0.bin', 'elan_i2c_154.0.bin'), touch_files)
    self.assertIn(
        TouchFile('elan/1cd2_5602.bin', 'elants_i2c_1cd2.bin'), touch_files)

  def testGetAudioFiles(self):
    audio_files = self.config.GetAudioFiles()
    self.assertEquals(6, len(audio_files))
    self.assertIn(
        BaseFile(
            'cras-config/basking/bxtda7219max',
            '/etc/cras/basking/bxtda7219max'),
        audio_files)

if __name__ == '__main__':
  unittest.main()
