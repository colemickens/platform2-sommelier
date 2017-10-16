# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Schema elements.

This module provides schema elements that can be used to build up a schema for
validation of the master configuration
"""

from __future__ import print_function

import re


def CheckPhandleTarget(val, target, target_path_match):
  """Check that the target of a phandle matches a pattern

  Args:
    val: Validator (used for model list, etc.)
    target: Target node path (string)
    target_path_match: Match string. This is the full path to the node that the
        target must point to. Some 'wildcard' nodes are supported in the path:

           MODEL - matches any model node
           SUBMODEL - matches any submodel when MODEL is earlier in the path
           ANY - matches any node

  Returns:
    True if the target matches, False if not
  """
  model = None
  parts = target_path_match.split('/')
  target_parts = target.path.split('/')
  ok = len(parts) == len(target_parts)
  if ok:
    for i, part in enumerate(parts):
      if part == 'MODEL':
        if target_parts[i] in val.model_list:
          model = target_parts[i]
          continue
      if part == 'SUBMODEL' and model:
        if target_parts[i] in val.submodel_list[model]:
          continue
      elif part == 'ANY':
        continue
      if part != target_parts[i]:
        ok = False
  return ok


class SchemaElement(object):
  """A schema element, either a property or a subnode

  Args:
    name: Name of schema eleent
    prop_type: String describing this property type
    required: True if this element is mandatory, False if optional
    conditional_props: Properties which control whether this element is present.
       Dict:
         key: name of controlling property
         value: True if the property must be present, False if it must be absent
  """
  def __init__(self, name, prop_type, required=False, conditional_props=None):
    self.name = name
    self.prop_type = prop_type
    self.required = required
    self.conditional_props = conditional_props
    self.parent = None

  def Validate(self, val, prop):
    """Validate the schema element against the given property.

    This method is overridden by subclasses. It should call val.Fail() if there
    is a problem during validation.

    Args:
      val: CrosConfigValidator object
      prop: Prop object of the property
    """
    pass

  def Present(self, sibling_names):
    """Check whether a schema element should be present

    This handles the conditional_props feature. The list of names of sibling
    nodes/properties that are actually present is checked to see if any of them
    conflict with the conditional properties for this node. If there is a
    conflict, then this element is considered to be absent.

    Args:
      sibling_names: List of sibling node/property names

    Returns:
      True if this element is present, False if absent
    """
    if self.conditional_props and sibling_names:
      parent_props = [e.name for e in self.parent.elements]
      for name, value in self.conditional_props.iteritems():
        if name in parent_props and value != (name in sibling_names):
          return False
    return True


class PropDesc(SchemaElement):
  """A generic property schema element (base class for properties)"""
  def __init__(self, name, prop_type, required=False, conditional_props=None):
    super(PropDesc, self).__init__(name, prop_type, required, conditional_props)


class PropString(PropDesc):
  """A string-property

  Args:
    str_pattern: Regex to use to validate the string
  """
  def __init__(self, name, required=False, str_pattern='',
               conditional_props=None):
    super(PropString, self).__init__(name, 'string', required,
                                     conditional_props)
    self.str_pattern = str_pattern

  def Validate(self, val, prop):
    """Check the string with a regex"""
    if not self.str_pattern:
      return
    pattern = '^' + self.str_pattern + '$'
    m = re.match(pattern, prop.value)
    if not m:
      val.Fail(prop.node.path, "'%s' value '%s' does not match pattern '%s'" %
               (prop.name, prop.value, pattern))


class PropStringList(PropDesc):
  """A string-list property schema element

  Note that the list may be empty in which case no validation is performed.

  Args:
    str_pattern: Regex to use to validate the string
  """
  def __init__(self, name, required=False, str_pattern='',
               conditional_props=None):
    super(PropStringList, self).__init__(name, 'stringlist', required,
                                         conditional_props)
    self.str_pattern = str_pattern

  def Validate(self, val, prop):
    """Check each item of the list with a regex"""
    if not self.str_pattern:
      return
    pattern = '^' + self.str_pattern + '$'
    for item in prop.value:
      m = re.match(pattern, item)
      if not m:
        val.Fail(prop.node.path, "'%s' value '%s' does not match pattern '%s'" %
                 (prop.name, item, pattern))


class PropPhandleTarget(PropDesc):
  """A phandle-target property schema element

  A phandle target can be pointed to by another node using a phandle property.
  """
  def __init__(self, required=False, conditional_props=None):
    super(PropPhandleTarget, self).__init__('phandle', 'phandle-target',
                                            required, conditional_props)


class PropPhandle(PropDesc):
  """A phandle property schema element

  Phandle properties point to other nodes, and allow linking from one node to
  another.

  Properties:
    target_path_match: String to use to validate the target of this phandle.
        It is the full path to the node that it must point to. See
        CheckPhandleTarget for details.
  """
  def __init__(self, name, target_path_match, required=False,
               conditional_props=None):
    super(PropPhandle, self).__init__(name, 'phandle', required,
                                      conditional_props)
    self.target_path_match = target_path_match

  def Validate(self, val, prop):
    """Check that this phandle points to the correct place"""
    phandle = prop.GetPhandle()
    target = prop.fdt.LookupPhandle(phandle)
    if not CheckPhandleTarget(val, target, self.target_path_match):
      val.Fail(prop.node.path, "Phandle '%s' targets node '%s' which does not "
               "match pattern '%s'" % (prop.name, target.path,
                                       self.target_path_match))


class PropCustom(PropDesc):
  """A custom property with its own validator

  Properties:
    validator: Function to call to validate this property
  """
  def __init__(self, name, validator, required=False, conditional_props=None):
    super(PropCustom, self).__init__(name, 'custom', required,
                                     conditional_props)
    self.validator = validator

  def Validate(self, val, prop):
    """Validator for this property

    This should be a static method in CrosConfigValidator.

    Args:
      val: CrosConfigValidator object
      prop: Prop object of the property
    """
    self.validator(val, prop)


class NodeDesc(SchemaElement):
  """A generic node schema element (base class for nodes)"""
  def __init__(self, name, required=False, elements=None,
               conditional_props=None):
    super(NodeDesc, self).__init__(name, 'node', required,
                                   conditional_props)
    self.elements = elements
    for element in elements:
      element.parent = self

  def GetNodes(self):
    """Get a list of schema elements which are nodes

    Returns:
      List of objects, each of which has NodeDesc as a base class
    """
    return [n for n in self.elements if isinstance(n, NodeDesc)]


class NodeModel(NodeDesc):
  """A generic node schema element (base class for nodes)"""
  def __init__(self, elements):
    super(NodeModel, self).__init__('MODEL', elements=elements)


class NodeSubmodel(NodeDesc):
  """A generic node schema element (base class for nodes)"""
  def __init__(self, elements):
    super(NodeSubmodel, self).__init__('SUBMODEL', elements=elements)


class NodeAny(NodeDesc):
  """A generic node schema element (base class for nodes)"""
  def __init__(self, name_pattern, elements):
    super(NodeAny, self).__init__('ANY', elements=elements)
    self.name_pattern = name_pattern

  def Validate(self, val, node):
    """Check the name with a regex"""
    if not self.name_pattern:
      return
    pattern = '^' + self.name_pattern + '$'
    m = re.match(pattern, node.name)
    if not m:
      val.Fail(node.path, "Node name '%s' does not match pattern '%s'" %
               (node.name, pattern))
