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

  PYTHONPATH=~/cosarm ./validate_config \
      ~/cosarm/chroot/build/coral/usr/share/chromeos-config/config.dtb \
      ~/cosarm/chroot/build/reef-uni/usr/share/chromeos-config/config.dtb \
      README.md

  The output format for each input file is the name of the file followed by a
  list of validation problems. If there are no problems, the filename is not
  shown.

  Unit tests can be run like this:

  PYTHONPATH=~/cosarm python validate_config_unittest.py
"""

from __future__ import print_function

import argparse
import copy
import itertools
import os
import re
import sys

from chromite.lib import cros_build_lib

import fdt, fdt_util
from validate_schema import NodeAny, NodeDesc, NodeModel, NodeSubmodel
from validate_schema import PropCustom, PropDesc, PropString, PropStringList
from validate_schema import PropPhandleTarget, PropPhandle, CheckPhandleTarget
from validate_schema import PropAny, PropBool, PropFile, PropFloat


def ParseArgv(argv):
  """Parse the available arguments.

  Invalid arguments or -h cause this function to print a message and exit.

  Args:
    argv: List of string arguments (excluding program name / argv[0])

  Returns:
    argparse.Namespace object containing the attributes.
  """
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('-d', '--debug', action='store_true',
                      help='Run in debug mode (full exception traceback)')
  parser.add_argument('-p', '--partial', action='store_true',
                      help='Validate a list partial files (.dtsi) individually')
  parser.add_argument('-r', '--raise-on-error', action='store_true',
                      help='Causes the validator to raise on the first ' +
                      'error it finds. This is useful for debugging.')
  parser.add_argument('config', type=str, nargs='+',
                      help='Paths to the config files (.dtb) to validated')
  return parser.parse_args(argv)


class CrosConfigValidator(object):
  """Validator for the master configuration"""
  def __init__(self, schema, raise_on_error):
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
    self._schema = schema

    # This iniital value matches the standard schema object. This is
    # overwritten by the real model list by Start().
    self.model_list = ['MODEL']
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

  def ElementPresent(self, schema, parent_node):
    """Check whether a schema element should be present

    This handles the conditional_props feature. The list of names of sibling
    nodes/properties that are actually present is checked to see if any of them
    conflict with the conditional properties for this node. If there is a
    conflict, then this element is considered to be absent.

    Args:
      schema: Schema element to check
      parent_node: Parent fdt.Node containing this schema element (or None if
          this is not known)

    Returns:
      True if this element is present, False if absent
    """
    if schema.conditional_props and parent_node:
      for rel_name, value in schema.conditional_props.iteritems():
        name = rel_name
        schema_target = schema.parent
        node_target = parent_node
        while name.startswith('../'):
          schema_target = schema_target.parent
          node_target = node_target.parent
          name = name[3:]
        parent_props = [e.name for e in schema_target.elements]
        sibling_names = node_target.props.keys()
        sibling_names += [n.name for n in node_target.subnodes.values()]
        if name in parent_props and value != (name in sibling_names):
          return False
    return True

  def GetElement(self, schema, name, node, expected=None):
    """Get an element from the schema by name

    Args:
      schema: Schema element to check
      name: Name of element to find (string)
      node: Node containing the property (or for nodes, the parent node
          containing the subnode) we are looking up. None if none available
      expected: The SchemaElement object that is expected. This can be NodeDesc
          if a node is expected, PropDesc if a property is expected, or None
          if either is fine.

    Returns:
      Tuple:
        Schema for the node, or None if none found
        True if the node should have schema, False if it can be ignored
            (because it is internal to the device-tree format)
    """
    for element in schema.elements:
      if not self.ElementPresent(element, node):
        continue
      if element.name == name:
        return element, True
      elif (self.model_list and isinstance(element, NodeModel) and
            name in self.model_list):
        return element, True
      elif self.submodel_list and isinstance(element, NodeSubmodel) and node:
        m = re.match('/chromeos/models/([a-z0-9]+)/submodels', node.path)
        if m and name in self.submodel_list[m.group(1)]:
          return element, True
      elif ((expected is None or expected == NodeDesc) and
            isinstance(element, NodeAny)):
        return element, True
      elif ((expected is None or expected == PropDesc) and
            isinstance(element, PropAny)):
        return element, True
    if expected == PropDesc:
      if name == 'linux,phandle':
        return None, False
    return None, True

  def GetElementByPath(self, path):
    """Find a schema element given its full path

    Args:
      path: Full path to look up (e.g. '/chromeos/models/MODEL/thermal/dptf-dv')

    Returns:
      SchemaElement object for that path

    Raises:
      AttributeError if not found
    """
    parts = path.split('/')[1:]
    schema = self._schema
    for part in parts:
      element, _ = self.GetElement(schema, part, None)
      schema = element
    return schema

  def _ValidateSchema(self, node, schema):
    """Simple validation of properties.

    This only handles simple mistakes like getting the name wrong. It
    cannot handle relationships between different properties.

    Args:
      node: fdt.Node where the property appears
      schema: NodeDesc containing schema for this node
    """
    schema.Validate(self, node)
    schema_props = [e.name for e in schema.elements
                    if isinstance(e, PropDesc) and
                    self.ElementPresent(e, node)]

    # Validate each property and check that there are no extra properties not
    # mentioned in the schema.
    for prop_name in node.props.keys():
      if prop_name == 'linux,phandle':  # Ignore this (use 'phandle' instead)
        continue
      element, _ = self.GetElement(schema, prop_name, node, PropDesc)
      if not element or not isinstance(element, PropDesc):
        if prop_name == 'phandle':
          self.Fail(node.path, 'phandle target not valid for this node')
        else:
          self.Fail(node.path, "Unexpected property '%s', valid list is (%s)" %
                    (prop_name, ', '.join(schema_props)))
        continue
      element.Validate(self, node.props[prop_name])

    # Check that there are no required properties which we don't have
    for element in schema.elements:
      if (not isinstance(element, PropDesc) or
          not self.ElementPresent(element, node)):
        continue
      if element.required and element.name not in node.props.keys():
        self.Fail(node.path, "Required property '%s' missing" % element.name)

    # Check that any required subnodes are present
    subnode_names = [n.name for n in node.subnodes.values()]
    for element in schema.elements:
      if (not isinstance(element, NodeDesc) or not element.required
          or not self.ElementPresent(element, node)):
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

    Returns:
      Schema for the node, or None if none found
    """
    schema, needed = self.GetElement(parent_schema, node.name, node.parent,
                                     NodeDesc)
    if not schema and needed:
      elements = [e.name for e in parent_schema.GetNodes()
                  if self.ElementPresent(e, node.parent)]
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
    for subnode in node.subnodes.values():
      self._ValidateTree(subnode, schema)

  @staticmethod
  def ValidateSkuMap(val, prop):
    it = iter(prop.value)
    sku_set = set()
    for sku, phandle in itertools.izip(it, it):
      sku_id = fdt_util.fdt32_to_cpu(sku)
      # Allow a SKU ID of -1 as a valid match.
      if sku_id > 0xffff and sku_id != 0xffffffff:
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

  def GetModelTargetDir(self, path, prop_name):
    """Get the target directory for a given path and property

    This looks up the model schema for a given path and property, and locates
    the target directory for that property.

    Args:
      path: Path within model schema to examine (e.g. /thermal)
      prop_name: Property name to examine (e.g. 'dptf-dv')

    Returns:
      target directory for that property (e.g. '/etc/dptf')
    """
    element = self.GetElementByPath(
        '/chromeos/models/MODEL%s/%s' % (path, prop_name))
    return element.target_dir

  def Prepare(self, _fdt):
    """Locate all the models and submodels before we start"""
    self._fdt = _fdt
    models = self._fdt.GetNode('/chromeos/models')
    for model in models.subnodes.values():
      self.model_list.append(model.name)
      sub_models = model.FindNode('submodels')
      if sub_models:
        self.submodel_list[model.name] = (
            [sm.name for sm in sub_models.subnodes.values()])
      else:
        self.submodel_list[model.name] = []


  def Start(self, fnames, partial=False):
    """Start validating a master configuration file

    Args:
      fnames: List of filenames containing the configuration to validate.
          Supports compiled .dtb files, source .dts files and README.md (which
          has configuration source between ``` markers). If partial is False
          then there can be only one filename in the list.
      partial: True to process a list of partial config files (.dtsi)
    """
    tmpfile = None
    self.model_list = []
    self.submodel_list = {}
    self._errors = []
    try:
      if partial:
        dtb, tmpfile = fdt_util.CompileAll(fnames)
      else:
        dtb, tmpfile = fdt_util.EnsureCompiled(fnames[0])
      self.Prepare(fdt.FdtScan(dtb))

      # Validate the entire master configuration
      self._ValidateTree(self._fdt.GetRoot(), self._schema)
    finally:
      if tmpfile:
        os.unlink(tmpfile.name)
    return self._errors

  @classmethod
  def AddElementTargetDirectories(cls, target_dirs, parent):
    if isinstance(parent, PropFile):
      if parent.name in target_dirs:
        if target_dirs[parent.name] != parent.target_dir:
          raise ValueError(
              "Path for element '%s' is inconsistent with previous path '%s'" %
              (parent.target_dir, target_dirs[parent.name]))
      else:
        target_dirs[parent.name] = parent.target_dir
    if isinstance(parent, NodeDesc):
      for element in parent.elements:
        cls.AddElementTargetDirectories(target_dirs, element)

  def GetTargetDirectories(self):
    """Gets a dict of directory targets for each PropFile property

    Returns:
      Dict:
        key: Property name
        value: Ansolute path for this property
    """
    target_dirs = {}
    self.AddElementTargetDirectories(target_dirs, self._schema)
    return target_dirs

  @classmethod
  def AddElementPhandleProps(cls, phandle_props, parent):
    if isinstance(parent, PropPhandle):
      phandle_props.add(parent.name)
    elif isinstance(parent, NodeDesc):
      for element in parent.elements:
        cls.AddElementPhandleProps(phandle_props, element)

  def GetPhandleProps(self):
    """Gets a set of properties which are used as phandles

    Some properties are used as phandles to link to shared config. This returns
    a set of such properties. Note that 'default' is a special case here
    because it is not a simple phandle link. It locates notes and properties
    anywhere in the linked model. So we need to exclude it from this list so
    that the 'default' handling works correctly.

    Returns:
      set of property names, each a string
    """
    phandle_props = set()
    self.AddElementPhandleProps(phandle_props, self._schema)
    phandle_props.discard('default')
    return phandle_props


