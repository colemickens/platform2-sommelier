#!/usr/bin/env python2
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The unit test suite for the libcros_config_host.py library"""

from __future__ import print_function

from collections import OrderedDict
import os
import unittest

import fdt_util
from libcros_config_host import AudioFile, CrosConfig, TouchFile

DTS_FILE = '../libcros_config/test.dts'
MODELS = ['pyro', 'caroline', 'reef', 'broken']
PYRO_BUCKET = ('gs://chromeos-binaries/HOME/bcs-reef-private/'
               'overlay-reef-private/chromeos-base/chromeos-firmware-pyro/')
CAROLINE_BUCKET = ('gs://chromeos-binaries/HOME/bcs-reef-private/'
                   'overlay-reef-private/chromeos-base/'
                   'chromeos-firmware-caroline/')
PYRO_FIRMWARE_FILES = ['Reef_EC.9042.87.1.tbz2',
                       'Reef_PD.9042.87.1.tbz2',
                       'Reef.9042.87.1.tbz2',
                       'Reef.9042.110.0.tbz2']
CAROLINE_FIRMWARE_FILES = ['Caroline_EC.2017.21.1.tbz2',
                           'Caroline_PD.2017.21.1.tbz2',
                           'Caroline.2017.21.1.tbz2',
                           'Caroline.2017.41.0.tbz2']


class CrosConfigHostTest(unittest.TestCase):
  """The unit test suite for the libcros_config_host.py library"""
  def setUp(self):
    path = os.path.join(os.path.dirname(__file__), DTS_FILE)
    (filepath, self.temp_file) = fdt_util.EnsureCompiled(path)
    self.file = open(filepath)

  def tearDown(self):
    if self.temp_file is not None:
      os.remove(self.temp_file.name)
    self.file.close()

  def testGoodDtbFile(self):
    self.assertIsNotNone(CrosConfig(self.file))

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

  def testSinglePhandleFollowProperty(self):
    config = CrosConfig(self.file)
    caroline = config.models['caroline']
    bcs_overlay = caroline.ChildPropertyFromPath('/firmware', 'bcs-overlay')
    self.assertEqual(bcs_overlay.value, 'overlay-reef-private')

  def testSinglePhandleFollowNode(self):
    config = CrosConfig(self.file)
    caroline = config.models['caroline']
    target = caroline.ChildPropertyFromPath('/firmware/build-targets',
                                            'coreboot')
    self.assertEqual(target.value, 'caroline')

  def testGetFirmwareUris(self):
    config = CrosConfig(self.file)
    firmware_uris = config.models['pyro'].GetFirmwareUris()
    self.assertSequenceEqual(firmware_uris, [PYRO_BUCKET + fname for
                                             fname in PYRO_FIRMWARE_FILES])

  def testGetSharedFirmwareUris(self):
    config = CrosConfig(self.file)
    firmware_uris = config.models['caroline'].GetFirmwareUris()
    self.assertSequenceEqual(firmware_uris, [CAROLINE_BUCKET + fname for
                                             fname in CAROLINE_FIRMWARE_FILES])

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
    touch_files = config.GetTouchFirmwareFiles()
    self.assertEqual(touch_files, [
        TouchFile(firmware='elan/0a97_1012.bin', symlink='elants_i2c_0a97.bin'),
        TouchFile(firmware='elan/3062_5602.bin', symlink='elants_i2c_3062.bin'),
        TouchFile(firmware='elan/306e_5611.bin', symlink='elants_i2c_306e.bin'),
        TouchFile(firmware='elan/97.0_6.0.bin', symlink='elan_i2c_97.0.bin'),
        TouchFile(firmware='wacom/4209.hex', symlink='wacom_firmware_REEF.bin'),
        TouchFile(firmware='wacom/4209.hex', symlink='wacom_firmware_PYRO.bin')
    ])

  def testGetMergedPropertiesPyro(self):
    config = CrosConfig(self.file)
    pyro = config.models['pyro']
    stylus = pyro.ChildNodeFromPath('touch/stylus')
    props = stylus.GetMergedProperties('touch-type')
    self.assertSequenceEqual(
        props,
        OrderedDict([('version', '4209'),
                     ('vendor', 'wacom'),
                     ('firmware-bin', 'wacom/${version}.hex'),
                     ('firmware-symlink', 'wacom_firmware_${MODEL}.bin')]))

  def testGetMergedPropertiesReef(self):
    config = CrosConfig(self.file)
    reef = config.models['reef']
    touchscreen = reef.ChildNodeFromPath('touch/touchscreen@1')
    props = touchscreen.GetMergedProperties('touch-type')
    self.assertSequenceEqual(
        props,
        OrderedDict([('pid', '306e'),
                     ('version', '5611')
                     , ('vendor', 'elan'),
                     ('firmware-bin', '${vendor}/${pid}_${version}.bin'),
                     ('firmware-symlink', '${vendor}ts_i2c_${pid}.bin')]))

  def testGetAudioFiles(self):
    config = CrosConfig(self.file)
    audio_files = config.GetAudioFiles()
    self.assertEqual(
        audio_files,
        [AudioFile('cras-config/pyro/bxtda7219max',
                   '/etc/cras/pyro/bxtda7219max'),
         AudioFile('cras-config/pyro/dsp.ini', '/etc/cras/pyro/dsp.ini'),
         AudioFile('cras-config/reef-cras/bxtda7219max',
                   '/etc/cras/reef-cras/bxtda7219max'),
         AudioFile('cras-config/reef-cras/dsp.ini',
                   '/etc/cras/reef-cras/dsp.ini'),
         AudioFile('topology/5a98-reef-pyro-8-tplg.bin',
                   '/lib/firmware/topology/5a98-reef-pyro-8-tplg.bin'),
         AudioFile('topology/5a98-reef-reef-top-8-tplg.bin',
                   '/lib/firmware/topology/5a98-reef-reef-top-8-tplg.bin'),
         AudioFile('ucm-config/bxtda7219max.pyro/HiFi.conf',
                   '/usr/share/alsa/ucm/bxtda7219max.pyro/HiFi.conf'),
         AudioFile('ucm-config/bxtda7219max.pyro/bxtda7219max.pyro.conf',
                   '/usr/share/alsa/ucm/bxtda7219max.pyro/bxtda7219max.pyro' +
                   '.conf'),
         AudioFile('ucm-config/bxtda7219max.reef-ucm/HiFi.conf',
                   '/usr/share/alsa/ucm/bxtda7219max.reef-ucm/HiFi.conf'),
         AudioFile('ucm-config/bxtda7219max.reef-ucm/bxtda7219max.reef-ucm' +
                   '.conf',
                   '/usr/share/alsa/ucm/bxtda7219max.reef-ucm/bxtda7219max' +
                   '.reef-ucm.conf')])


if __name__ == '__main__':
  unittest.main()
