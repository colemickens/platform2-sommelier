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

from libcros_config_host import fdt_util
from libcros_config_host.libcros_config_host import BaseFile, CrosConfig
from libcros_config_host.libcros_config_host import TouchFile, FirmwareInfo


DTS_FILE = 'libcros_config/test.dts'
MODELS = ['pyro', 'caroline', 'reef', 'broken', 'whitetip', 'whitetip1',
          'whitetip2', ]
PYRO_BUCKET = ('gs://chromeos-binaries/HOME/bcs-pyro-private/'
               'overlay-pyro-private/chromeos-base/chromeos-firmware-pyro/')
REEF_BUCKET = ('gs://chromeos-binaries/HOME/bcs-reef-private/'
               'overlay-reef-private/chromeos-base/chromeos-firmware-reef/')
CAROLINE_BUCKET = ('gs://chromeos-binaries/HOME/bcs-reef-private/'
                   'overlay-reef-private/chromeos-base/'
                   'chromeos-firmware-caroline/')
REEF_FIRMWARE_FILES = ['Reef_EC.9042.87.1.tbz2',
                       'Reef.9042.87.1.tbz2',
                       'Reef.9042.110.0.tbz2']
PYRO_FIRMWARE_FILES = ['Pyro_EC.9042.87.1.tbz2',
                       'Pyro_PD.9042.87.1.tbz2',
                       'Pyro.9042.87.1.tbz2',
                       'Pyro.9042.110.0.tbz2']
CAROLINE_FIRMWARE_FILES = ['Caroline_EC.2017.21.1.tbz2',
                           'Caroline_PD.2017.21.1.tbz2',
                           'Caroline.2017.21.1.tbz2',
                           'Caroline.2017.41.0.tbz2']
BROKEN_BUCKET = ('gs://chromeos-binaries/HOME/bcs-reef-private/'
                 'overlay-reef-private/chromeos-base/chromeos-firmware-broken/')
