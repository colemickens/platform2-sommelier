#!/usr/bin/env python2
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for the config validator"""

from __future__ import print_function

import os
import tempfile

from chromite.lib import cros_test_lib

import validate_config


HEADER = '''/dts-v1/;

/ {
  chromeos {
    family: family {
    };

    models: models {
    };
  };
};
'''

MODELS = '''
&models {
  reef {
  };
  pyro {
  };
  snappy {
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
      firmware-bin = "${vendor}/${pid}_${version}.bin";
      firmware-symlink = "${vendor}ts_i2c_${pid}.bin";
    };
    weida_touchscreen: weida-touchscreen {
        vendor = "weida";
        firmware-bin = "weida/${pid}_${version}_${date-code}.bin";
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

class UnitTests(cros_test_lib.TestCase):
  """Unit tests for CrosConfigValidator"""
  def setUp(self):
    self.val = validate_config.CrosConfigValidator(False)

  def Run(self, dts_source):
    dts = tempfile.NamedTemporaryFile(suffix='.dts', delete=False)
    dts.write(dts_source)
    dts.close()
    errors = self.val.Start(dts.name)
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
         "which does not match pattern '/chromeos/family/firmware/MODEL'",
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
         "valid list is (shares, key-id)"], result)

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


if __name__ == '__main__':
  cros_test_lib.main(module=__name__)
