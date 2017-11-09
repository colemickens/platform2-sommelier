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
import tempfile

from . import cros_config_schema

BASIC_CONFIG = """
name: 'coral'
componentConfig:
    bluetoothConfigPath: '/etc/bluetooth/coral.conf'
models:
  - name: 'astronaut'
    componentConfig:
      bluetoothConfigPath: '/etc/bluetooth/astronaut.conf'
  - name: 'lava'
    componentConfig:
      bluetoothConfigPath: '/etc/bluetooth/lava.conf'
"""

INHERITED_CONFIG = """
name: 'coral'
componentConfig:
    bluetoothConfigPath: '/etc/bluetooth/coral.conf'
models:
  - name: 'astronaut'
    componentConfig:
  - name: 'lava'
    componentConfig:
      bluetoothConfigPath: '/etc/bluetooth/lava.conf'
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


class ParseArgsTests(unittest.TestCase):
  def testParseArgs(self):
    argv = ['-s', 'schema', '-c', 'config', '-o', 'output']
    args = cros_config_schema.ParseArgs(argv)
    self.assertEqual(args.schema, 'schema')
    self.assertEqual(args.config, 'config')
    self.assertEqual(args.output, 'output')


class TransformConfigTests(unittest.TestCase):
  def testBasicTransform(self):
    result = cros_config_schema.TransformConfig(BASIC_CONFIG)
    json_obj = cros_config_schema.GetNamedTuple(json.loads(result))
    self.assertEqual(2, len(json_obj.models))
    self.assertEqual(
        'astronaut',
        json_obj.models[0].name)
    self.assertEqual(
        '/etc/bluetooth/astronaut.conf',
        json_obj.models[0].componentConfig.bluetoothConfigPath)
    self.assertEqual(
        'lava',
        json_obj.models[1].name)

  def testFamilyConfigInheritanceNoValue(self):
    result = cros_config_schema.TransformConfig(INHERITED_CONFIG)
    json_obj = cros_config_schema.GetNamedTuple(json.loads(result))
    self.assertEqual(
        '/etc/bluetooth/coral.conf',
        json_obj.models[0].componentConfig.bluetoothConfigPath)

  def testFamilyConfigInheritanceEmptyValue(self):
    config = """
name: 'coral'
componentConfig:
    bluetoothConfigPath: '/etc/bluetooth/coral.conf'
models:
  - name: 'astronaut'
    componentConfig:
      bluetoothConfigPath: ''
"""
    result = cros_config_schema.TransformConfig(config)
    json_obj = cros_config_schema.GetNamedTuple(json.loads(result))
    self.assertEqual(
        '/etc/bluetooth/coral.conf',
        json_obj.models[0].componentConfig.bluetoothConfigPath)

  def testFullFamilyConfigInheritance(self):
    config = """
name: 'coral'
componentConfig:
    bluetoothConfigPath: '/etc/bluetooth/coral.conf'
models:
  - name: 'astronaut'
"""
    result = cros_config_schema.TransformConfig(config)
    json_obj = cros_config_schema.GetNamedTuple(json.loads(result))
    self.assertEqual(
        '/etc/bluetooth/coral.conf',
        json_obj.models[0].componentConfig.bluetoothConfigPath)

class ValidateConfigSchemaTests(unittest.TestCase):
  def setUp(self):
    with open(
        os.path.join(this_dir, 'cros_config_schema.json')) as schema_stream:
      self._schema = schema_stream.read()

  def testBasicSchemaValidation(self):
    cros_config_schema.ValidateConfigSchema(
        self._schema, cros_config_schema.TransformConfig(BASIC_CONFIG))

  def testFamilyConfigInheritance(self):
    cros_config_schema.ValidateConfigSchema(
        self._schema, cros_config_schema.TransformConfig(INHERITED_CONFIG))

  def testMissingBluetoothPath(self):
    config = """
name: 'coral'
models:
  - name: 'astronaut'
    componentConfig:
      bluetoothConfigPath: ''
"""
    try:
      cros_config_schema.ValidateConfigSchema(
          self._schema, cros_config_schema.TransformConfig(config))
    except jsonschema.ValidationError as err:
      self.assertIn('does not match', err.__str__())
      self.assertIn('bluetooth', err.__str__())

  def testInvalidBluetoothPath(self):
    config = """
name: 'coral'
models:
  - name: 'astronaut'
    componentConfig:
      bluetoothConfigPath: 'not a valid path'
"""
    try:
      cros_config_schema.ValidateConfigSchema(
          self._schema, cros_config_schema.TransformConfig(config))
    except jsonschema.ValidationError as err:
      self.assertIn('does not match', err.__str__())
      self.assertIn('bluetooth', err.__str__())

  def testInvalidName(self):
    config = """
name: 'ab'
models:
  - name: 'astronaut'
    componentConfig:
      bluetoothConfigPath: '/etc/bluetooth/valid.conf'
"""
    try:
      cros_config_schema.ValidateConfigSchema(
          self._schema, cros_config_schema.TransformConfig(config))
    except jsonschema.ValidationError as err:
      self.assertIn('does not match', err.__str__())
      self.assertIn('name', err.__str__())


class ValidateConfigTests(unittest.TestCase):
  def testBasicValidation(self):
    cros_config_schema.ValidateConfig(
        cros_config_schema.TransformConfig(BASIC_CONFIG))

  def testModelNamesNotUnique(self):
    config = """
name: 'coral'
models:
  - name: 'astronaut'
    componentConfig:
      bluetoothConfigPath: '/etc/bluetooth/valid.conf'
  - name: 'astronaut'
    componentConfig:
      bluetoothConfigPath: '/etc/bluetooth/valid.conf'
"""
    try:
      cros_config_schema.ValidateConfig(
          cros_config_schema.TransformConfig(config))
    except cros_config_schema.ValidationError as err:
      self.assertIn('Model names are not unique', err.__str__())

class MainTests(unittest.TestCase):
  def testMainWithExample(self):
    output = tempfile.mktemp()
    cros_config_schema.Main(
        os.path.join(this_dir, 'cros_config_schema.json'),
        os.path.join(this_dir, 'cros_config_schema_example.yaml'),
        output)
    with open(output, 'r') as output_stream:
      with open(
          os.path.join(this_dir, 'cros_config_schema_example.json')
      ) as expected_stream:
        self.assertEqual(expected_stream.read(), output_stream.read())

    os.remove(output)


if __name__ == '__main__':
  unittest.main()
