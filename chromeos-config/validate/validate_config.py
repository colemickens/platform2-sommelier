#!/usr/bin/env python2
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Validates a given master configuration

This enforces various rules defined by the master configuration. Some of these
are fairly simple (the valid properties and subnodes for each node, the
allowable values for properties) and some are more complex (where phandles
are allowed to point).

The schema is defined by Python objects containing variable SchemaElement
subclasses. Each subclass defines how the device tree property is validated.
For strings this is via a regex. Phandles properties are validated by the
target they are expected to point to.

Schema elements can be optional or required. Optional elements will not cause
a failure if the node does not include them.

The presence or absense of a particular schema element can also be controlled
by a 'conditional_props' option. This lists elements that must (or must not)
be present in the node for this element to be present. This provides some
flexibility where the schema for a node has two options, for example, where
the presence of one element conflicts with the presence of others.

Usage:
  The validator can be run like this (set PYTHONPATH to your chromium dir):

  PYTHONPATH=~/cosarm ./validate/validate_config \
      ~/cosarm/chroot/build/coral/usr/share/chromeos-config/config.dtb \
      ~/cosarm/chroot/build/reef-uni/usr/share/chromeos-config/config.dtb \
      README.md

  The output format for each input file is the name of the file followed by a
  list of validation problems. If there are no problems, the filename is not
  shown.

  Unit tests can be run like this:

  PYTHONPATH=~/cosarm python validate/validate_config_unittest.py
