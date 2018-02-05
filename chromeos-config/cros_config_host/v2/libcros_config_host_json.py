# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Crome OS Configuration access library.

Provides build-time access to the master configuration on the host. It is used
for reading from the master configuration. Consider using cros_config_host.py
for CLI access to this library.
"""

from __future__ import print_function

import json

from .cros_config_schema import TransformConfig
from ..libcros_config_host import CrosConfigImpl

LIB_FIRMWARE = '/lib/firmware'
UNIBOARD_JSON_INSTALL_PATH = 'usr/share/chromeos-config/config.json'


class CrosConfigJson(CrosConfigImpl):
  """The ChromeOS Configuration API for the host.

  CrosConfig is the top level API for accessing ChromeOS Configs from the host.
  This is the JSON based implementation of this API.

  Properties:
    models: All models in the CrosConfig tree, in the form of a dictionary:
            <model name: string, model: CrosConfig.Model>
  """
  def __init__(self, infile):
    super(CrosConfigJson, self).__init__(infile)
    self._json = json.loads(
        TransformConfig(self.infile.read(), drop_family=False))
    self.root = CrosConfigJson.MakeNode(self, None, '/', self._json)
    self.family = self.root.subnodes['chromeos'].subnodes.get('family')

  @staticmethod
  def MakeNode(cros_config, parent, name, json_node):
    """Make a new Node in the tree

    This create a new Node or Model object in the tree and recursively adds all
    subnodes to it.

    Args:
      cros_config: CrosConfig object
      parent: Parent Node object
      name: Name of this JSON node
      json_node: JSON node object containing the current node
    """
    node = CrosConfigJson.Node(cros_config, parent, name, json_node)
    if parent and parent.name == 'models':
      cros_config.models[node.name] = node
    node.default = node.FollowPhandle('default')

    node.ScanSubnodes()
    return node

  class Node(CrosConfigImpl.Node):
    """YAML implementation of a node"""
    def __init__(self, cros_config, parent, name, json_node):
      super(CrosConfigJson.Node, self).__init__(cros_config)
      self.parent = parent
      self.name = name
      self._json_node = json_node
      # Subnodes are set up in Model.ScanSubnodes()
      self.ScanForProperties(json_node)

    def ScanForProperties(self, json_node):
      if isinstance(json_node, list):
        for json_subnode in json_node:
          name = json_subnode['name']
          self.subnodes[name] = CrosConfigJson.MakeNode(
              self.cros_config, self, name, json_subnode)
      else:
        for name, json_subnode in json_node.iteritems():
          if isinstance(json_subnode, dict):
            if self.IsPhandleName(name):
              self.ScanForProperties(json_subnode)
            else:
              self.subnodes[name] = CrosConfigJson.MakeNode(
                  self.cros_config, self, name, json_subnode)
          elif isinstance(json_subnode, list):
            self.subnodes[name] = CrosConfigJson.MakeNode(
                self.cros_config, self, name, json_subnode)
          else:
            self.properties[name] = CrosConfigJson.Property(name, json_subnode)

    def FollowPhandle(self, prop_name):
      """Follow a property's phandle

      Args:
        prop_name: Property name to check

      Returns:
        Node that the property's phandle points to, or None if none
      """
      node = self.subnodes.get(prop_name)
      return node

    def GetPath(self):
      """Get the full path to a node.

      Returns:
        path to node as a string
      """
      parent = self.parent.GetPath() if self.parent else ''
      return '%s%s%s' % (parent, '/' if parent and self.parent.parent else '',
                         self.name)

    def IsPhandleName(self, _):
      # JSON never uses phandles
      return False

  class Property(CrosConfigImpl.Property):
    """FDT implementation of a property

    Properties:
      name: The name of the property.
      value: The value of the property.
      type: The FDT type of the property.
    """
    def __init__(self, name, json_prop):
      super(CrosConfigJson.Property, self).__init__()
      self._json_prop = json_prop
      self.name = name
      self.value = json_prop
      # TODO(athilenius): Transform these int types to something more useful
      self.type = str