# Known directories for installation
CRAS_CONFIG_DIR = '/etc/cras'
UCM_CONFIG_DIR = '/usr/share/alsa/ucm'
LIB_FIRMWARE = '/lib/firmware'
TOUCH_FIRMWARE = '/opt/google/touch/firmware'
# In order not to pollute the regular /usr/sbin with model specific files
# putting the files underneath chromeos-config
ARC_SBIN_DIR = '/usr/share/chromeos-config/sbin'

# Basic firmware schema, which is augmented depending on the situation.
FW_COND = {'shares': False, '../whitelabel': False}

BASE_FIRMWARE_SCHEMA = [
    PropString('bcs-overlay', True, 'overlay-.*', FW_COND),
    PropString('ec-image', False, r'bcs://.*\.tbz2', FW_COND),
    PropString('main-image', False, r'bcs://.*\.tbz2', FW_COND),
    PropString('main-rw-image', False, r'bcs://.*\.tbz2', FW_COND),
    PropString('pd-image', False, r'bcs://.*\.tbz2', FW_COND),
    PropStringList('extra', False,
                   r'(\${(FILESDIR|SYSROOT)}/[a-z/]+)|' +
                   r'(bcs://[A-Za-z0-9\.]+\.tbz2)', FW_COND),
    ]

