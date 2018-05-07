#!/usr/bin/env python2
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=module-missing-docstring,class-missing-docstring

from __future__ import print_function

import json
import jsonschema
import os
import unittest
import re
import tempfile

import cros_config_schema

BASIC_CONFIG = """
reef-9042-fw: &reef-9042-fw
  bcs-overlay: 'overlay-reef-private'
  ec-image: 'Reef_EC.9042.87.1.tbz2'
  main-image: 'Reef.9042.87.1.tbz2'
  main-rw-image: 'Reef.9042.110.0.tbz2'
  build-targets:
    coreboot: 'reef'

chromeos:
  devices:
    - $name: 'basking'
      products:
        - $key-id: 'OEM2'
          $brand-code: 'ASUN'
      skus:
        - $sku-id: 0
          config:
            audio:
              main:
                $card: 'bxtda7219max'
                cras-config-dir: '{{$name}}'
                ucm-suffix: '{{$name}}'
                $dsp-ini: "{{cras-config-dir}}/dsp.ini"
                files:
                  - source: "{{$dsp-ini}}"
                    destination: "/etc/cras/{{$dsp-ini}}"
            brand-code: '{{$brand-code}}'
            identity:
              platform-name: "Reef"
              smbios-name-match: "Reef"
              sku-id: "{{$sku-id}}"
            name: '{{$name}}'
            firmware: *reef-9042-fw
            firmware-signing:
              key-id: '{{$key-id}}'
              signature-id: '{{$name}}'
            powerd-prefs: 'reef'
            test-label: 'reef'
"""

this_dir = os.path.dirname(__file__)


class GetNamedTupleTests(unittest.TestCase):

  def testRecursiveDicts(self):
    val = {'a': {'b': 1, 'c': 2}}
    val_tuple = cros_config_schema.GetNamedTuple(val)
    self.assertEqual(val['a']['b'], val_tuple.a.b)
    self.assertEqual(val['a']['c'], val_tuple.a.c)

  def testListInRecursiveDicts(self):
    val = {'a': {'b': [{'c': 2}]}}
    val_tuple = cros_config_schema.GetNamedTuple(val)
    self.assertEqual(val['a']['b'][0]['c'], val_tuple.a.b[0].c)

  def testDashesReplacedWithUnderscores(self):
    val = {'a-b': 1}
    val_tuple = cros_config_schema.GetNamedTuple(val)
    self.assertEqual(val['a-b'], val_tuple.a_b)


class MergeDictionaries(unittest.TestCase):

  def testBaseKeyMerge(self):
    primary = {'a': {'b': 1, 'c': 2}}
    overlay = {'a': {'c': 3}, 'b': 4}
    cros_config_schema.MergeDictionaries(primary, overlay)
    self.assertEqual({'a': {'b': 1, 'c': 3}, 'b': 4}, primary)

  def testBaseListAppend(self):
    primary = {'a': {'b': 1, 'c': [1, 2]}}
    overlay = {'a': {'c': [3, 4]}}
    cros_config_schema.MergeDictionaries(primary, overlay)
    self.assertEqual({'a': {'b': 1, 'c': [1, 2, 3, 4]}}, primary)


class ParseArgsTests(unittest.TestCase):

  def testParseArgs(self):
    argv = ['-s', 'schema', '-c', 'config', '-o', 'output', '-f' 'True']
    args = cros_config_schema.ParseArgs(argv)
    self.assertEqual(args.schema, 'schema')
    self.assertEqual(args.config, 'config')
    self.assertEqual(args.output, 'output')
    self.assertTrue(args.filter)

  def testParseArgsForMerge(self):
    argv = ['-o', 'output', '-m' 'm1' 'm2' 'm3']
    args = cros_config_schema.ParseArgs(argv)
    self.assertEqual(args.output, 'output')
    self.assertEqual(args.merge, ['m1' 'm2' 'm3'])


class TransformConfigTests(unittest.TestCase):

  def testBasicTransform(self):
    result = cros_config_schema.TransformConfig(BASIC_CONFIG)
    json_dict = json.loads(result)
    self.assertEqual(len(json_dict), 1)
    json_obj = cros_config_schema.GetNamedTuple(json_dict)
    self.assertEqual(1, len(json_obj.chromeos.configs))
    model = json_obj.chromeos.configs[0]
    self.assertEqual('basking', model.name)
    self.assertEqual('basking', model.audio.main.cras_config_dir)
    # Check multi-level template variable evaluation
    self.assertEqual('/etc/cras/basking/dsp.ini',
                     model.audio.main.files[0].destination)


