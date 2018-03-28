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
import shutil
import sys
import unittest

from . import fdt_util
from .libcros_config_host import BaseFile, CrosConfig, TouchFile, FirmwareInfo
from .libcros_config_host import FORMAT_FDT, FORMAT_YAML
from .libcros_config_host import COMPARE_ALWAYS, COMPARE_NEVER


DTS_FILE = '../libcros_config/test.dts'
YAML_FILE = '../libcros_config/test.yaml'
MODELS = sorted(['pyro', 'caroline', 'reef', 'broken', 'whitetip', 'whitetip1',
                 'whitetip2', 'blacktip'])
PYRO_BUCKET = ('gs://chromeos-binaries/HOME/bcs-pyro-private/'
               'overlay-pyro-private/chromeos-base/chromeos-firmware-pyro/')
REEF_BUCKET = ('gs://chromeos-binaries/HOME/bcs-reef-private/'
               'overlay-reef-private/chromeos-base/chromeos-firmware-reef/')
CAROLINE_BUCKET = REEF_BUCKET
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


def MakeTests(pathname):
  class CrosConfigHostTest(unittest.TestCase):
    """The unit test suite for the libcros_config_host.py library"""
    def setUp(self):
      self.config_format, self.filepath, self.temp_file = (
          self.GetConfig(pathname))
      _, self.filepath_bad_compare, self.temp_file_bad_compare = self.GetConfig(
          pathname.replace('test.', 'test_bad_compare.'))

    def tearDown(self):
      if self.temp_file is not None:
        os.remove(self.temp_file.name)

    def GetConfig(self, pathname):
      path = os.path.join(os.path.dirname(__file__), pathname)
      dts_path = path.replace('.yaml', '.dts')
      yaml_path = path.replace('.dts', '.yaml')
      (dts_filepath, temp_file) = fdt_util.EnsureCompiled(dts_path)
      yaml_dest = dts_filepath.replace('.dtb', '.yaml')
      shutil.copy(yaml_path, yaml_dest)
      if '.dts' in path:
        conf_format = FORMAT_FDT
        filepath = dts_filepath
      else:
        conf_format = FORMAT_YAML
        filepath = yaml_dest
      return conf_format, filepath, temp_file

    def testGoodDtbFile(self):
      self.assertIsNotNone(CrosConfig(self.filepath))

    def testModels(self):
      config = CrosConfig(self.filepath)
      self.assertSequenceEqual(config.GetModelList(), MODELS)

    def testNodeSubnames(self):
      config = CrosConfig(self.filepath)
      for name, model in config.models.iteritems():
        self.assertEqual(name, model.name)

    def testProperties(self):
      config = CrosConfig(self.filepath, compare_results=COMPARE_NEVER)
      pyro = config.models['pyro']
      self.assertEqual(pyro.properties['wallpaper'].value, 'default')
      self.assertEqual(pyro.Property('wallpaper').value, 'default')
      self.assertIsNone(pyro.Property('missing'))

    def testPathNode(self):
      config = CrosConfig(self.filepath)
      self.assertIsNotNone(config.models['pyro'].PathNode('/firmware'))

    def testBadPathNode(self):
      config = CrosConfig(self.filepath)
      self.assertIsNone(config.models['pyro'].PathNode('/dne'))

    def testPathProperty(self):
      config = CrosConfig(self.filepath)
      pyro = config.models['pyro']
      ec_image = pyro.PathProperty('/firmware', 'ec-image')
      self.assertEqual(ec_image.value, 'bcs://Pyro_EC.9042.87.1.tbz2')

    def testBadPathProperty(self):
      config = CrosConfig(self.filepath)
      pyro = config.models['pyro']
      self.assertIsNone(pyro.PathProperty('/firmware', 'dne'))
      self.assertIsNone(pyro.PathProperty('/dne', 'ec-image'))

    def testSinglePhandleFollowProperty(self):
      config = CrosConfig(self.filepath)
      caroline = config.models['caroline']
      bcs_overlay = caroline.PathProperty('/firmware', 'bcs-overlay')
      self.assertEqual(bcs_overlay.value, 'overlay-reef-private')

    def testSinglePhandleFollowNode(self):
      config = CrosConfig(self.filepath)
      caroline = config.models['caroline']
      target = caroline.PathProperty('/firmware/build-targets', 'coreboot')
      self.assertEqual(target.value, 'caroline')

    def testGetFirmwareUris(self):
      config = CrosConfig(self.filepath)
      firmware_uris = config.models['pyro'].GetFirmwareUris()
      self.assertSequenceEqual(
          firmware_uris,
          sorted([PYRO_BUCKET + fname for fname in PYRO_FIRMWARE_FILES]))

    def testGetSharedFirmwareUris(self):
      config = CrosConfig(self.filepath)
      firmware_uris = config.GetFirmwareUris()
      expected = sorted(
          [CAROLINE_BUCKET + fname for fname in CAROLINE_FIRMWARE_FILES] +
          [PYRO_BUCKET + fname for fname in PYRO_FIRMWARE_FILES] +
          [REEF_BUCKET + fname for fname in REEF_FIRMWARE_FILES]
          )
      self.assertSequenceEqual(firmware_uris, expected)

    def testGetTouchFirmwareFiles(self):
      def _GetFile(source, symlink):
        """Helper to return a suitable TouchFile"""
        return TouchFile(source, TOUCH_FIRMWARE + source,
                         LIB_FIRMWARE + symlink)

      os.environ['DISTDIR'] = 'distdir'
      os.environ['FILESDIR'] = 'files'
      config = CrosConfig(self.filepath, compare_results=COMPARE_NEVER)
      touch_files = config.models['pyro'].GetTouchFirmwareFiles()
      self.assertEqual(
          touch_files,
          {'stylus': TouchFile('files/wacom/4209.hex',
                               TOUCH_FIRMWARE + 'wacom/4209.hex',
                               LIB_FIRMWARE + 'wacom_firmware_PYRO.bin'),
           'touchscreen': TouchFile('chromeos-touch-firmware-reef-1.0-r9' +
                                    TOUCH_FIRMWARE + '0a97_1012.bin',
                                    TOUCH_FIRMWARE + 'elan/0a97_1012.bin',
                                    LIB_FIRMWARE + 'elants_i2c_0a97.bin')})
      touch_files = config.models['reef'].GetTouchFirmwareFiles()

      # This checks that duplicate processing works correct, since both models
      # have the same wacom firmware
      self.assertEqual(
          touch_files,
          {'stylus': TouchFile('files/wacom/4209.hex',
                               TOUCH_FIRMWARE + 'wacom/4209.hex',
                               LIB_FIRMWARE + 'wacom_firmware_REEF.bin'),
           'touchpad': TouchFile('chromeos-touch-firmware-reef-1.0-r9' +
                                 TOUCH_FIRMWARE + '97.0_6.0.bin',
                                 TOUCH_FIRMWARE + 'elan/97.0_6.0.bin',
                                 LIB_FIRMWARE + 'elan_i2c_97.0.bin'),
           'touchscreen@0': TouchFile('chromeos-touch-firmware-reef-1.0-r9' +
                                      TOUCH_FIRMWARE + '3062_5602.bin',
                                      TOUCH_FIRMWARE + 'elan/3062_5602.bin',
                                      LIB_FIRMWARE + 'elants_i2c_3062.bin'),
           'touchscreen@1': TouchFile('chromeos-touch-firmware-reef-1.0-r9' +
                                      TOUCH_FIRMWARE + '306e_5611.bin',
                                      TOUCH_FIRMWARE + 'elan/306e_5611.bin',
                                      LIB_FIRMWARE + 'elants_i2c_306e.bin')})
      touch_files = config.GetTouchFirmwareFiles()
      expected = set([
          TouchFile('chromeos-touch-firmware-reef-1.0-r9' +
                    TOUCH_FIRMWARE + '0a97_1012.bin',
                    TOUCH_FIRMWARE + 'elan/0a97_1012.bin',
                    LIB_FIRMWARE + 'elants_i2c_0a97.bin'),
          TouchFile('chromeos-touch-firmware-reef-1.0-r9' +
                    TOUCH_FIRMWARE + '3062_5602.bin',
                    TOUCH_FIRMWARE + 'elan/3062_5602.bin',
                    LIB_FIRMWARE + 'elants_i2c_3062.bin'),
          TouchFile('chromeos-touch-firmware-reef-1.0-r9' +
                    TOUCH_FIRMWARE + '306e_5611.bin',
                    TOUCH_FIRMWARE + 'elan/306e_5611.bin',
                    LIB_FIRMWARE + 'elants_i2c_306e.bin'),
          TouchFile('chromeos-touch-firmware-reef-1.0-r9' +
                    TOUCH_FIRMWARE + '97.0_6.0.bin',
                    TOUCH_FIRMWARE + 'elan/97.0_6.0.bin',
                    LIB_FIRMWARE + 'elan_i2c_97.0.bin'),
          TouchFile('files/wacom/4209.hex',
                    TOUCH_FIRMWARE + 'wacom/4209.hex',
                    LIB_FIRMWARE + 'wacom_firmware_PYRO.bin'),
          TouchFile('files/wacom/4209.hex',
                    TOUCH_FIRMWARE + 'wacom/4209.hex',
                    LIB_FIRMWARE + 'wacom_firmware_WHITETIP.bin'),
          TouchFile('files/wacom/4209.hex',
                    TOUCH_FIRMWARE + 'wacom/4209.hex',
                    LIB_FIRMWARE + 'wacom_firmware_REEF.bin')])
      if self.config_format == FORMAT_FDT:
        expected |= set([
            TouchFile('files/wacom/4209.hex',
                      TOUCH_FIRMWARE + 'wacom/4209.hex',
                      LIB_FIRMWARE + 'wacom_firmware_WHITETIP1.bin'),
            TouchFile('files/wacom/4209.hex',
                      TOUCH_FIRMWARE + 'wacom/4209.hex',
                      LIB_FIRMWARE + 'wacom_firmware_WHITETIP2.bin')])
      self.assertEqual(set(touch_files), expected)

    def testGetTouchFirmwareFilesTar(self):
      """Test unpacking from a tarfile or reading from ${FILESDIR}"""
      os.environ['FILESDIR'] = 'test'
      config = CrosConfig(self.filepath)
      touch_files = config.models['pyro'].GetTouchFirmwareFiles()
      self.assertEqual(
          touch_files,
          {'stylus': TouchFile('test/wacom/4209.hex',
                               TOUCH_FIRMWARE + 'wacom/4209.hex',
                               LIB_FIRMWARE + 'wacom_firmware_PYRO.bin'),
           'touchscreen': TouchFile('chromeos-touch-firmware-reef-1.0-r9' +
                                    TOUCH_FIRMWARE + '0a97_1012.bin',
                                    TOUCH_FIRMWARE + 'elan/0a97_1012.bin',
                                    LIB_FIRMWARE + 'elants_i2c_0a97.bin')})
      touch_files = config.models['reef'].GetTouchFirmwareFiles()

    def testGetMergedPropertiesPyro(self):
      config = CrosConfig(self.filepath)
      pyro = config.models['pyro']
      stylus = pyro.PathNode('touch/stylus')
      props = pyro.GetMergedProperties(stylus, 'touch-type')
      self.assertSequenceEqual(
          props,
          {'version': '4209',
           'vendor': 'wacom',
           'firmware-bin': 'wacom/{version}.hex',
           'firmware-symlink': 'wacom_firmware_{MODEL}.bin'})

    def testGetMergedPropertiesReef(self):
      config = CrosConfig(self.filepath)
      reef = config.models['reef']
      touchscreen = reef.PathNode('touch/touchscreen@1')
      props = reef.GetMergedProperties(touchscreen, 'touch-type')
      self.assertEqual(
          props,
          {'pid': '306e',
           'version': '5611',
           'vendor': 'elan',
           'firmware-bin': '{vendor}/{pid}_{version}.bin',
           'firmware-symlink': '{vendor}ts_i2c_{pid}.bin'})

    def testGetMergedPropertiesDefault(self):
      """Test that the 'default' property is used when collecting properties"""
      if self.config_format != FORMAT_FDT:
        return
      config = CrosConfig(self.filepath, compare_results=COMPARE_NEVER)
      caroline = config.models['caroline']
      audio = caroline.PathNode('/audio/main')
      props = caroline.GetMergedProperties(audio, 'audio-type')
      self.assertSequenceEqual(
          props,
          {'cras-config-dir': 'caroline',
           'ucm-suffix': 'pyro',
           'topology-name': 'pyro',
           'card': 'bxtda7219max',
           'volume': 'cras-config/{cras-config-dir}/{card}',
           'dsp-ini': 'cras-config/{cras-config-dir}/dsp.ini',
           'hifi-conf': 'ucm-config/{card}.{ucm-suffix}/HiFi.conf',
           'alsa-conf': 'ucm-config/{card}.{ucm-suffix}/{card}.' +
                        '{ucm-suffix}.conf',
           'topology-bin': 'topology/5a98-reef-{topology-name}-8-tplg.bin'})

    def testGetArcFiles(self):
      config = CrosConfig(self.filepath)
      arc_files = config.GetArcFiles()
      self.assertEqual(
          arc_files,
          [BaseFile('reef/arc++/hardware_features',
                    '/usr/share/chromeos-config/sbin/reef/arc++/'
                    'hardware_features')])

    def testGetAudioFiles(self):
      config = CrosConfig(self.filepath, compare_results=COMPARE_NEVER)
      audio_files = config.GetAudioFiles()
      expected = [
          BaseFile('cras-config/1mic/bxtda7219max',
                   '/etc/cras/1mic/bxtda7219max'),
          BaseFile('cras-config/1mic/dsp.ini', '/etc/cras/1mic/dsp.ini'),
          BaseFile('cras-config/2mic/bxtda7219max',
                   '/etc/cras/2mic/bxtda7219max'),
          BaseFile('cras-config/2mic/dsp.ini', '/etc/cras/2mic/dsp.ini'),
          BaseFile('cras-config/pyro/bxtda7219max',
                   '/etc/cras/pyro/bxtda7219max'),
          BaseFile('cras-config/pyro/dsp.ini', '/etc/cras/pyro/dsp.ini'),
          BaseFile('cras-config/reefcras/bxtda7219max',
                   '/etc/cras/reefcras/bxtda7219max'),
          BaseFile('cras-config/reefcras/dsp.ini',
                   '/etc/cras/reefcras/dsp.ini'),

          BaseFile('topology/5a98-reef-1mic-8-tplg.bin',
                   LIB_FIRMWARE + '5a98-reef-1mic-8-tplg.bin'),
          BaseFile('topology/5a98-reef-pyro-8-tplg.bin',
                   LIB_FIRMWARE + '5a98-reef-pyro-8-tplg.bin'),
          BaseFile('topology/5a98-reef-reeftop-8-tplg.bin',
                   LIB_FIRMWARE + '5a98-reef-reeftop-8-tplg.bin'),

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
                   '.reefucm.conf')]
      if self.config_format == FORMAT_FDT:
        expected += (BaseFile('cras-config/caroline/bxtda7219max',
                              '/etc/cras/caroline/bxtda7219max'),
                     BaseFile('cras-config/caroline/dsp.ini',
                              '/etc/cras/caroline/dsp.ini'))

      self.assertEqual(audio_files, sorted(expected))

    def testBadAudioFiles(self):
      path = os.path.join(os.path.dirname(__file__),
                          '../libcros_config/test_bad_audio.dts')
      (filepath, temp_file) = fdt_util.EnsureCompiled(path)
      config = CrosConfig(filepath)
      os.remove(temp_file.name)
      with self.assertRaisesRegexp(
          ValueError, "pyro/audio/main': Should have a cras-config-dir"):
        config.GetAudioFiles()

    def testGetThermalFiles(self):
      config = CrosConfig(self.filepath)
      thermal_files = config.GetThermalFiles()
      self.assertEqual(
          thermal_files,
          [BaseFile('pyro/dptf.dv', '/etc/dptf/pyro/dptf.dv'),
           BaseFile('reef_notouch/dptf.dv', '/etc/dptf/reef_notouch/dptf.dv'),
           BaseFile('reef_touch/dptf.dv', '/etc/dptf/reef_touch/dptf.dv')])

    def testWhitelabel(self):
      if self.config_format != FORMAT_FDT:
        return
      # These mirror the tests in cros_config_unittest.cc CheckWhiteLabel
      # Note that we have no tests for the alternative whitelabel schema. In
      # that case the key-id and brand-code are 1:many with the model and we
      # need a separate identifier to determine which to use. For now, there are
      # no users in the build system, so we can ignore it.
      config = CrosConfig(self.filepath)
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
      config = CrosConfig(self.filepath)
      self.assertSequenceEqual(config.GetFirmwareBuildTargets('coreboot'),
                               ['caroline', 'pyro'])
      os.environ['FW_NAME'] = 'pyro'
      self.assertSequenceEqual(config.GetFirmwareBuildTargets('coreboot'),
                               ['pyro'])
      del os.environ['FW_NAME']

    def testFileTree(self):
      """Test that we can obtain a file tree"""
      os.environ['DISTDIR'] = 'distdir'
      os.environ['FILESDIR'] = 'files'
      config = CrosConfig(self.filepath, compare_results=COMPARE_NEVER)
      node = config.GetFileTree()
      self.assertEqual(node.name, '')
      self.assertEqual(sorted(node.children.keys()),
                       ['etc', 'lib', 'opt', 'usr'])
      etc = node.children['etc']
      self.assertEqual(etc.name, 'etc')
      cras = etc.children['cras']
      self.assertEqual(cras.name, 'cras')
      basking = cras.children['pyro']
      self.assertEqual(sorted(basking.children.keys()),
                       ['bxtda7219max', 'dsp.ini'])

    def testShowTree(self):
      """Test that we can show a file tree"""
      config = CrosConfig(self.filepath, compare_results=COMPARE_NEVER)
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
      config = CrosConfig(self.filepath)
      target_dirs = config.GetTargetDirectories()
      self.assertEqual(target_dirs['dptf-dv'], '/etc/dptf')
      self.assertEqual(target_dirs['hifi-conf'], '/usr/share/alsa/ucm')
      self.assertEqual(target_dirs['alsa-conf'], '/usr/share/alsa/ucm')
      self.assertEqual(target_dirs['volume'], '/etc/cras')
      self.assertEqual(target_dirs['dsp-ini'], '/etc/cras')
      self.assertEqual(target_dirs['cras-config-dir'], '/etc/cras')

    def testDefault(self):
      """Test the 'default' property"""
      if self.config_format != FORMAT_FDT:
        return
      config = CrosConfig(self.filepath, compare_results=COMPARE_NEVER)
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
      config = CrosConfig(self.filepath)
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
        CrosConfig(config_format=self.config_format,
                   compare_results=COMPARE_NEVER)
      ext = 'dtb' if self.config_format == FORMAT_FDT else 'yaml'
      self.assertIn('fred/usr/share/chromeos-config/config.%s' % ext,
                    str(e.exception))

    def testModelList(self):
      """Test that we can obtain a model list"""
      config = CrosConfig(self.filepath)
      self.assertEqual(
          ['blacktip', 'broken', 'caroline', 'pyro', 'reef', 'whitetip',
           'whitetip1', 'whitetip2'], config.GetModelList())

    def testFirmware(self):
      """Test access to firmware information"""
      config = CrosConfig(self.filepath, compare_results=COMPARE_NEVER)
      self.assertEqual('updater4.sh', config.GetFirmwareScript())

      # YAML does not support naming of the target node so far as I can see.
      shared_model = ('caroline' if self.config_format == FORMAT_FDT
                      else 'shares')

      # Use this to avoid repeating common fields
      caroline = FirmwareInfo(
          model='', shared_model=shared_model, key_id='', have_image=True,
          bios_build_target='caroline', ec_build_target='caroline',
          main_image_uri='bcs://Caroline.2017.21.1.tbz2',
          main_rw_image_uri='bcs://Caroline.2017.41.0.tbz2',
          ec_image_uri='bcs://Caroline_EC.2017.21.1.tbz2',
          pd_image_uri='bcs://Caroline_PD.2017.21.1.tbz2',
          extra=[], create_bios_rw_image=False, tools=[], sig_id='')
      expected = OrderedDict([
          ('blacktip', caroline._replace(model='blacktip',
                                         sig_id='sig-id-in-customization-id')),
          ('blacktip-blacktip1', caroline._replace(
              model='blacktip-blacktip1', key_id='BLACKTIP1', have_image=False,
              sig_id='blacktip-blacktip1')),
          ('blacktip-blacktip2', caroline._replace(
              model='blacktip-blacktip2', key_id='BLACKTIP2', have_image=False,
              sig_id='blacktip-blacktip2')),
          ('blacktip-blacktip3', caroline._replace(
              model='blacktip-blacktip3', key_id='BLACKTIP3', have_image=False,
              sig_id='blacktip-blacktip3')),
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
          ])
      result = config.GetFirmwareInfo()
      # With FDT we support models which are whitelabels of another model.
      # Filter whitetip2 out for other formats.
      if self.config_format == FORMAT_FDT:
        expected['whitetip2'] = caroline._replace(
            model='whitetip2', key_id='WHITETIP2', have_image=False,
            sig_id='whitetip2')
      else:
        del result['whitetip2']
      self.assertEqual(result, expected)

    def testFimwareBuildCombinations(self):
      """Test generating a dict of firmware build combinations."""
      config = CrosConfig(self.filepath, compare_results=COMPARE_NEVER)
      expected = OrderedDict([
          ('caroline', ['caroline', 'caroline']),
          ('pyro', ['pyro', 'pyro']),
          ])
      result = config.GetFirmwareBuildCombinations(['coreboot', 'depthcharge'])
      self.assertEqual(result, expected)

      os.environ['FW_NAME'] = 'pyro'
      expected = OrderedDict([('pyro', ['pyro', 'pyro'])])
      result = config.GetFirmwareBuildCombinations(['coreboot', 'depthcharge'])
      self.assertEqual(result, expected)
      del os.environ['FW_NAME']

    def testGetBspUris(self):
      """Test access to the BSP URIs"""
      config = CrosConfig(self.filepath)
      uris = config.GetBspUris()
      self.assertSequenceEqual(uris, [
          'gs://chromeos-binaries/HOME/bcs-reef-private/overlay-reef-private/'
          'chromeos-base/chromeos-touch-firmware-reef/'
          'chromeos-touch-firmware-reef-1.0-r9.tbz2'])

    def testCompareResults(self):
      """Test that CrosConfig can compare config results for the two formats"""
      config = CrosConfig(self.filepath)
      caroline = config.models['caroline']
      self.assertEqual(caroline.properties['wallpaper'].value, 'caroline')

      config.models['pyro'].GetFirmwareUris()

      pyro = config.models['pyro']
      stylus = pyro.PathNode('touch/stylus')
      pyro.GetMergedProperties(stylus, 'touch-type')

    # TODO(shapiroc): Re-enable once YAML refactor is complete
    def ignoreCompareResultsBad(self):
      """Test that mismatches are detected"""
      config = CrosConfig(self.filepath_bad_compare,
                          compare_results=COMPARE_ALWAYS)
      caroline = config.models['caroline']
      with self.assertRaisesRegexp(ValueError, 'results differ.*not-caroline'):
        self.assertEqual(caroline.PathProperty('/', 'wallpaper').value,
                         'caroline')

      with self.assertRaisesRegexp(ValueError,
                                   'results differ.*notpyro-private'):
        config.models['pyro'].GetFirmwareUris()

      pyro = config.models['pyro']
      stylus = pyro.PathNode('touch/stylus')
      with self.assertRaisesRegexp(ValueError,
                                   'Properties.*differ.*4209.*4210'):
        pyro.GetMergedProperties(stylus, 'touch-type')

    def testGetWallpaper(self):
      """Test that we can access the wallpaper information"""
      config = CrosConfig(self.filepath)
      self.assertEquals(['caroline', 'dark', 'darker', 'default', 'epic',
                         'more_shark', 'shark'],
                        config.GetWallpaperFiles())

  return CrosConfigHostTest


class CrosConfigHostTestFdt(MakeTests(DTS_FILE)):
  """Tests for master configuration in device tree format"""

# TODO(shapiroc): Re-enable this after YAML is fully fixed up.
# class CrosConfigHostTestYaml(MakeTests(YAML_FILE)):
#  """Tests for master configuration in yaml format"""


if __name__ == '__main__':
  unittest.main()