# Firmware build targets schema, defined here since it is used in a few places.
BUILD_TARGETS_SCHEMA = NodeDesc('build-targets', True, elements=[
    PropString('coreboot', True),
    PropString('ec', True),
    PropString('depthcharge', True),
    PropString('libpayload', True),
    PropString('u-boot'),
    PropString('cr50'),
], conditional_props={'shares': False, '../whitelabel': False})

BASE_AUDIO_SCHEMA = [
    PropString('card', True, '', {'audio-type': False}),
    PropFile('volume', True, '', {'audio-type': False}, CRAS_CONFIG_DIR),
    PropFile('dsp-ini', True, '', {'audio-type': False}, CRAS_CONFIG_DIR),
    PropFile('hifi-conf', True, '', {'audio-type': False}, UCM_CONFIG_DIR),
    PropFile('alsa-conf', True, '', {'audio-type': False}, UCM_CONFIG_DIR),
    PropString('topology-name', False, r'\w+'),
    PropFile('topology-bin', False, '', {'audio-type': False}, LIB_FIRMWARE),

    # TODO(sjg@chromium.org): There is no validation that we have these two.
    # They must both exist either in the model's audio node or here.
    PropFile('cras-config-dir', False, r'[\w${}]+', target_dir=CRAS_CONFIG_DIR),
    PropString('ucm-suffix', False, r'[\w${}]+'),
]