class ValidateConfigSchemaTests(unittest.TestCase):

  def setUp(self):
    with open(os.path.join(this_dir,
                           'cros_config_schema.yaml')) as schema_stream:
      self._schema = schema_stream.read()

  def testBasicSchemaValidation(self):
    cros_config_schema.ValidateConfigSchema(
        self._schema, cros_config_schema.TransformConfig(BASIC_CONFIG))

  def testMissingRequiredElement(self):
    config = re.sub(r' *cras-config-dir: .*', '', BASIC_CONFIG)
    config = re.sub(r' *volume: .*', '', BASIC_CONFIG)
    try:
      cros_config_schema.ValidateConfigSchema(
          self._schema, cros_config_schema.TransformConfig(config))
    except jsonschema.ValidationError as err:
      self.assertIn('required', err.__str__())
      self.assertIn('cras-config-dir', err.__str__())

  def testReferencedNonExistentTemplateVariable(self):
    config = re.sub(r' *$card: .*', '', BASIC_CONFIG)
    try:
      cros_config_schema.ValidateConfigSchema(
          self._schema, cros_config_schema.TransformConfig(config))
    except cros_config_schema.ValidationError as err:
      self.assertIn('Referenced template variable', err.__str__())
      self.assertIn('cras-config-dir', err.__str__())


class ValidateConfigTests(unittest.TestCase):

  def testBasicValidation(self):
    cros_config_schema.ValidateConfig(
        cros_config_schema.TransformConfig(BASIC_CONFIG))

  def testIdentitiesNotUnique(self):
    config = """
reef-9042-fw: &reef-9042-fw
  bcs-overlay: 'overlay-reef-private'
  ec-image: 'Reef_EC.9042.87.1.tbz2'
  main-image: 'Reef.9042.87.1.tbz2'
  main-rw-image: 'Reef.9042.110.0.tbz2'
  build-targets:
    coreboot: 'reef'
chromeos:
  devices:
    - $name: 'astronaut'
      products:
        - $key-id: 'OEM2'
      skus:
        - config:
            identity:
              sku-id: 0
            audio:
              main:
                cras-config-dir: '{{$name}}'
                ucm-suffix: '{{$name}}'
            name: '{{$name}}'
            firmware: *reef-9042-fw
            firmware-signing:
              key-id: '{{$key-id}}'
              signature-id: '{{$name}}'
    - $name: 'astronaut'
      products:
        - $key-id: 'OEM2'
      skus:
        - config:
            identity:
              sku-id: 0
            audio:
              main:
                cras-config-dir: '{{$name}}'
                ucm-suffix: '{{$name}}'
            name: '{{$name}}'
            firmware: *reef-9042-fw
            firmware-signing:
              key-id: '{{$key-id}}'
              signature-id: '{{$name}}'
"""
    try:
      cros_config_schema.ValidateConfig(
          cros_config_schema.TransformConfig(config))
    except cros_config_schema.ValidationError as err:
      self.assertIn('Identities are not unique', err.__str__())

  def testWhitelabelWithOtherThanBrandChanges(self):
    config = """
chromeos:
  devices:
    - $name: 'whitelabel'
      products:
        - $key-id: 'WL1'
          $wallpaper: 'WL1_WALLPAPER'
          $whitelabel-tag: 'WL1_TAG'
          $brand-code: 'WL1_BRAND_CODE'
          $powerd-prefs: 'WL1_POWERD_PREFS'
        - $key-id: 'WL2'
          $wallpaper: 'WL2_WALLPAPER'
          $whitelabel-tag: 'WL2_TAG'
          $brand-code: 'WL2_BRAND_CODE'
          $powerd-prefs: 'WL2_POWERD_PREFS'
      skus:
        - config:
            identity:
              sku-id: 0
              whitelabel-tag: '{{$whitelabel-tag}}'
            name: '{{$name}}'
            brand-code: '{{$brand-code}}'
            wallpaper: '{{$wallpaper}}'
            # THIS WILL CAUSE THE FAILURE
            powerd-prefs: '{{$powerd-prefs}}'
"""
    try:
      cros_config_schema.ValidateConfig(
          cros_config_schema.TransformConfig(config))
    except cros_config_schema.ValidationError as err:
      self.assertIn('Whitelabel configs can only', err.__str__())


