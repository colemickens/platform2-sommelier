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
from libcros_config_host import CrosConfig, FORMAT_FDT
from libcros_config_host_base import BaseFile, TouchFile, FirmwareInfo


DTS_FILE = '../libcros_config/test.dts'
YAML_FILE = '../libcros_config/test.yaml'
MODELS = sorted(['some', 'another', 'whitelabel'])
ANOTHER_BUCKET = ('gs://chromeos-binaries/HOME/bcs-another-private/overlay-'
                  'another-private/chromeos-base/chromeos-firmware-another/')
SOME_BUCKET = ('gs://chromeos-binaries/HOME/bcs-some-private/'
               'overlay-some-private/chromeos-base/chromeos-firmware-some/')
SOME_FIRMWARE_FILES = ['Some_EC.1111.11.1.tbz2',
                       'Some.1111.11.1.tbz2',
                       'Some_RW.1111.11.1.tbz2']
ANOTHER_FIRMWARE_FILES = ['Another_EC.1111.11.1.tbz2',
                          'Another.1111.11.1.tbz2',
                          'Another_RW.1111.11.1.tbz2']

LIB_FIRMWARE = '/lib/firmware/'
TOUCH_FIRMWARE = '/opt/google/touch/firmware/'


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


class CommonTests(object):
  """Shared tests between the YAML and FDT implementations."""

  def testGetProperty(self):
    config = CrosConfig(self.filepath)
    another = config.GetConfig('another')
    self.assertEqual(another.GetProperty('/', 'wallpaper'), 'default')
    self.assertFalse(another.GetProperty('/', 'missing'))

  def testModels(self):
    config = CrosConfig(self.filepath)
    models = config.GetModelList()
    for model in MODELS:
      self.assertIn(model, models)

  def testGetFirmwareUris(self):
    config = CrosConfig(self.filepath)
    firmware_uris = config.GetConfig('another').GetFirmwareUris()
    self.assertSequenceEqual(
        firmware_uris,
        sorted([ANOTHER_BUCKET + fname for fname in ANOTHER_FIRMWARE_FILES]))

  def testGetSharedFirmwareUris(self):
    config = CrosConfig(self.filepath)
    firmware_uris = config.GetFirmwareUris()
    expected = sorted(
        [ANOTHER_BUCKET + fname for fname in ANOTHER_FIRMWARE_FILES] +
        [SOME_BUCKET + fname for fname in SOME_FIRMWARE_FILES])
    self.assertSequenceEqual(firmware_uris, expected)

  def testGetArcFiles(self):
    config = CrosConfig(self.filepath)
    arc_files = config.GetArcFiles()
    self.assertEqual(
        arc_files,
        [BaseFile(
            source='some/hardware_features',
            dest='/usr/share/chromeos-config/sbin/some/hardware_features')])

  def testGetThermalFiles(self):
    config = CrosConfig(self.filepath)
    thermal_files = config.GetThermalFiles()
    self.assertEqual(
        thermal_files,
        [BaseFile('another/dptf.dv', '/etc/dptf/another/dptf.dv'),
         BaseFile('some_notouch/dptf.dv', '/etc/dptf/some_notouch/dptf.dv'),
         BaseFile('some_touch/dptf.dv', '/etc/dptf/some_touch/dptf.dv')])

  def testGetFirmwareBuildTargets(self):
    config = CrosConfig(self.filepath)
    self.assertSequenceEqual(config.GetFirmwareBuildTargets('coreboot'),
                             ['another', 'some'])
    os.environ['FW_NAME'] = 'another'
    self.assertSequenceEqual(config.GetFirmwareBuildTargets('coreboot'),
                             ['another'])
    del os.environ['FW_NAME']

  def testFileTree(self):
    """Test that we can obtain a file tree"""
    config = CrosConfig(self.filepath)
    node = config.GetFileTree()
    self.assertEqual(node.name, '')
    self.assertEqual(sorted(node.children.keys()),
                     ['etc', 'lib', 'opt', 'usr'])
    etc = node.children['etc']
    self.assertEqual(etc.name, 'etc')
    cras = etc.children['cras']
    self.assertEqual(cras.name, 'cras')
    another = cras.children['another']
    self.assertEqual(sorted(another.children.keys()),
                     ['a-card', 'dsp.ini'])

  def testShowTree(self):
    """Test that we can show a file tree"""
    config = CrosConfig(self.filepath)
    tree = config.GetFileTree()
    with capture_sys_output() as (stdout, stderr):
      config.ShowTree('/', tree)
    self.assertEqual(stderr.getvalue(), '')
    lines = [line.strip() for line in stdout.getvalue().splitlines()]
    self.assertEqual(lines[0].split(), ['Size', 'Path'])
    self.assertEqual(lines[1], '/')
    self.assertEqual(lines[2], 'etc/')
    self.assertEqual(lines[3].split(), ['missing', 'cras/'])


  def testFimwareBuildCombinations(self):
    """Test generating a dict of firmware build combinations."""
    config = CrosConfig(self.filepath)
    expected = OrderedDict(
        [('another', ['another', 'another']),
         ('some', ['some', 'some'])])
    result = config.GetFirmwareBuildCombinations(['coreboot', 'depthcharge'])
    self.assertEqual(result, expected)

    os.environ['FW_NAME'] = 'another'
    expected = OrderedDict([('another', ['another', 'another'])])
    result = config.GetFirmwareBuildCombinations(['coreboot', 'depthcharge'])
    self.assertEqual(result, expected)
    del os.environ['FW_NAME']

  def testGetWallpaper(self):
    """Test that we can access the wallpaper information"""
    config = CrosConfig(self.filepath)
    wallpaper = config.GetWallpaperFiles()
    self.assertEquals(['default',
                       'some',
                       'wallpaper-wl1',
                       'wallpaper-wl2'],
                      wallpaper)

  def testGetTouchFirmwareFiles(self):
    def _GetFile(source, symlink):
      """Helper to return a suitable TouchFile"""
      return TouchFile(source, TOUCH_FIRMWARE + source,
                       LIB_FIRMWARE + symlink)

    config = CrosConfig(self.filepath)
    touch_files = config.GetConfig('another').GetTouchFirmwareFiles()
    # pylint: disable=line-too-long
    self.assertEqual(
        touch_files,
        [TouchFile(source='some_stylus_vendor/another-version.hex',
                   dest='/opt/google/touch/firmware/some_stylus_vendor/another-version.hex',
                   symlink='/lib/firmware/some_stylus_vendor_firmware_ANOTHER.bin'),
         TouchFile(source='some_touch_vendor/some-pid_some-version.bin',
                   dest='/opt/google/touch/firmware/some_touch_vendor/some-pid_some-version.bin',
                   symlink='/lib/firmware/some_touch_vendorts_i2c_some-pid.bin')])
    touch_files = config.GetConfig('some').GetTouchFirmwareFiles()

    # This checks that duplicate processing works correct, since both models
    # have the same wacom firmware
    self.assertEqual(
        touch_files,
        [TouchFile(source='some_stylus_vendor/some-version.hex',
                   dest='/opt/google/touch/firmware/some_stylus_vendor/some-version.hex',
                   symlink='/lib/firmware/some_stylus_vendor_firmware_SOME.bin'),
         TouchFile(source='some_touch_vendor/some-pid_some-version.bin',
                   dest='/opt/google/touch/firmware/some_touch_vendor/some-pid_some-version.bin',
                   symlink='/lib/firmware/some_touch_vendorts_i2c_some-pid.bin'),
         TouchFile(source='some_touch_vendor/some-other-pid_some-other-version.bin',
                   dest='/opt/google/touch/firmware/some_touch_vendor/some-other-pid_some-other-version.bin',
                   symlink='/lib/firmware/some_touch_vendorts_i2c_some-other-pid.bin')])
    touch_files = config.GetTouchFirmwareFiles()
    expected = set(
        [TouchFile(source='some_stylus_vendor/another-version.hex',
                   dest='/opt/google/touch/firmware/some_stylus_vendor/another-version.hex',
                   symlink='/lib/firmware/some_stylus_vendor_firmware_ANOTHER.bin'),
         TouchFile(source='some_stylus_vendor/some-version.hex',
                   dest='/opt/google/touch/firmware/some_stylus_vendor/some-version.hex',
                   symlink='/lib/firmware/some_stylus_vendor_firmware_SOME.bin'),
         TouchFile(source='some_touch_vendor/some-pid_some-version.bin',
                   dest='/opt/google/touch/firmware/some_touch_vendor/some-pid_some-version.bin',
                   symlink='/lib/firmware/some_touch_vendorts_i2c_some-pid.bin'),
         TouchFile(source='some_touch_vendor/some-other-pid_some-other-version.bin',
                   dest='/opt/google/touch/firmware/some_touch_vendor/some-other-pid_some-other-version.bin',
                   symlink='/lib/firmware/some_touch_vendorts_i2c_some-other-pid.bin'),
         TouchFile(source='some_touch_vendor/some-pid_some-version.bin',
                   dest='/opt/google/touch/firmware/some_touch_vendor/some-pid_some-version.bin',
                   symlink='/lib/firmware/some_touch_vendorts_i2c_some-pid.bin')])
    self.assertEqual(set(touch_files), expected)

  def testGetAudioFiles(self):
    config = CrosConfig(self.filepath)
    audio_files = config.GetAudioFiles()
    expected = \
      [BaseFile(source='cras-config/another/dsp.ini',
                dest='/etc/cras/another/dsp.ini'),
       BaseFile(source='cras-config/another/a-card',
                dest='/etc/cras/another/a-card'),
       BaseFile(source='cras-config/some/dsp.ini',
                dest='/etc/cras/some/dsp.ini'),
       BaseFile(source='cras-config/some/a-card',
                dest='/etc/cras/some/a-card'),
       BaseFile(source='topology/another-tplg.bin',
                dest='/lib/firmware/another-tplg.bin'),
       BaseFile(source='topology/some-tplg.bin',
                dest='/lib/firmware/some-tplg.bin'),
       BaseFile(source='ucm-config/a-card.another/HiFi.conf',
                dest='/usr/share/alsa/ucm/a-card.another/HiFi.conf'),
       BaseFile(source='ucm-config/a-card.another/a-card.another.conf',
                dest='/usr/share/alsa/ucm/a-card.another/a-card.another.conf'),
       BaseFile(source='ucm-config/a-card.some/HiFi.conf',
                dest='/usr/share/alsa/ucm/a-card.some/HiFi.conf'),
       BaseFile(source='ucm-config/a-card.some/a-card.some.conf',
                dest='/usr/share/alsa/ucm/a-card.some/a-card.some.conf')]

    self.assertEqual(audio_files, sorted(expected))

  def testFirmware(self):
    """Test access to firmware information"""
    config = CrosConfig(self.filepath)
    self.assertEqual('updater4.sh', config.GetFirmwareScript())

    expected = OrderedDict(
        [('another',
          FirmwareInfo(model='another',
                       shared_model=None,
                       key_id='ANOTHER',
                       have_image=True,
                       bios_build_target='another',
                       ec_build_target='another',
                       main_image_uri='bcs://Another.1111.11.1.tbz2',
                       main_rw_image_uri='bcs://Another_RW.1111.11.1.tbz2',
                       ec_image_uri='bcs://Another_EC.1111.11.1.tbz2',
                       pd_image_uri='',
                       extra=['${FILESDIR}/extra'],
                       create_bios_rw_image=False,
                       tools=['${FILESDIR}/tools1', '${FILESDIR}/tools2'],
                       sig_id='another')),
         ('some',
          FirmwareInfo(model='some',
                       shared_model='some',
                       key_id='SOME',
                       have_image=True,
                       bios_build_target='some',
                       ec_build_target='some',
                       main_image_uri='bcs://Some.1111.11.1.tbz2',
                       main_rw_image_uri='bcs://Some_RW.1111.11.1.tbz2',
                       ec_image_uri='bcs://Some_EC.1111.11.1.tbz2',
                       pd_image_uri='',
                       extra=[],
                       create_bios_rw_image=False,
                       tools=[],
                       sig_id='some')),
         ('whitelabel',
          FirmwareInfo(model='whitelabel',
                       shared_model='some',
                       key_id='',
                       have_image=True,
                       bios_build_target='some',
                       ec_build_target='some',
                       main_image_uri='bcs://Some.1111.11.1.tbz2',
                       main_rw_image_uri='bcs://Some_RW.1111.11.1.tbz2',
                       ec_image_uri='bcs://Some_EC.1111.11.1.tbz2',
                       pd_image_uri='',
                       extra=[],
                       create_bios_rw_image=False,
                       tools=[],
                       sig_id='sig-id-in-customization-id')),
         ('whitelabel-whitelabel1',
          FirmwareInfo(model='whitelabel-whitelabel1',
                       shared_model='some',
                       key_id='WHITELABEL1',
                       have_image=False,
                       bios_build_target='some',
                       ec_build_target='some',
                       main_image_uri='bcs://Some.1111.11.1.tbz2',
                       main_rw_image_uri='bcs://Some_RW.1111.11.1.tbz2',
                       ec_image_uri='bcs://Some_EC.1111.11.1.tbz2',
                       pd_image_uri='',
                       extra=[],
                       create_bios_rw_image=False,
                       tools=[],
                       sig_id='whitelabel-whitelabel1')),
         ('whitelabel-whitelabel2',
          FirmwareInfo(model='whitelabel-whitelabel2',
                       shared_model='some',
                       key_id='WHITELABEL2',
                       have_image=False,
                       bios_build_target='some',
                       ec_build_target='some',
                       main_image_uri='bcs://Some.1111.11.1.tbz2',
                       main_rw_image_uri='bcs://Some_RW.1111.11.1.tbz2',
                       ec_image_uri='bcs://Some_EC.1111.11.1.tbz2',
                       pd_image_uri='',
                       extra=[],
                       create_bios_rw_image=False,
                       tools=[],
                       sig_id='whitelabel-whitelabel2'))])
    result = config.GetFirmwareInfo()
    self.assertEqual(result, expected)


