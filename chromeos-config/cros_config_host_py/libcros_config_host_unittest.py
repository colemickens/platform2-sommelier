#!/usr/bin/env python2
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The unit test suite for the libcros_config_host.py library"""

from __future__ import print_function

from collections import OrderedDict
from contextlib import contextmanager
from io import BytesIO
import os
import sys
import unittest

import fdt_util
from libcros_config_host import BaseFile, CrosConfig, TouchFile

DTS_FILE = '../libcros_config/test.dts'
MODELS = ['pyro', 'caroline', 'reef', 'broken', 'whitetip', 'whitetip1',
          'whitetip2', ]
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


# Use this to suppress stdout/stderr output:
# with capture_sys_output() as (stdout, stderr)
#   ...do something...
@contextmanager
def capture_sys_output():
  capture_out, capture_err = BytesIO(), BytesIO()
  old_out, old_err = sys.stdout, sys.stderr
  try:
    sys.stdout, sys.stderr = capture_out, capture_err
    yield capture_out, capture_err
  finally:
    sys.stdout, sys.stderr = old_out, old_err


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
    self.assertEqual(set(touch_files), {
        TouchFile('elan/0a97_1012.bin', 'elants_i2c_0a97.bin'),
        TouchFile('elan/3062_5602.bin', 'elants_i2c_3062.bin'),
        TouchFile('elan/306e_5611.bin', 'elants_i2c_306e.bin'),
        TouchFile('elan/97.0_6.0.bin', 'elan_i2c_97.0.bin'),
        TouchFile('wacom/4209.hex', 'wacom_firmware_REEF.bin'),
        TouchFile('wacom/4209.hex', 'wacom_firmware_PYRO.bin'),
        TouchFile('wacom/4209.hex', 'wacom_firmware_WHITETIP1.bin'),
        TouchFile('wacom/4209.hex', 'wacom_firmware_WHITETIP.bin'),
        TouchFile('wacom/4209.hex', 'wacom_firmware_WHITETIP2.bin'),
    })

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
        [BaseFile('cras-config/pyro/bxtda7219max',
                  '/etc/cras/pyro/bxtda7219max'),
         BaseFile('cras-config/pyro/dsp.ini', '/etc/cras/pyro/dsp.ini'),
         BaseFile('cras-config/reef-cras/bxtda7219max',
                  '/etc/cras/reef-cras/bxtda7219max'),
         BaseFile('cras-config/reef-cras/dsp.ini',
                  '/etc/cras/reef-cras/dsp.ini'),
         BaseFile('topology/5a98-reef-pyro-8-tplg.bin',
                  '/lib/firmware/topology/5a98-reef-pyro-8-tplg.bin'),
         BaseFile('topology/5a98-reef-reef-top-8-tplg.bin',
                  '/lib/firmware/topology/5a98-reef-reef-top-8-tplg.bin'),
         BaseFile('ucm-config/bxtda7219max.pyro/HiFi.conf',
                  '/usr/share/alsa/ucm/bxtda7219max.pyro/HiFi.conf'),
         BaseFile('ucm-config/bxtda7219max.pyro/bxtda7219max.pyro.conf',
                  '/usr/share/alsa/ucm/bxtda7219max.pyro/bxtda7219max.pyro' +
                  '.conf'),
         BaseFile('ucm-config/bxtda7219max.reef-ucm/HiFi.conf',
                  '/usr/share/alsa/ucm/bxtda7219max.reef-ucm/HiFi.conf'),
         BaseFile('ucm-config/bxtda7219max.reef-ucm/bxtda7219max.reef-ucm' +
                  '.conf',
                  '/usr/share/alsa/ucm/bxtda7219max.reef-ucm/bxtda7219max' +
                  '.reef-ucm.conf')])

  def testGetThermalFiles(self):
    config = CrosConfig(self.file)
    thermal_files = config.GetThermalFiles()
    self.assertEqual(
        thermal_files,
        [BaseFile(source='pyro/dptf.dv', dest='/etc/dptf/pyro/dptf.dv'),
         BaseFile(source='reef/dptf.dv', dest='/etc/dptf/reef/dptf.dv')])

  def testWhitelabel(self):
    # These mirror the tests in cros_config_unittest.cc CheckWhiteLabel
    config = CrosConfig(self.file)
    whitetip1 = config.models['whitetip1']

    # These are defined by whitetip1 itself
    self.assertEqual(whitetip1.properties['wallpaper'].value, 'shark')
    self.assertEqual(
        whitetip1.ChildPropertyFromPath('/firmware', 'key-id').value,
        'WHITETIP1')

    # This is in a subnode defined by whitetip
    self.assertEqual(whitetip1.ChildPropertyFromPath('/touch', 'present').value,
                     'yes')

    # This is in the main node, but defined by whitetip
    self.assertEqual(whitetip1.ChildPropertyFromPath('/', 'powerd-prefs').value,
                     'whitetip')

    # This is defined by whitetip's shared firmware
    target = whitetip1.ChildPropertyFromPath('/firmware/build-targets',
                                             'coreboot')
    self.assertEqual(target.value, 'caroline')

  def testGetFirmwareBuildTargets(self):
    config = CrosConfig(self.file)
    self.assertSequenceEqual(config.GetFirmwareBuildTargets('coreboot'),
                             ['pyro', 'caroline'])

  def testFileTree(self):
    """Test that we can obtain a file tree"""
    config = CrosConfig(self.file)
    node = config.GetFileTree()
    self.assertEqual(node.name, '')
    self.assertEqual(sorted(node.children.keys()), ['etc', 'lib', 'opt', 'usr'])
    etc = node.children['etc']
    self.assertEqual(etc.name, 'etc')
    cras = etc.children['cras']
    self.assertEqual(cras.name, 'cras')
    basking = cras.children['pyro']
    self.assertEqual(sorted(basking.children.keys()),
                     ['bxtda7219max', 'dsp.ini'])

  def testShowTree(self):
    """Test that we can show a file tree"""
    config = CrosConfig(self.file)
    tree = config.GetFileTree()
    with capture_sys_output() as (stdout, stderr):
      config.ShowTree('/', tree)
    self.assertEqual(stderr.getvalue(), '')
    lines = [line.strip() for line in stdout.getvalue().splitlines()]
    self.assertEqual(lines[0].split(), ['Size', 'Path'])
    self.assertEqual(lines[1], '/')
    self.assertEqual(lines[2], 'etc/')
    self.assertEqual(lines[3].split(), ['missing', 'cras/'])


if __name__ == '__main__':
  unittest.main()