class FilterBuildElements(unittest.TestCase):

  def testBasicFilterBuildElements(self):
    json_dict = json.loads(
        cros_config_schema.FilterBuildElements(
            cros_config_schema.TransformConfig(BASIC_CONFIG)))
    self.assertNotIn('firmware', json_dict['chromeos']['configs'][0])


class MainTests(unittest.TestCase):

  def testMainWithExampleWithBuildAndCBindings(self):
    output = tempfile.mktemp()
    c_output = tempfile.mktemp()
    cros_config_schema.Main(
        None,
        os.path.join(this_dir, '../libcros_config/test.yaml'),
        output,
        gen_c_bindings_output=c_output)
    regen_cmd = ('To regenerate the expected output, run:\n'
                 '\tpython -m cros_config_host.cros_config_schema '
                 '-c libcros_config/test.yaml '
                 '-o libcros_config/test_build.json '
                 '-g libcros_config/test.c')
    with open(output, 'r') as output_stream:
      with open(os.path.join(
          this_dir, '../libcros_config/test_build.json')) as expected_stream:
        self.assertEqual(expected_stream.read(), output_stream.read(),
                         regen_cmd)
    with open(c_output, 'r') as output_stream:
      with open(os.path.join(this_dir,
                             '../libcros_config/test.c')) as expected_stream:
        self.assertEqual(expected_stream.read(), output_stream.read(),
                         regen_cmd)

  def testMainWithExampleWithoutBuild(self):
    output = tempfile.mktemp()
    cros_config_schema.Main(
        None,
        os.path.join(this_dir, '../libcros_config/test.yaml'),
        output,
        filter_build_details=True)
    with open(output, 'r') as output_stream:
      with open(os.path.join(this_dir,
                             '../libcros_config/test.json')) as expected_stream:
        self.assertEqual(expected_stream.read(), output_stream.read(),
                         ('To regenerate the expected output, run:\n'
                          '\tpython -m cros_config_host.cros_config_schema '
                          '-f True '
                          '-c libcros_config/test.yaml '
                          '-o libcros_config/test.json'))

    os.remove(output)

  def testMainArmExample(self):
    output = tempfile.mktemp()
    c_output = tempfile.mktemp()
    cros_config_schema.Main(
        None,
        os.path.join(this_dir, '../libcros_config/test_arm.yaml'),
        output,
        filter_build_details=True,
        gen_c_bindings_output=c_output)
    regen_cmd = ('To regenerate the expected output, run:\n'
                 '\tpython -m cros_config_host.cros_config_schema '
                 '-f True '
                 '-c libcros_config/test_arm.yaml '
                 '-o libcros_config/test_arm.json '
                 '-g libcros_config/test_arm.c')
    with open(output, 'r') as output_stream:
      with open(os.path.join(
          this_dir,
          '../libcros_config/test_arm.json')) as expected_stream:
        self.assertEqual(
            expected_stream.read(), output_stream.read(), regen_cmd)
    with open(c_output, 'r') as output_stream:
      expected_file = os.path.join(this_dir, '../libcros_config/test_arm.c')
      with open(expected_file) as expected_stream:
        self.assertEqual(expected_stream.read(), output_stream.read(),
                         regen_cmd)

    os.remove(output)

  def testMainImportExample(self):
    output = tempfile.mktemp()
    cros_config_schema.Main(
        None,
        os.path.join(this_dir, '../libcros_config/test_import.yaml'),
        output)
    regen_cmd = ('To regenerate the expected output, run:\n'
                 '\tpython -m cros_config_host.cros_config_schema '
                 '-c libcros_config/test_import.yaml '
                 '-o libcros_config/test_import.json')
    with open(output, 'r') as output_stream:
      with open(os.path.join(
          this_dir,
          '../libcros_config/test_import.json')) as expected_stream:
        self.assertEqual(
            expected_stream.read(), output_stream.read(), regen_cmd)

    os.remove(output)


if __name__ == '__main__':
  unittest.main()