ARC_PROPERTIES_SCHEMA = [
    PropString('product', False, '[a-z0-9]+', {'../whitelabel': False}),
    PropString('device', False, '[{}a-z0-9_]+', {'../whitelabel': False}),
    PropString('oem', False),
    PropString('marketing-name', False),
    PropString('metrics-tag', False),
    PropString('first-api-level', False, '[0-9]+'),
]

BASE_AUDIO_NODE = [
    NodeAny(r'main', [
        PropPhandle('audio-type', '/chromeos/family/audio/ANY',
                    False),
    ] + BASE_AUDIO_SCHEMA)
]

BASE_POWER_SCHEMA = [
    PropString('charging-ports'),
    PropFloat('keyboard-backlight-no-als-brightness', False, (0, 100)),
    PropFloat('low-battery-shutdown-percent', False, (0, 100)),
    PropFloat('power-supply-full-factor', False, (0.001, 1.0)),
    PropString('set-wifi-transmit-power-for-tablet-mode', False, '0|1'),
    PropString('suspend-to-idle', False, '0|1'),
    PropString('touchpad-wakeup', False, '0|1'),
]

NOT_WL = {'whitelabel': False}

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
            NodeDesc('bcs', False, [
                NodeAny('', [
                    PropPhandleTarget(),
                    PropString('overlay', False, 'overlay-.*'),
                    PropString('package'),
                    PropString('tarball'),
                    PropString('ebuild-version', False, '[-.0-9r]+'),
                ]),
            ]),
            NodeDesc('power', elements=[
                NodeAny('', [PropPhandleTarget()] +
                        copy.deepcopy(BASE_POWER_SCHEMA)),
            ]),
            NodeDesc('arc', elements=[
                NodeDesc('build-properties', elements=[
                    NodeAny('', [PropPhandleTarget()] +
                            copy.deepcopy(ARC_PROPERTIES_SCHEMA)),
                ]),
            ]),
            NodeDesc('firmware', elements=[
                PropString('script', True, r'updater4\.sh'),
                NodeAny('', [
                    PropPhandleTarget(),
                    copy.deepcopy(BUILD_TARGETS_SCHEMA),
                    ] + copy.deepcopy(BASE_FIRMWARE_SCHEMA))
            ]),
            NodeDesc('touch', False, [
                NodeAny('', [
                    PropPhandleTarget(),
                    PropPhandle('bcs-type', '/chromeos/family/bcs/ANY'),
                    PropFile('firmware-bin', True, target_dir=TOUCH_FIRMWARE),
                    PropFile('firmware-symlink', True, target_dir=LIB_FIRMWARE),
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
                PropPhandle('default', '/chromeos/models/MODEL', False),
                PropPhandle('whitelabel', '/chromeos/models/MODEL', False),
                NodeDesc('firmware', False, [
                    PropPhandle('shares', '/chromeos/family/firmware/ANY',
                                False, {'../whitelabel': False}),
                    PropString(('sig-id-in-customization-id'),
                               conditional_props={'../whitelabel': False}),
                    PropString('key-id', False, '[A-Z][A-Z0-9]+'),
                    PropBool('no-firmware'),
                    copy.deepcopy(BUILD_TARGETS_SCHEMA)
                    ] + copy.deepcopy(BASE_FIRMWARE_SCHEMA)),
                PropString('brand-code', False, '[A-Z]{4}'),
                PropString('powerd-prefs', conditional_props=NOT_WL),
                PropString('test-label', False, '[a-z0-9_]+'),
                PropString('wallpaper', False, '[a-z_]+'),
                PropString('oem-id', False, '[0-9]+'),
                NodeDesc('audio', False, copy.deepcopy(BASE_AUDIO_NODE),
                         conditional_props=NOT_WL),
                NodeDesc('arc', False, [
                    PropFile('hw-features', False, '',
                             target_dir=ARC_SBIN_DIR),
                    NodeDesc('build-properties', False, [
                        PropPhandle('arc-properties-type',
                                    '/chromeos/family/arc/build-properties/ANY',
                                    False, {'../whitelabel': False})
                    ] + copy.deepcopy(ARC_PROPERTIES_SCHEMA)),
                ], conditional_props=NOT_WL),
                NodeDesc('power', False, [
                    PropPhandle('power-type', '/chromeos/family/power/ANY',
                                False),
                ] + copy.deepcopy(BASE_POWER_SCHEMA), conditional_props=NOT_WL),
                NodeDesc('submodels', False, [
                    NodeSubmodel([
                        PropPhandleTarget(),
                        NodeDesc('audio', False, copy.deepcopy(BASE_AUDIO_NODE),
                                 conditional_props={'../../audio': False}),
                        NodeDesc('touch', False, [
                            PropString('present', False, r'yes|no|probe'),
                            PropString('probe-regex', False, ''),
                        ]),
                    ])
                ], conditional_props=NOT_WL),
                NodeDesc('thermal', False, [
                    PropFile('dptf-dv', False, r'\w+/dptf.dv',
                             target_dir='/etc/dptf'),
                ], conditional_props=NOT_WL),
                NodeDesc('touch', False, [
                    PropString('present', False, r'yes|no|probe'),
                    # We want to validate that probe-regex is only present when
                    # 'present' = 'probe', but have no way of doing this
                    # currently.
                    PropString('probe-regex', False, ''),
                    NodeAny(r'(stylus|touchpad|touchscreen)(@[0-9])?', [
                        PropPhandle('bcs-type', '/chromeos/family/bcs/ANY'),
                        PropString('pid', False),
                        PropString('version', True),
                        PropPhandle('touch-type', '/chromeos/family/touch/ANY',
                                    False),
                        PropFile('firmware-bin', True, '',
                                 {'touch-type': False},
                                 target_dir=TOUCH_FIRMWARE),
                        PropFile('firmware-symlink', True, '',
                                 {'touch-type': False},
                                 target_dir=LIB_FIRMWARE),
                        PropString('date-code', False),
                    ]),
                ], conditional_props=NOT_WL),
                NodeDesc('whitelabels', False, [
                    NodeAny('', [
                        PropString('brand-code', False, '[A-Z]{4}'),
                        PropString('wallpaper', False, '[a-z_]+'),
                        PropString('key-id', False, '[A-Z][A-Z0-9]+'),
                    ]),
                ], conditional_props=NOT_WL),
                NodeDesc('ui', False, [
                    NodeDesc('power-button', False, [
                        PropString('edge', True, 'left|right|top|bottom'),
                        PropFloat('position', True, (0, 1)),
                    ]),
                ], conditional_props=NOT_WL),
            ])
        ]),
        NodeDesc('schema', False, [
            NodeDesc('target-dirs', False, [
                PropAny(),
            ]),
            PropStringList('phandle-properties'),
        ])
    ])
])