BROKEN_FIRMWARE_FILES = ['Reef.9042.87.1.tbz2']

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
    self.assertEqual(pyro.Property('wallpaper').value, 'default')
    self.assertIsNone(pyro.Property('missing'))

  def testPathNode(self):
    config = CrosConfig(self.file)
    self.assertIsNotNone(config.models['pyro'].PathNode('/firmware'))

  def testBadPathNode(self):
    config = CrosConfig(self.file)
    self.assertIsNone(config.models['pyro'].PathNode('/dne'))

  def testPathProperty(self):
    config = CrosConfig(self.file)
    pyro = config.models['pyro']
    ec_image = pyro.PathProperty('/firmware', 'ec-image')
    self.assertEqual(ec_image.value, 'bcs://Pyro_EC.9042.87.1.tbz2')

  def testBadPathProperty(self):
    config = CrosConfig(self.file)
    pyro = config.models['pyro']
    self.assertIsNone(pyro.PathProperty('/firmware', 'dne'))
    self.assertIsNone(pyro.PathProperty('/dne', 'ec-image'))

  def testSinglePhandleFollowProperty(self):
    config = CrosConfig(self.file)
    caroline = config.models['caroline']
    bcs_overlay = caroline.PathProperty('/firmware', 'bcs-overlay')
    self.assertEqual(bcs_overlay.value, 'overlay-reef-private')

  def testSinglePhandleFollowNode(self):
    config = CrosConfig(self.file)
    caroline = config.models['caroline']
    target = caroline.PathProperty('/firmware/build-targets', 'coreboot')
    self.assertEqual(target.value, 'caroline')

  def testGetFirmwareUris(self):
    config = CrosConfig(self.file)
    firmware_uris = config.models['pyro'].GetFirmwareUris()
    self.assertSequenceEqual(firmware_uris, [PYRO_BUCKET + fname for
                                             fname in PYRO_FIRMWARE_FILES])

  def testGetSharedFirmwareUris(self):
    config = CrosConfig(self.file)
    firmware_uris = config.GetFirmwareUris()
    expected = sorted(
        [CAROLINE_BUCKET + fname for fname in CAROLINE_FIRMWARE_FILES] +
        [PYRO_BUCKET + fname for fname in PYRO_FIRMWARE_FILES] +
        [REEF_BUCKET + fname for fname in REEF_FIRMWARE_FILES] +
        [BROKEN_BUCKET + fname for fname in BROKEN_FIRMWARE_FILES]
        )
    self.assertSequenceEqual(firmware_uris, expected)

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
    stylus = pyro.PathNode('touch/stylus')
    props = stylus.GetMergedProperties(None, 'touch-type')
    self.assertSequenceEqual(
        props,
        OrderedDict([('version', '4209'),
                     ('vendor', 'wacom'),
                     ('firmware-bin', 'wacom/${version}.hex'),
                     ('firmware-symlink', 'wacom_firmware_${MODEL}.bin')]))

  def testGetMergedPropertiesReef(self):
    config = CrosConfig(self.file)
    reef = config.models['reef']
    touchscreen = reef.PathNode('touch/touchscreen@1')
    props = touchscreen.GetMergedProperties(None, 'touch-type')
    self.assertSequenceEqual(
        props,
        OrderedDict([('pid', '306e'),
                     ('version', '5611')
                     , ('vendor', 'elan'),
                     ('firmware-bin', '${vendor}/${pid}_${version}.bin'),
                     ('firmware-symlink', '${vendor}ts_i2c_${pid}.bin')]))

  def testGetMergedPropertiesDefault(self):
    config = CrosConfig(self.file)
    caroline = config.models['caroline']
    audio = caroline.PathNode('/audio/main')
    props = caroline.GetMergedProperties(audio, 'audio-type')
    self.assertSequenceEqual(
        props,
        OrderedDict([('cras-config-dir', 'caroline'),
                     ('ucm-suffix', 'pyro'),
                     ('topology-name', 'pyro'),
                     ('card', 'bxtda7219max'),
                     ('volume', 'cras-config/${cras-config-dir}/${card}'),
                     ('dsp-ini', 'cras-config/${cras-config-dir}/dsp.ini'),
                     ('hifi-conf',
                      'ucm-config/${card}.${ucm-suffix}/HiFi.conf'),
                     ('alsa-conf',
                      'ucm-config/${card}.${ucm-suffix}/${card}.' +
                      '${ucm-suffix}.conf'),
                     ('topology-bin',
                      'topology/5a98-reef-${topology-name}-8-tplg.bin')]))

  def testGetAudioFiles(self):
    config = CrosConfig(self.file)
    audio_files = config.GetAudioFiles()
    self.assertEqual(
        audio_files,
        [BaseFile('cras-config/1mic/bxtda7219max',
                  '/etc/cras/1mic/bxtda7219max'),
         BaseFile('cras-config/1mic/dsp.ini', '/etc/cras/1mic/dsp.ini'),
         BaseFile('cras-config/2mic/bxtda7219max',
                  '/etc/cras/2mic/bxtda7219max'),
         BaseFile('cras-config/2mic/dsp.ini', '/etc/cras/2mic/dsp.ini'),
         BaseFile('cras-config/caroline/bxtda7219max',
                  '/etc/cras/caroline/bxtda7219max'),
         BaseFile('cras-config/caroline/dsp.ini', '/etc/cras/caroline/dsp.ini'),
         BaseFile('cras-config/pyro/bxtda7219max',
                  '/etc/cras/pyro/bxtda7219max'),
         BaseFile('cras-config/pyro/dsp.ini', '/etc/cras/pyro/dsp.ini'),
         BaseFile('cras-config/reefcras/bxtda7219max',
                  '/etc/cras/reefcras/bxtda7219max'),
         BaseFile('cras-config/reefcras/dsp.ini', '/etc/cras/reefcras/dsp.ini'),

         BaseFile('topology/5a98-reef-1mic-8-tplg.bin',
                  '/lib/firmware/5a98-reef-1mic-8-tplg.bin'),
         BaseFile('topology/5a98-reef-pyro-8-tplg.bin',
                  '/lib/firmware/5a98-reef-pyro-8-tplg.bin'),
         BaseFile('topology/5a98-reef-reeftop-8-tplg.bin',
                  '/lib/firmware/5a98-reef-reeftop-8-tplg.bin'),

         BaseFile('ucm-config/1mic/HiFi.conf',
                  '/usr/share/alsa/ucm/bxtda7219max.1mic/HiFi.conf'),
         BaseFile('ucm-config/1mic/bxtda7219max.conf',
                  '/usr/share/alsa/ucm/bxtda7219max.1mic/bxtda7219max.1mic'
                  '.conf'),
         BaseFile('ucm-config/2mic/Wibble',
                  '/usr/share/alsa/ucm/bxtda7219max.2mic/Wibble'),
         BaseFile('ucm-config/2mic/bxtda7219max.conf',
                  '/usr/share/alsa/ucm/bxtda7219max.2mic/bxtda7219max.2mic'
                  '.conf'),
         BaseFile('ucm-config/bxtda7219max.pyro/HiFi.conf',
                  '/usr/share/alsa/ucm/bxtda7219max.pyro/HiFi.conf'),
         BaseFile('ucm-config/bxtda7219max.pyro/bxtda7219max.pyro.conf',
                  '/usr/share/alsa/ucm/bxtda7219max.pyro/bxtda7219max.pyro' +
                  '.conf'),
         BaseFile('ucm-config/bxtda7219max.reefucm/HiFi.conf',
                  '/usr/share/alsa/ucm/bxtda7219max.reefucm/HiFi.conf'),
         BaseFile('ucm-config/bxtda7219max.reefucm/bxtda7219max.reefucm' +
                  '.conf',
                  '/usr/share/alsa/ucm/bxtda7219max.reefucm/bxtda7219max' +
                  '.reefucm.conf')])

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
        whitetip1.PathProperty('/firmware', 'key-id').value, 'WHITETIP1')

    # This is in a subnode defined by whitetip
    self.assertEqual(whitetip1.PathProperty('/touch', 'present').value, 'yes')

    # This is in the main node, but defined by whitetip
    self.assertEqual(whitetip1.PathProperty('/', 'powerd-prefs').value,
                     'whitetip')

    # This is defined by whitetip's shared firmware
    target = whitetip1.PathProperty('/firmware/build-targets', 'coreboot')
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


  def testWriteTargetDirectories(self):
    """Test that we can write out a list of file paths"""
    config = CrosConfig(self.file)
    target_dirs = config.GetTargetDirectories()
    self.assertEqual(target_dirs['dptf-dv'], '/etc/dptf')
    self.assertEqual(target_dirs['hifi-conf'], '/usr/share/alsa/ucm')
    self.assertEqual(target_dirs['alsa-conf'], '/usr/share/alsa/ucm')
    self.assertEqual(target_dirs['volume'], '/etc/cras')
    self.assertEqual(target_dirs['dsp-ini'], '/etc/cras')
    self.assertEqual(target_dirs['cras-config-dir'], '/etc/cras')

  def testDefault(self):
    """Test the 'default' property"""
    config = CrosConfig(self.file)
    caroline = config.models['caroline']

    # These are defined by caroline itself
    self.assertEqual(caroline.properties['wallpaper'].value, 'caroline')
    self.assertEqual(
        caroline.PathProperty('/audio/main', 'cras-config-dir').value,
        'caroline')

    # This relies on a default property
    self.assertEqual(
        caroline.PathProperty('/audio/main', 'ucm-suffix').value, 'pyro')

  def testSubmodel(self):
    """Test that we can read properties from the submodel"""
    config = CrosConfig(self.file)
    reef = config.models['reef']
    self.assertEqual(
        reef.SubmodelPathProperty('touch', '/audio/main', 'ucm-suffix').value,
        '1mic')
    self.assertEqual(
        reef.SubmodelPathProperty('touch', '/touch', 'present').value, 'yes')
    self.assertEqual(
        reef.SubmodelPathProperty('notouch', '/touch', 'present').value, 'no')

  def testGetProperty(self):
    """Test that we can read properties from non-model nodes"""
    config = CrosConfig(self.file)
    # pylint: disable=protected-access
    self.assertEqual(config._GetProperty('/chromeos/family/firmware',
                                         'script').value, 'updater4.sh')
    firmware = config.GetFamilyNode('/firmware')
    self.assertEqual(firmware.name, 'firmware')
    self.assertEqual(firmware.Property('script').value, 'updater4.sh')
    self.assertEqual(
        config.GetFamilyProperty('/firmware', 'script').value, 'updater4.sh')

  def testDefaultConfig(self):
    with self.assertRaisesRegexp(ValueError,
                                 'No master configuration is available'):
      CrosConfig()

    os.environ['SYSROOT'] = 'fred'
    with self.assertRaises(IOError) as e:
      CrosConfig()
    self.assertIn('fred/usr/share/chromeos-config/config.dtb', str(e.exception))

  def testModelList(self):
    """Test that we can obtain a model list"""
    config = CrosConfig(self.file)
    self.assertEqual(
        ['broken', 'caroline', 'pyro', 'reef', 'whitetip', 'whitetip1',
         'whitetip2'], config.GetModelList())

  def testFirmware(self):
    """Test access to firmware information"""
    config = CrosConfig(self.file)
    self.assertEqual('updater4.sh', config.GetFirmwareScript())

    # Use this to avoid repeating common fields
    caroline = FirmwareInfo(
        model='', shared_model='caroline', key_id='', have_image=True,
        bios_build_target='caroline', ec_build_target='caroline',
        main_image_uri='bcs://Caroline.2017.21.1.tbz2',
        main_rw_image_uri='bcs://Caroline.2017.41.0.tbz2',
        ec_image_uri='bcs://Caroline_EC.2017.21.1.tbz2',
        pd_image_uri='bcs://Caroline_PD.2017.21.1.tbz2',
        extra=[], create_bios_rw_image=False, tools=[], sig_id='')
    self.assertEqual(config.GetFirmwareInfo(), OrderedDict([
        ('broken', FirmwareInfo(
            model='broken', shared_model=None, key_id='', have_image=True,
            bios_build_target=None, ec_build_target=None,
            main_image_uri='bcs://Reef.9042.87.1.tbz2',
            main_rw_image_uri='', ec_image_uri='', pd_image_uri='',
            extra=[], create_bios_rw_image=False, tools=[], sig_id='broken')),
        ('caroline', caroline._replace(model='caroline', sig_id='caroline')),
        ('pyro', FirmwareInfo(
            model='pyro', shared_model=None, key_id='', have_image=True,
            bios_build_target='pyro', ec_build_target='pyro',
            main_image_uri='bcs://Pyro.9042.87.1.tbz2',
            main_rw_image_uri='bcs://Pyro.9042.110.0.tbz2',
            ec_image_uri='bcs://Pyro_EC.9042.87.1.tbz2',
            pd_image_uri='bcs://Pyro_PD.9042.87.1.tbz2',
            extra=[], create_bios_rw_image=False, tools=[], sig_id='pyro')),
        ('reef', FirmwareInfo(
            model='reef', shared_model=None, key_id='', have_image=True,
            bios_build_target='pyro', ec_build_target='pyro',
            main_image_uri='bcs://Reef.9042.87.1.tbz2',
            main_rw_image_uri='bcs://Reef.9042.110.0.tbz2',
            ec_image_uri='bcs://Reef_EC.9042.87.1.tbz2', pd_image_uri='',
            extra=[], create_bios_rw_image=False, tools=[], sig_id='reef')),
        ('whitetip', caroline._replace(model='whitetip',
                                       sig_id='sig-id-in-customization-id')),
        ('whitetip1', caroline._replace(model='whitetip1', key_id='WHITETIP1',
                                        have_image=False, sig_id='whitetip1')),
        ('whitetip2', caroline._replace(model='whitetip2', key_id='WHITETIP2',
                                        have_image=False, sig_id='whitetip2'))
        ]))


if __name__ == '__main__':
  unittest.main()