"""

from __future__ import print_function

import copy
import itertools
import os
import re
import sys

our_path = os.path.dirname(os.path.realpath(__file__))
sys.path.append(os.path.join(our_path, '../cros_config_host_py'))

from chromite.lib import commandline
import fdt
import fdt_util
from validate_schema import NodeAny, NodeDesc, NodeModel, NodeSubmodel
from validate_schema import PropCustom, PropDesc, PropString, PropStringList
from validate_schema import PropPhandleTarget, PropPhandle, CheckPhandleTarget


def ParseArgv(argv):
  """Parse the available arguments.

  Invalid arguments or -h cause this function to print a message and exit.

  Args:
    argv: List of string arguments (excluding program name / argv[0])

  Returns:
    argparse.Namespace object containing the attributes.
  """
  parser = commandline.ArgumentParser(description=__doc__)
  parser.add_argument('-r', '--raise-on-error', action='store_true',
                      help='Causes the validator to raise on the first ' +
                      'error it finds. This is useful for debugging.')
  parser.add_argument('config', type=str, nargs='+',
                      help='Path to the config file (.dtb) to validated')
  return parser.parse_args(argv)


class CrosConfigValidator(object):
  """Validator for the master configuration"""
  def __init__(self, raise_on_error):
    """Master configuration validator.

    Properties:
      _errors: List of validation errors detected (each a string)
      _fdt: fdt.Fdt object containing device tree to validate
      _raise_on_error: True if the validator should raise on the first error
          (useful for debugging)
      model_list: List of model names found in the config
      submodel_list: Dict of submodel names found in the config:
          key: Model name
          value: List of submodel names
    """
    self._errors = []
    self._fdt = None
    self._raise_on_error = raise_on_error
    self.model_list = []
    self.submodel_list = {}

  def Fail(self, location, msg):
    """Record a validation failure

    Args:
      location: fdt.Node object where the error occurred
      msg: Message to record for this failure
    """
    self._errors.append('%s: %s' % (location, msg))
    if self._raise_on_error:
      raise ValueError(self._errors[-1])

  def _IsBuiltInProperty(self, node, prop_name):
    """Checks if a property is a built-in device-tree construct

    This checks for 'reg', '#address-cells' and '#size-cells' properties which
    are valid when correctly used in a device-tree context.

    Args:
      node: fdt.Node where the property appears
      prop_name: Name of the property

    Returns:
      True if this property is a built-in property and does not have to be
      covered by the schema
    """
    if prop_name == 'reg' and '@' in node.name:
      return True
    if prop_name in ['#address-cells', '#size-cells']:
      for subnode in node.subnodes:
        if '@' in subnode.name:
          return True
    return False

  def GetElement(self, schema, name, node):
    """Get an element from the schema by name

    Args:
      schema: Schema element to check
      name: Name of element to find (string)
      node: Node containing the property (or for nodes, the parent node
          containing the subnode) we are looking up

    Returns:
      Schema for the given element, or None if not found
    """
    prop_names = node.props.keys()
    for element in schema.elements:
      if not element.Present(prop_names):
        continue
      if element.name == name:
        return element
      elif (self.model_list and isinstance(element, NodeModel) and
            name in self.model_list):
        return element
      elif self.submodel_list and isinstance(element, NodeSubmodel):
        m = re.match('/chromeos/models/([a-z0-9]+)/submodels', node.path)
        if m and name in self.submodel_list[m.group(1)]:
          return element
      elif isinstance(element, NodeAny):
        return element
    return None

  def _ValidateSchema(self, node, schema):
    """Simple validation of properties.

    This only handles simple mistakes like getting the name wrong. It
    cannot handle relationships between different properties.

    Args:
      node: fdt.Node where the property appears
      schema: NodeDesc containing schema for this node
    """
    schema.Validate(self, node)
    prop_names = node.props.keys()
    schema_props = [e.name for e in schema.elements
                    if isinstance(e, PropDesc) and e.Present(prop_names)]

    # Validate each property and check that there are no extra properties not
    # mentioned in the schema.
    for prop_name in prop_names:
      if prop_name == 'linux,phandle':  # Ignore this (use 'phandle' instead)
        continue
      element = self.GetElement(schema, prop_name, node)
      if not element:
        if prop_name == 'phandle':
          self.Fail(node.path, 'phandle target not valid for this node')
        elif not self._IsBuiltInProperty(node, prop_name):
          self.Fail(node.path, "Unexpected property '%s', valid list is (%s)" %
                    (prop_name, ', '.join(schema_props)))
      if not isinstance(element, PropDesc):
        continue
      element.Validate(self, node.props[prop_name])

    # Check that there are no required properties which we don't have
    for element in schema.elements:
      if not isinstance(element, PropDesc) or not element.Present(prop_names):
        continue
      if element.required and element.name not in node.props.keys():
        self.Fail(node.path, "Required property '%s' missing" % element.name)

    # Check that any required subnodes are present
    subnode_names = [n.name for n in node.subnodes]
    for element in schema.elements:
      if (not isinstance(element, NodeDesc) or not element.required
          or not element.Present(prop_names)):
        continue
      if element.name not in subnode_names:
        msg = "Missing subnode '%s'" % element.name
        if subnode_names:
          msg += ' in %s' % ', '.join(subnode_names)
        self.Fail(node.path, msg)

  def GetSchema(self, node, parent_schema):
    """Obtain the schema for a subnode

    This finds the schema for a subnode, by scanning for a matching element.

    Args:
      node: fdt.Node whose schema we are searching for
      parent_schema: Schema for the parent node, which contains that schema
    """
    prop_names = node.parent.props.keys()
    schema = self.GetElement(parent_schema, node.name, node.parent)
    if schema is None:
      elements = [e.name for e in parent_schema.GetNodes()
                  if e.Present(prop_names)]
      self.Fail(os.path.dirname(node.path),
                "Unexpected subnode '%s', valid list is (%s)" %
                (node.name, ', '.join(elements)))
    return schema

  def _ValidateTree(self, node, parent_schema):
    """Validate a node and all its subnodes recursively

    Args:
      node: name of fdt.Node to search for
      parent_schema: Schema for the parent node
    """
    if node.name == '/':
      schema = parent_schema
    else:
      schema = self.GetSchema(node, parent_schema)
      if schema is None:
        return

    self._ValidateSchema(node, schema)
    for subnode in node.subnodes:
      self._ValidateTree(subnode, schema)

  @staticmethod
  def ValidateSkuMap(val, prop):
    it = iter(prop.value)
    sku_set = set()
    for sku, phandle in itertools.izip(it, it):
      sku_id = fdt_util.fdt32_to_cpu(sku)
      if sku_id > 255:
        val.Fail(prop.node.path, 'sku_id %d out of range' % sku_id)
      if sku_id in sku_set:
        val.Fail(prop.node.path, 'Duplicate sku_id %d' % sku_id)
      sku_set.add(sku_id)
      phandle_val = fdt_util.fdt32_to_cpu(phandle)
      target = prop.fdt.LookupPhandle(phandle_val)
      if (not CheckPhandleTarget(val, target, '/chromeos/models/MODEL') and
          not CheckPhandleTarget(val, target,
                                 '/chromeos/models/MODEL/submodels/SUBMODEL')):
        val.Fail(prop.node.path,
                 "Phandle '%s' sku-id %d must target a model or submodel'" %
                 (prop.name, sku_id))

  def Start(self, fname, schema):
    """Start validating a master configuration file

    Args:
      fname: Filename containing the configuration. Supports compiled .dtb
          files, source .dts files and README.md (which has configuration
          source between ``` markers).
      schema: Schema to use to validate the configuration (NodeDesc object)
    """
    tmpfile = None
    self.model_list = []
    self.submodel_list = {}
    self._errors = []
    try:
      dtb, tmpfile = fdt_util.EnsureCompiled(fname)
      self._fdt = fdt.FdtScan(dtb)

      # Locate all the models and submodels before we start
      models = self._fdt.GetNode('/chromeos/models')
      for model in models.subnodes:
        self.model_list.append(model.name)
        sub_models = model.FindNode('submodels')
        if sub_models:
          self.submodel_list[model.name] = (
              [sm.name for sm in sub_models.subnodes])
        else:
          self.submodel_list[model.name] = []

      # Validate the entire master configuration
      self._ValidateTree(self._fdt.GetRoot(), schema)
    finally:
      if tmpfile:
        os.unlink(tmpfile.name)
    return self._errors


# Basic firmware schema, which is augmented depending on the situation.
BASE_FIRMWARE_SCHEMA = [
    PropString('bcs-overlay', True, 'overlay-.*', {'shares': False}),
    PropString('ec-image', False, r'bcs://.*\.tbz2', {'shares': False}),
    PropString('main-image', False, r'bcs://.*\.tbz2', {'shares': False}),
    PropString('main-rw-image', False, r'bcs://.*\.tbz2', {'shares': False}),
    PropString('pd-image', False, r'bcs://.*\.tbz2', {'shares': False}),
    PropStringList('extra', False,
                   r'(\${(FILESDIR|SYSROOT)}/[a-z/]+)|' +
                   r'(bcs://[A-Za-z0-9\.]+\.tbz2)', {'shares': False}),
    ]

# Firmware build targets schema, defined here since it is used in a few places.
BUILD_TARGETS_SCHEMA = NodeDesc('build-targets', True, elements=[
    PropString('coreboot', True),
    PropString('ec', True),
    PropString('depthcharge', True),
    PropString('libpayload', True),
], conditional_props={'shares': False})

BASE_AUDIO_SCHEMA = [
    PropString('card', True, '', {'audio-type': False}),
    PropString('volume', True, '', {'audio-type': False}),
    PropString('dsp-ini', True, '', {'audio-type': False}),
    PropString('hifi-conf', True, '', {'audio-type': False}),
    PropString('alsa-conf', True, '', {'audio-type': False}),
    PropString('topology-xml', False, '', {'audio-type': False}),
    PropString('topology-bin', False, '', {'audio-type': False}),
]

"""This is the schema. It is a hierarchical set of nodes and properties, just
like the device tree. If an object subclasses NodeDesc then it is a node,
possibly with properties and subnodes.

In this way it is possible to describe the schema in a fairly natural,
hierarchical way.
"""
SCHEMA = NodeDesc('/', True, [
    NodeDesc('chromeos', True, [
        NodeDesc('family', True, [
            NodeDesc('audio', elements=[
                NodeAny('', [PropPhandleTarget()] +
                    copy.deepcopy(BASE_AUDIO_SCHEMA)),
            ]),
            NodeDesc('firmware', elements=[
                PropString('script', True, r'updater4\.sh'),
                NodeModel([
                    PropPhandleTarget(),
                    copy.deepcopy(BUILD_TARGETS_SCHEMA),
                    ] + copy.deepcopy(BASE_FIRMWARE_SCHEMA))
            ]),
            NodeDesc('touch', False, [
                NodeAny('', [
                    PropPhandleTarget(),
                    PropString('firmware-bin', True, ''),
                    PropString('firmware-symlink', True, ''),
                    PropString('vendor', True, ''),
                ]),
            ]),
            NodeDesc('mapping', False, [
                NodeAny(r'sku-map(@[0-9])?', [
                    PropString('platform-name', False, ''),
                    PropString('smbios-name-match', False, ''),
                    PropPhandle('single-sku', '/chromeos/models/MODEL', False),
                    PropCustom('simple-sku-map',
                               CrosConfigValidator.ValidateSkuMap, False),
                ]),
            ]),
        ]),
        NodeDesc('models', True, [
            NodeModel([
                PropPhandleTarget(),
                NodeDesc('firmware', False, [
                    PropPhandle('shares', '/chromeos/family/firmware/MODEL',
                                False),
                    PropString('key-id', False, '[A-Z][A-Z0-9]+'),
                    copy.deepcopy(BUILD_TARGETS_SCHEMA)
                    ] + copy.deepcopy(BASE_FIRMWARE_SCHEMA)),
                PropString('brand-code', False, '[A-Z]{4}'),
                PropString('powerd-prefs'),
                PropString('wallpaper', False, '[a-z_]+'),
                NodeDesc('audio', False, [
                  NodeAny(r'main', [
                      PropPhandle('audio-type', '/chromeos/family/audio/ANY',
                                  False),
                      PropString('cras-config-dir', True, r'\w+'),
                      PropString('ucm-suffix', True, r'\w+'),
                      PropString('topology-name', False, r'\w+'),
                  ] + copy.deepcopy(BASE_AUDIO_SCHEMA)),
                ]),
                NodeDesc('submodels', False, [
                    NodeSubmodel([
                        PropPhandleTarget()
                    ])
                ]),
                NodeDesc('thermal', False, [
                    PropString('dptf-dv', False, r'\w+/dptf.dv'),
                ]),
                NodeDesc('touch', False, [
                    PropString('present', False, r'yes|no|probe'),
                    # We want to validate that probe-regex is only present when
                    # 'present' = 'probe', but have no way of doing this
                    # currently.
                    PropString('probe-regex', False, ''),
                    NodeAny(r'(stylus|touchpad|touchscreen)(@[0-9])?', [
                        PropString('pid', False),
                        PropString('version', True),
                        PropPhandle('touch-type', '/chromeos/family/touch/ANY',
                                    False),
                        PropString('firmware-bin', True, '',
                                   {'touch-type': False}),
                        PropString('firmware-symlink', True, '',
                                   {'touch-type': False}),
                        PropString('date-code', False),
                    ]),
                ]),
            ])
        ])
    ])
])


def Main(argv):
  """Main program for validator

  This validates each of the provided files and prints the errors for each, if
  any.

  Args:
    argv: Arguments to the problem (excluding argv[0])
  """
  args = ParseArgv(argv)
  validator = CrosConfigValidator(args.raise_on_error)
  found_errors = False
  for fname in args.config:
    errors = validator.Start(fname, SCHEMA)
    if errors:
      found_errors = True
      print('%s:' % fname)
      for error in errors:
        print(error)
      print()
  if found_errors:
    sys.exit(2)


if __name__ == "__main__":
  Main(sys.argv[1:])