def GetValidator():
  """Get a schema validator for use by another module

  Returns:
    CrosConfigValidator object
  """
  return CrosConfigValidator(SCHEMA, raise_on_error=True)


def ShowErrors(fname, errors):
  """Show validation errors

  Args:
    fname: Filename containng the errors
    errors: List of errors, each a string
  """
  print('%s:' % fname, file=sys.stderr)
  for error in errors:
    print(error, file=sys.stderr)
  print(file=sys.stderr)


def Main(argv=None):
  """Main program for validator

  This validates each of the provided files and prints the errors for each, if
  any.

  Args:
    argv: Arguments to the problem (excluding argv[0]); if None, uses sys.argv
  """
  if argv is None:
    argv = sys.argv[1:]
  args = ParseArgv(argv)
  validator = CrosConfigValidator(SCHEMA, args.raise_on_error)
  found_errors = False
  try:
    # If we are given partial files (.dtsi) then we compile them all into one
    # .dtb and validate that.
    if args.partial:
      errors = validator.Start(args.config, partial=True)
      fname = args.config[0]
      if errors:
        ShowErrors(fname, errors)
        found_errors = True

    # Otherwise process each file individually
    else:
      for fname in args.config:
        errors = validator.Start([fname])
        if errors:
          found_errors = True
          if errors:
            ShowErrors(fname, errors)
            found_errors = True
  except cros_build_lib.RunCommandError as e:
    if args.debug:
      raise
    print('Failed: %s' % e, file=sys.stderr)
    found_errors = True
  if found_errors:
    sys.exit(1)


if __name__ == "__main__":
  Main()
