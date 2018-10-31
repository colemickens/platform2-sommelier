# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Chrome OS Configuration Schema library.

Provides common cros_config and cros_config_test schema functions.
"""

from __future__ import print_function

import collections
import json
import os
import re
import yaml

from jsonschema import validate

def GetNamedTuple(mapping):
  """Converts a mapping into Named Tuple recursively.

  Args:
    mapping: A mapping object to be converted.

  Returns:
    A named tuple generated from mapping
  """
  if not isinstance(mapping, collections.Mapping):
    return mapping
  new_mapping = {}
  for k, v in mapping.iteritems():
    if isinstance(v, list):
      new_list = []
      for val in v:
        new_list.append(GetNamedTuple(val))
      new_mapping[k.replace('-', '_').replace('@', '_')] = new_list
    else:
      new_mapping[k.replace('-', '_').replace('@', '_')] = GetNamedTuple(v)
  return collections.namedtuple('Config', new_mapping.iterkeys())(**new_mapping)


def FormatJson(config):
  """Formats JSON for output or printing.

  Args:
    config: Dictionary to be output
  """
  return json.dumps(config, sort_keys=True, indent=2, separators=(',', ': '))


def GetValidSchemaProperties(schema_node, path, result):
  """Recursively finds the valid properties for a given node

  Args:
    schema_node: Single node from the schema
    path: Running path that a given node maps to
    result: Running collection of results
  """
  full_path = '/%s' % '/'.join(path)
  for key in schema_node:
    new_path = path + [key]
    schema_type = schema_node[key]['type']

    if schema_type == 'object':
      if 'properties' in schema_node[key]:
        GetValidSchemaProperties(
            schema_node[key]['properties'], new_path, result)
    elif schema_type == 'string':
      all_props = result.get(full_path, [])
      all_props.append(key)
      result[full_path] = all_props


def ValidateConfigSchema(schema, config):
  """Validates a transformed config against the schema specified.

  Verifies that the config complies with the schema supplied.

  Args:
    schema: Source schema used to verify the config.
    config: Config (transformed) that will be verified.
  """
  json_config = json.loads(config)
  schema_yaml = yaml.load(schema)
  schema_json_from_yaml = json.dumps(schema_yaml, sort_keys=True, indent=2)
  schema_json = json.loads(schema_json_from_yaml)
  validate(json_config, schema_json)


def FindImports(config_file, includes):
  """Recursively looks up and finds files to include for yaml.

  Args:
    config_file: Path to the config file for which to apply imports.
    includes: List that is built up through processing the files.
  """
  working_dir = os.path.dirname(config_file)
  with open(config_file, 'r') as config_stream:
    config_lines = config_stream.readlines()
    yaml_import_lines = []
    found_imports = False
    # Parsing out just the imports snippet is required because the YAML
    # isn't valid until the imports are eval'd.
    for line in config_lines:
      if re.match(r'^imports', line):
        found_imports = True
        yaml_import_lines.append(line)
      elif found_imports:
        match = re.match(r' *- (.*)', line)
        if match:
          yaml_import_lines.append(line)
        else:
          break

    if yaml_import_lines:
      yaml_import = yaml.load('\n'.join(yaml_import_lines))

      for import_file in yaml_import.get('imports', []):
        full_path = os.path.join(working_dir, import_file)
        FindImports(full_path, includes)
    includes.append(config_file)


def ApplyImports(config_file):
  """Parses the imports statements and applies them to a result config.

  Args:
    config_file: Path to the config file for which to apply imports.

  Returns:
    Raw config with the imports applied.
  """
  import_files = []
  FindImports(config_file, import_files)

  all_yaml_files = []
  for import_file in import_files:
    with open(import_file, 'r') as yaml_stream:
      all_yaml_files.append(yaml_stream.read())

  return '\n'.join(all_yaml_files)