class CrosConfigHostTestFdt(unittest.TestCase, CommonTests):
  """Tests for master configuration in device tree format"""
  def setUp(self):
    path = os.path.join(os.path.dirname(__file__), DTS_FILE)
    (self.filepath, self.temp_file) = fdt_util.EnsureCompiled(path)

  def tearDown(self):
    os.remove(self.temp_file.name)

  def testGoodDtbFile(self):
    self.assertIsNotNone(CrosConfig(self.filepath))

  def testNodeSubnames(self):
    config = CrosConfig(self.filepath)
    for name, model in config.models.iteritems():
      self.assertEqual(name, model.name)

  def testPathNode(self):
    config = CrosConfig(self.filepath)
    self.assertIsNotNone(config.models['another'].PathNode('/firmware'))

  def testBadPathNode(self):
    config = CrosConfig(self.filepath)
    self.assertIsNone(config.models['another'].PathNode('/dne'))

  def testPathProperty(self):
    config = CrosConfig(self.filepath)
    another = config.models['another']
    ec_image = another.PathProperty('/firmware', 'ec-image')
    self.assertEqual(ec_image.value, 'bcs://Another_EC.1111.11.1.tbz2')

  def testBadPathProperty(self):
    config = CrosConfig(self.filepath)
    another = config.models['another']
    self.assertIsNone(another.PathProperty('/firmware', 'dne'))
    self.assertIsNone(another.PathProperty('/dne', 'ec-image'))

  def testSinglePhandleFollowProperty(self):
    config = CrosConfig(self.filepath)
    another = config.models['another']
    bcs_overlay = another.PathProperty('/firmware', 'bcs-overlay')
    self.assertEqual(bcs_overlay.value, 'overlay-another-private')

  def testSinglePhandleFollowNode(self):
    config = CrosConfig(self.filepath)
    another = config.models['another']
    target = another.PathProperty('/firmware/build-targets', 'coreboot')
    self.assertEqual(target.value, 'another')

  def testGetMergedPropertiesAnother(self):
    config = CrosConfig(self.filepath)
    another = config.models['another']
    stylus = another.PathNode('touch/stylus')
    props = another.GetMergedProperties(stylus, 'touch-type')
    self.assertSequenceEqual(
        props,
        {'version': 'another-version',
         'vendor': 'some_stylus_vendor',
         'firmware-bin': '{vendor}/{version}.hex',
         'firmware-symlink': '{vendor}_firmware_{MODEL}.bin'})

  def testGetMergedPropertiesSome(self):
    config = CrosConfig(self.filepath)
    some = config.models['some']
    touchscreen = some.PathNode('touch/touchscreen@0')
    props = some.GetMergedProperties(touchscreen, 'touch-type')
    self.assertEqual(
        props,
        {'pid': 'some-other-pid',
         'version': 'some-other-version',
         'vendor': 'some_touch_vendor',
         'firmware-bin': '{vendor}/{pid}_{version}.bin',
         'firmware-symlink': '{vendor}ts_i2c_{pid}.bin'})

  def testGetMergedPropertiesDefault(self):
    """Test that the 'default' property is used when collecting properties"""
    config = CrosConfig(self.filepath)
    another = config.models['another']
    audio = another.PathNode('/audio/main')
    props = another.GetMergedProperties(audio, 'audio-type')
    self.assertSequenceEqual(
        props,
        {'cras-config-dir': 'another',
         'ucm-suffix': 'another',
         'topology-name': 'another',
         'card': 'a-card',
         'volume': 'cras-config/{cras-config-dir}/{card}',
         'dsp-ini': 'cras-config/{cras-config-dir}/dsp.ini',
         'hifi-conf': 'ucm-config/{card}.{ucm-suffix}/HiFi.conf',
         'alsa-conf': 'ucm-config/{card}.{ucm-suffix}/{card}.' +
                      '{ucm-suffix}.conf',
         'topology-bin': 'topology/{topology-name}-tplg.bin'})

  def testWriteTargetDirectories(self):
    """Test that we can write out a list of file paths"""
    config = CrosConfig(self.filepath)
    target_dirs = config.GetTargetDirectories()
    self.assertEqual(target_dirs['dptf-dv'], '/etc/dptf')
    self.assertEqual(target_dirs['hifi-conf'], '/usr/share/alsa/ucm')
    self.assertEqual(target_dirs['alsa-conf'], '/usr/share/alsa/ucm')
    self.assertEqual(target_dirs['volume'], '/etc/cras')
    self.assertEqual(target_dirs['dsp-ini'], '/etc/cras')
    self.assertEqual(target_dirs['cras-config-dir'], '/etc/cras')

  def testSubmodel(self):
    """Test that we can read properties from the submodel"""
    config = CrosConfig(self.filepath)
    some = config.models['some']
    self.assertEqual(
        some.SubmodelPathProperty('touch', '/audio/main', 'ucm-suffix').value,
        'some')
    self.assertEqual(
        some.SubmodelPathProperty('touch', '/touch', 'present').value, 'yes')
    self.assertEqual(
        some.SubmodelPathProperty('notouch', '/touch', 'present').value, 'no')

  def testGetProperty(self):
    """Test that we can read properties from non-model nodes"""
    config = CrosConfig(self.filepath)
    # pylint: disable=protected-access
    self.assertEqual(config._GetProperty('/chromeos/family/firmware',
                                         'script').value, 'updater4.sh')
    firmware = config.GetFamilyNode('/firmware')
    self.assertEqual(firmware.name, 'firmware')
    self.assertEqual(firmware.Property('script').value, 'updater4.sh')
    self.assertEqual(
        config.GetFamilyProperty('/firmware', 'script').value, 'updater4.sh')

  def testDefaultConfig(self):
    if 'SYSROOT' in os.environ:
      del os.environ['SYSROOT']
    with self.assertRaisesRegexp(ValueError,
                                 'No master configuration is available'):
      CrosConfig()

    os.environ['SYSROOT'] = 'fred'
    with self.assertRaises(IOError) as e:
      CrosConfig()
    self.assertIn('fred/usr/share/chromeos-config/config.dtb',
                  str(e.exception))


class CrosConfigHostTestYaml(unittest.TestCase, CommonTests):
  """Tests for master configuration in yaml format"""
  def setUp(self):
    self.filepath = os.path.join(os.path.dirname(__file__), YAML_FILE)


if __name__ == '__main__':
  unittest.main()
