# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chrome OS Configuration access library (FDT).

Chrome OS Configuration access library for a master configuration using flat
device tree.
"""

from __future__ import print_function

from collections import OrderedDict

from . import fdt
import libcros_config_host

class CrosConfigFdt(libcros_config_host.CrosConfigImpl):
  """Flat Device Tree implementation of CrosConfig.

  This uses a device-tree file to hold this config. This provides efficient
  run-time access since there is no need to parse the whole file. It also
  supports links from one node to another, reducing redundancy in the config.

  Properties:
    phandle_to_node: Map of phandles to the assocated CrosConfigImpl.Node:
        key: Integer phandle value (>= 1)
        value: Associated CrosConfigImpl.Node object
    family: Family node (CrosConigImpl.Node object)
  """
  def __init__(self, infile):
    super(CrosConfigFdt, self).__init__(infile)
    self._fdt = fdt.Fdt(self.infile)
    self._fdt.Scan()
    self.phandle_to_node = {}
    self.root = CrosConfigFdt.MakeNode(self, self._fdt.GetRoot())
    self.family = self.root.subnodes['chromeos'].subnodes['family']

  @staticmethod
  def MakeNode(cros_config, fdt_node):
    """Make a new Node in the tree

    This create a new Node or Model object in the tree and recursively adds all
    subnodes to it. Any phandles found update the phandle_to_node map.

    Args:
      cros_config: CrosConfig object
      fdt_node: fdt.Node object containing the device-tree node
    """
    node = CrosConfigFdt.Node(cros_config, fdt_node)
    if fdt_node.parent and fdt_node.parent.name == 'models':
      cros_config.models[node.name] = node
    if 'phandle' in node.properties:
      phandle = fdt_node.props['phandle'].GetPhandle()
      cros_config.phandle_to_node[phandle] = node
    node.default = node.FollowPhandle('default')
    for subnode in fdt_node.subnodes.values():
      node.subnodes[subnode.name] = CrosConfigFdt.MakeNode(cros_config, subnode)
    node.ScanSubnodes()
    return node

  class Node(libcros_config_host.CrosConfigImpl.Node):
    """FDT implementation of a node"""
    def __init__(self, cros_config, fdt_node):
      super(CrosConfigFdt.Node, self).__init__(cros_config)
      self._fdt_node = fdt_node
      self.name = fdt_node.name
      # Subnodes are set up in Model.ScanSubnodes()
      self.properties = OrderedDict((n, CrosConfigFdt.Property(p))
                                    for n, p in fdt_node.props.iteritems())

    def GetPath(self):
      """Get the full path to a node.

      Returns:
        path to node as a string
      """
      return self._fdt_node.path

    def FollowPhandle(self, prop_name):
      """Follow a property's phandle

      Args:
        prop_name: Property name to check

      Returns:
        Node that the property's phandle points to, or None if none
      """
      prop = self.properties.get(prop_name)
      if not prop:
        return None
      return self.cros_config.phandle_to_node[prop.GetPhandle()]

  class Property(libcros_config_host.CrosConfigImpl.Property):
    """FDT implementation of a property

    Properties:
      name: The name of the property.
      value: The value of the property.
      type: The FDT type of the property.
    """
    def __init__(self, fdt_prop):
      super(CrosConfigFdt.Property, self).__init__()
      self._fdt_prop = fdt_prop
      self.name = fdt_prop.name
      self.value = fdt_prop.value
      # TODO(athilenius): Transform these int types to something more useful
      self.type = fdt_prop.type

    def GetPhandle(self):
      """Get the value of a property as a phandle

      Returns:
        Property's phandle as an integer (> 0)
      """
      return self._fdt_prop.GetPhandle()
