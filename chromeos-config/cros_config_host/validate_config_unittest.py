#!/usr/bin/env python2
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for the config validator"""

from __future__ import print_function

import os
import subprocess
import tempfile

from chromite.lib import cros_test_lib

from . import validate_config


HEADER = '''/dts-v1/;

/ {
  chromeos {
    family: family {
    };

    models: models {
    };
    schema {
      target-dirs {
        dptf-dv = "/etc/dptf";
      };
    };
  };
};
'''

MODELS = '''
&models {
  reef: reef {
  };
  pyro: pyro {
  };
  snappy: snappy {
  };
};
'''

FAMILY_FIRMWARE_MISSING = '''
&family {
  firmware {
    script = "updater4.sh";
    shared: reef {
      ec-image = "bcs://Reef_EC.9984.0.0.tbz2";
      main-image = "bcs://Reef.9984.0.0.tbz2";
    };
  };
};
'''

FAMILY_FIRMWARE_SCRIPT_MISSING = '''
&family {
  firmware {
  };
};
'''

FAMILY_FIRMWARE_BAD_SHARES = '''
&family {
  firmware {
    shared: reef {
      shares = <&shared>;
    };
  };
};
'''

FAMILY_FIRMWARE = FAMILY_FIRMWARE_MISSING + '''
&family {
  firmware {
    script = "updater4.sh";
    shared: reef {
      bcs-overlay = "overlay-reef-private";
      build-targets {
        coreboot = "reef";
        ec = "reef";
        depthcharge = "reef";
        libpayload = "reef";
      };
    };
  };
};
'''

MODEL_FIRMWARE_SHARED_WRONG_TARGET = '''
&family {
  firmware {
    shared: reef {
      build_targets: build-targets {
        coreboot = "reef";
        ec = "reef";
        depthcharge = "reef";
        libpayload = "reef";
      };
    };
  };
};

&models {
  reef {
    firmware {
      shares = < &build_targets >;
      key-id = "REEF";
    };
  };
};
'''

MODEL_FIRMWARE_SHARED = '''
&models {
  reef {
    firmware {
      shares = < &shared >;
      key-id = "REEF";
    };
  };
};
'''

MODEL_FIRMWARE_SHARE_EXTRA_PROPS = '''
&models {
  reef {
    firmware {
      shares = < &shared >;
      key-id = "REEF";
      bcs-overlay = "overlay-reef-private";
    };
  };
};
'''

MODEL_FIRMWARE_SHARE_EXTRA_NODES = '''
&models {
  reef {
    firmware {
      shares = < &shared >;
      key-id = "REEF";
      build-targets {
        coreboot = "reef";
      };
    };
  };
};
'''

MODEL_BRAND_CODE = '''
&models {
  reef {
    brand-code = "ABCD";
  };
  pyro{
    brand-code = "abcd";
  };
  snappy{
    brand-code = "AB1";
  };
};
'''

MODEL_THERMAL = '''
&models {
  reef {
    thermal {
      dptf-dv = "reef/dptf.dv";
    };
  };
  pyro{
    thermal {
      dptf-dv = "dptf.dv";
    };
  };
  snappy{
    thermal {
      dptf-dv = "reef/bad.dv";
    };
  };
};
'''

TOUCH = r'''
&family {
  touch {
    elan_touchscreen: elan-touchscreen {
      vendor = "elan";
      firmware-bin = "{vendor}/{pid}_{version}.bin";
      firmware-symlink = "{vendor}ts_i2c_{pid}.bin";
    };
    weida_touchscreen: weida-touchscreen {
        vendor = "weida";
        firmware-bin = "weida/{pid}_{version}_{date-code}.bin";
        firmware-symlink = "wdt87xx.bin";
    };
  };
};
&models {
  reef {
    touch {
      #address-cells = <1>;
      #size-cells = <0>;
      present = "probe";
      probe-regex = "[Tt]ouchscreen\|WCOMNTN2";
      touchscreen@0 {
        reg = <0>;
        touch-type = <&elan_touchscreen>;
        pid = "0a97";
        version = "1012";
      };
      touchscreen@1 {
        reg = <1>;
        touch-type = <&shared>;
        pid = "0b10";
        version = "002n";
      };
      touchscreen@2 {
        reg = <2>;
        touch-type = <&weida_touchscreen>;
        pid = "01017401";
        version = "2082";
        date-code = "0133c65b";
      };
      bad {
      };
    };
  };
};
'''

AUDIO = r'''
&family {
  audio {
      audio_type: audio-type {
        card = "bxtda7219max";
        volume = "cras-config/{cras-config-dir}/{card}";
        dsp-ini = "cras-config/{cras-config-dir}/dsp.ini";
        hifi-conf = "ucm-config/{card}.{ucm-suffix}/HiFi.conf";
        alsa-conf = "ucm-config/{card}.{ucm-suffix}/{card}.{ucm-suffix}.conf";
        topology-bin = "topology/5a98-reef-{topology-name}-8-tplg.bin";
      };
      bad_audio_type: bad-audio-type {
        volume = "cras-config/{cras-config-dir}/{card}";
        dsp-ini = "cras-config/{cras-config-dir}/dsp.ini";
        hifi-conf = "ucm-config/{card}.{ucm-suffix}/HiFi.conf";
        alsa-conf = "ucm-config/{card}.{ucm-suffix}/{card}.{ucm-suffix}.conf";
        topology-bin = "topology/5a98-reef-{topology-name}-8-tplg.bin";
      };
  };
};
&models {
  pyro: pyro {
    powerd-prefs = "pyro_snappy";
    wallpaper = "alien_invasion";
    brand-code = "ABCE";
    audio {
      main {
        audio-type = <&audio_type>;
        cras-config-dir = "pyro";
        ucm-suffix = "pyro";
        topology-name = "pyro";
      };
    };
  };
  pyro: pyro {
    powerd-prefs = "pyro_snappy";
    wallpaper = "alien_invasion";
    brand-code = "ABCE";
    audio {
      main {
        audio-type = <&audio_type>;
        ucm-suffix = "pyro";
        topology-name = "pyro";
      };
    };
  };
  snappy: snappy {
    powerd-prefs = "pyro_snappy";
    wallpaper = "chocolate";
    brand-code = "ABCF";
    audio {
      main {
        cras-config-dir = "snappy";
        ucm-suffix = "snappy";
        topology-name = "snappy";
      };
    };
  };
};
'''

POWER = r'''
&family {
  power {
    power_type: power_type {
      power-supply-full-factor = "0.10";
      suspend-to-idle = "1";
    };
    bad_power_type: bad-power-type {
      low-battery-shutdown-percent = "110";
      power-supply-full-factor = "ten point one";
      suspend-to-idle = "2";
    };
  };
};
&models {
  pyro: pyro {
    power {
      power-type = <&power_type>;
      charging-ports = "CROS_USB_PD_CHARGER0 LEFT\nCROS_USB_PD_CHARGER1 RIGHT";
      power-supply-full-factor = "0.12";
    };
  };
};
'''

MAPPING = '''
&family {
  mapping {
    #address-cells = <1>;
    #size-cells = <0>;
    sku-map@0 {
      reg = <0>;
      platform-name = "Reef";
      smbios-name-match = "reef";
      /* This is an example! It does not match any real family */
      simple-sku-map = <
        (-1) &pyro
        0 &reef
        4 &reef_4
        4 &snappy    /* duplicate */
        5 &reef_5
        8 &shared
        256 &reef_5>;
    };
    sku-map@1 {
      reg = <1>;
      platform-name = "Pyro";
      smbios-name-match = "pyro";
      single-sku = <&pyro>;
    };
    sku-map@2 {
      reg = <2>;
      platform-name = "Snappy";
      smbios-name-match = "snappy";
      single-sku = <&snappy>;
    };
  };
};
&models {
  reef {
    /*
     * We don't have submodel validation, but add this in here so that mapping
     * validation is complete.
     */
    submodels {
      reef_4: reef-touchscreen {
      };
      reef_5: reef-notouch {
      };
    };
  };
};
'''

WHITELABEL = '''
&models {
  /* Whitelabel model */
  whitetip: whitetip {
    firmware {
      shares = <&shared>;
    };
  };

  whitetip1 {
    whitelabel = <&whitetip>;
    wallpaper = "shark";
    brand-code = "SHAR";
    firmware {
      key-id = "WHITELABEL1";
    };
  };

  whitetip2 {
    whitelabel = <&whitetip>;
    wallpaper = "more_shark";
    brand-code = "SHAQ";
    firmware {
      key-id = "WHITELABEL2";
    };
  };

  bad {
    whitelabel = <&whitetip>;
    firmware {
      shares = <&shared>;
      key-id = "WHITELABEL2";
    };
    thermal {
      dptf-dv = "bad/dptf.dv";
    };
  };
};
'''

DEFAULT_MODEL = '''
&models {
  /* Model which gets most of its settings from another */
  other {
    default = <&reef>;
  };
};
'''

class UnitTests(cros_test_lib.TestCase):
  """Unit tests for CrosConfigValidator

  Properties:
    val: Validator to use
    returncode: Holds the return code for the case where the validator is called
        through its command-line interface
  """
  def setUp(self):
    self.val = validate_config.CrosConfigValidator(validate_config.SCHEMA,
                                                   False)
    self.returncode = 0

  def Run(self, dts_source, use_command_line=False, extra_options=None):
    """Run the validator with a single source file

    Args:
      dts_source: String containing the device-tree source to process
      use_command_line: True to run through the command-line interface.
          Otherwise the imported validator class is used directly. When using
          the command-line interface, the return code is available in
          self.returncode, since only one test needs it.
      extra_options: Extra command-line arguments to pass
    """
    dts = tempfile.NamedTemporaryFile(suffix='.dts', delete=False)
    dts.write(dts_source)
    dts.close()
    self.returncode = 0
    if use_command_line:
      call_args = ['python', '-m', 'cros_config_host.validate_config',
                   '-d', dts.name]
      if extra_options:
        call_args += extra_options
      try:
        output = subprocess.check_output(call_args, stderr=subprocess.STDOUT)
      except subprocess.CalledProcessError as e:
        output = e.output
        self.returncode = e.returncode
      errors = output.strip().splitlines()
    else:
      errors = self.val.Start([dts.name])
    if errors:
      return errors
    if dts:
      os.unlink(dts.name)
    return []

  def RunMultiple(self, dts_source_list, use_command_line=False):
    """Run the validator with a list of .dtsi fragments

    Args:
      dts_source_list: List of strings, containing the device-tree source to
          process for each fragment
      use_command_line: True to run through the command-line interface.
          Otherwise the imported validator class is used directly. When using
          the command-line interface, the return code is available in
          self.returncode, since only one test needs it.
    """
    dts_list = []
    for source in dts_source_list:
      dts = tempfile.NamedTemporaryFile(suffix='.dtsi', delete=False)
      dts_list.append(dts)
      dts.write(source)
      dts.close()
    fnames = [dts.name for dts in dts_list]
    self.returncode = 0
    if use_command_line:
      call_args = ['python', '-m', 'cros_config_host.validate_config',
                   '-d', '-p'] + fnames
      try:
        output = subprocess.check_output(call_args, stderr=subprocess.STDOUT)
      except subprocess.CalledProcessError as e:
        output = e.output
        self.returncode = e.returncode
      errors = output.strip().splitlines()
    else:
      errors = self.val.Start(fnames, partial=True)
    if errors:
      return errors
    if dts:
      os.unlink(dts.name)
    return []

  def _CheckAllIn(self, err_msg_list, result_lines):
    """Check that the given messages appear in the validation result

    All messages must appear, and all lines must be matches.

    Args:
      result_lines: List of validation results to check, each a string
      err_msg_list: List of error messages to check for
    """
    err_msg_set = set(err_msg_list)
    for line in result_lines:
      found = False
      for err_msg in err_msg_set:
        if err_msg in line:
          err_msg_set.remove(err_msg)
          found = True
          break
      if not found:
        self.fail("Found unexpected result: %s" % line)
    if err_msg_set:
      self.fail("Expected '%s'\n but not found in result: %s" %
                (err_msg_set.pop(), '\n'.join(result_lines)))

  def testBase(self):
    """Test a skeleton file"""
    self.assertEqual([], self.Run(HEADER))

  def testModels(self):
    """Test a skeleton file with models"""
    self.assertEqual([], self.Run(HEADER + MODELS))

  def testFamilyFirmwareMissing(self):
    """Test family firmware with missing pieces"""
    self.assertEqual([
        "/chromeos/family/firmware/reef: Required property 'bcs-overlay' " +
        "missing",
        "/chromeos/family/firmware/reef: Missing subnode 'build-targets'",
        ], self.Run(HEADER + MODELS + FAMILY_FIRMWARE_MISSING))

  def testFamilyFirmwareScriptMissing(self):
    """Test a family firmware without a script"""
    self.assertIn(
        "/chromeos/family/firmware: Required property 'script' missing",
        self.Run(HEADER + MODELS + FAMILY_FIRMWARE_SCRIPT_MISSING))

  def testFamilyFirmware(self):
    """Test valid family firmware"""
    self.assertEqual([], self.Run(HEADER + MODELS + FAMILY_FIRMWARE))

  def testFamilyFirmwareSharedWrongTarget(self):
    """Test a model trying to share an invalid firmware target"""
    result = self.Run(HEADER + MODELS + FAMILY_FIRMWARE +
                      MODEL_FIRMWARE_SHARED_WRONG_TARGET)
    self.assertEqual(
        ['/chromeos/family/firmware/reef/build-targets: phandle target not '
         'valid for this node',
         "/chromeos/models/reef/firmware: Phandle 'shares' " +
         "targets node '/chromeos/family/firmware/reef/build-targets' "
         "which does not match pattern '/chromeos/family/firmware/ANY'",
        ], result)

  def testFamilyFirmwareBadShares(self):
    """Test a model trying to share an invalid firmware target"""
    result = self.Run(HEADER + MODELS + FAMILY_FIRMWARE +
                      FAMILY_FIRMWARE_BAD_SHARES)
    self.assertEqual(
        ["/chromeos/family/firmware/reef: Unexpected property 'shares', " +
         "valid list is (phandle, bcs-overlay, ec-image, main-image, " +
         "main-rw-image, pd-image, extra)"], result)

  def testFamilyFirmwareShared(self):
    """Test valid shared firmware"""
    result = self.Run(HEADER + MODELS + FAMILY_FIRMWARE +
                      MODEL_FIRMWARE_SHARED)
    self.assertEqual([], result)

  def testFamilyFirmwareSharedExtraProps(self):
    """Test the model trying to specify properties that should be shared"""
    result = self.Run(HEADER + MODELS + FAMILY_FIRMWARE +
                      MODEL_FIRMWARE_SHARE_EXTRA_PROPS)
    self.assertEqual(
        ["/chromeos/models/reef/firmware: Unexpected property 'bcs-overlay', "
         'valid list is (shares, sig-id-in-customization-id, key-id, '
         'no-firmware)'], result)

  def testFamilyFirmwareSharedExtraNodes(self):
    """Test the model trying to specify nodes that should be shared"""
    result = self.Run(HEADER + MODELS + FAMILY_FIRMWARE +
                      MODEL_FIRMWARE_SHARE_EXTRA_NODES)
    self.assertEqual(
        ["/chromeos/models/reef/firmware: Unexpected subnode 'build-targets', "
         "valid list is ()"], result)

  def testBrandCode(self):
    """Test validation of brand codes"""
    self.assertEqual([
        "/chromeos/models/pyro: 'brand-code' value 'abcd' does not match " +
        "pattern '^[A-Z]{4}$'",
        "/chromeos/models/snappy: 'brand-code' value 'AB1' does not match " +
        "pattern '^[A-Z]{4}$'"], self.Run(HEADER + MODELS + MODEL_BRAND_CODE))

  def testThermal(self):
    """Test validation of the thermal node"""
    self.assertEqual([
        "/chromeos/models/pyro/thermal: 'dptf-dv' value 'dptf.dv' does not "
        "match pattern '^\\w+/dptf.dv$'",
        "/chromeos/models/snappy/thermal: 'dptf-dv' value 'reef/bad.dv' does " +
        "not match pattern '^\\w+/dptf.dv$'"
        ], self.Run(HEADER + MODELS + MODEL_THERMAL))

  def testTouch(self):
    """Test validation of the thermal node"""
    result = self.Run(HEADER + MODELS + FAMILY_FIRMWARE + TOUCH)
    self._CheckAllIn([
        "Node name 'bad' does not match pattern",
        "/bad: Required property 'version' missing",
        "/bad: Required property 'firmware-bin' missing",
        "/bad: Required property 'firmware-symlink' missing",
        "/touchscreen@1: Phandle 'touch-type' targets node",
        ], result)

  def testAudio(self):
    """Test validation of the audio nodes"""
    result = self.Run(HEADER + MODELS + AUDIO)
    self._CheckAllIn([
        "snappy/audio/main: Required property 'card' missing",
        "snappy/audio/main: Required property 'volume' missing",
        "snappy/audio/main: Required property 'dsp-ini' missing",
        "snappy/audio/main: Required property 'hifi-conf' missing",
        "snappy/audio/main: Required property 'alsa-conf' missing",
        "bad-audio-type: Required property 'card' missing",
        ], result)

  def testPower(self):
    """Test validation of the power nodes"""
    result = self.Run(HEADER + MODELS + POWER)
    self._CheckAllIn([
        "/chromeos/family/power/bad-power-type: 'power-supply-full-factor' " +
        "value 'ten point one' is not a float",
        "/chromeos/family/power/bad-power-type: 'suspend-to-idle' value '2' " +
        "does not match pattern '^0|1$'",
        "/chromeos/family/power/bad-power-type: " +
        "'low-battery-shutdown-percent' value '110' is out of range [0..100]",
        ], result)

  def testMapping(self):
    """Test validation of the mapping node"""
    result = self.Run(HEADER + MODELS + FAMILY_FIRMWARE + MAPPING)
    self._CheckAllIn([
        'mapping/sku-map@0: Duplicate sku_id 4',
        'mapping/sku-map@0: sku_id 256 out of range',
        "mapping/sku-map@0: Phandle 'simple-sku-map' sku-id 8 must target a " +
        'model or submodel',
        ], result)

  def testWhiteLabel(self):
    result = self.Run(HEADER + MODELS + FAMILY_FIRMWARE + WHITELABEL)
    self._CheckAllIn([
        "/bad: Unexpected subnode 'thermal', valid list is (",
        "bad/firmware: Unexpected property 'shares', valid list is "
        '(key-id, no-firmware)',
        ], result)

  def testPartial(self):
    """Test that we can validate config coming from multiple .dtsi files"""
    result = self.RunMultiple([MODELS, FAMILY_FIRMWARE, WHITELABEL])
    self._CheckAllIn([
        "/bad: Unexpected subnode 'thermal', valid list is (",
        "bad/firmware: Unexpected property 'shares', valid list is "
        '(key-id, no-firmware)',
        ], result)

  def testComanndLine(self):
    """Test that the command-line interface works correctly"""
    self.assertEqual([], self.Run(HEADER, True))

    output = self.Run(HEADER, True, ['--invalid-option'])
    self.assertEqual(self.returncode, 2)
    self._CheckAllIn([
        'usage: validate_config.py',
        'error: unrecognized arguments: --invalid-option'
        ], output)

    result = self.Run(HEADER + MODELS + FAMILY_FIRMWARE + WHITELABEL, True)
    self.assertEqual(self.returncode, 1)
    self._CheckAllIn([
        '/tmp/',
        "/bad: Unexpected subnode 'thermal', valid list is (",
        "bad/firmware: Unexpected property 'shares', valid list is "
        '(key-id, no-firmware)',
        ], result)

    result = self.RunMultiple([MODELS, FAMILY_FIRMWARE, WHITELABEL], True)
    self.assertEqual(self.returncode, 1)
    self._CheckAllIn([
        '/tmp/',
        "/bad: Unexpected subnode 'thermal', valid list is (",
        "bad/firmware: Unexpected property 'shares', valid list is "
        '(key-id, no-firmware)',
        ], result)

  def testDefault(self):
    result = self.Run(HEADER + MODELS + FAMILY_FIRMWARE + DEFAULT_MODEL)
    self.assertEqual([], result)

if __name__ == '__main__':
  cros_test_lib.main(module=__name__)
