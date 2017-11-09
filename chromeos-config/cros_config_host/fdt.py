# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# Taken from U-Boot v2017.07 (tools/dtoc)

"""The higher level FDT library for parsing and interfacing with a dtb."""

from __future__ import print_function

from collections import OrderedDict
import struct

import libfdt

from . import fdt_util

# This deals with a device tree, presenting it as an assortment of Node and
# Prop objects, representing nodes and properties, respectively. This file
# contains the base classes and defines the high-level API. You can use
# FdtScan() as a convenience function to create and scan an Fdt.

# This implementation uses a libfdt Python library to access the device tree,
# so it is fairly efficient.

# A list of types we support
(TYPE_BYTE, TYPE_INT, TYPE_STRING, TYPE_BOOL, TYPE_INT64) = range(5)


def CheckErr(errnum, msg):
  """Checks for a lib fdt error and prints it out if one exists.

  Args:
    errnum: The error number returned by lib fdt
    msg: The message to bundle with the error print
  """
  if errnum:
    raise ValueError('Error %d: %s: %s' %
                     (errnum, libfdt.fdt_strerror(errnum), msg))


class Prop(object):
  """A device tree property

  Properties:
    fdt: Device tree object
    name: Property name (as per the device tree)
    value: Property value as a string of bytes, or a list of strings of
      bytes
    type: Value type
    data: The string data
  """
  def __init__(self, fdt, node, offset, name, data):
    self.fdt = fdt
    self.node = node
    self._offset = offset
    self.name = name
    self.value = None
    self.data = str(data)
    if not data:
      self.type = TYPE_BOOL
      self.value = True
      return
    self.type, self.value = self.BytesToValue(data)

  def GetPhandle(self):
    """Get a (single) phandle value from a property

    Gets the phandle value from a property and returns it as an integer
    """
    return fdt_util.fdt32_to_cpu(self.value[:4])

  def LookupPhandle(self):
    """Look up a node by its phandle (treating this property as a phandle)

    Returns:
      Node object, or None if not found
    """
    return self.fdt.LookupPhandle(self.GetPhandle())

  def BytesToValue(self, data):
    """Converts a string of bytes into a type and value

    Args:
      data: A string containing bytes

    Returns:
      A tuple:
        Type of data
        Data, either a single element or a list of elements. Each
        element is one of:
          TYPE_STRING: string value from the property
          TYPE_INT: a byte-swapped integer stored as a 4-byte string
          TYPE_BYTE: a byte stored as a single-byte string
    """
    data = str(data)
    size = len(data)
    strings = data.split('\0')
    is_string = True
    count = len(strings) - 1
    if count > 0 and not strings[-1]:
      for string in strings[:-1]:
        if not string:
          is_string = False
          break
        for ch in string:
          if ch < ' ' or ch > '~':
            is_string = False
            break
    else:
      is_string = False
    if is_string:
      if count == 1:
        return TYPE_STRING, strings[0]
      else:
        return TYPE_STRING, strings[:-1]
    if size % 4:
      if size == 1:
        return TYPE_BYTE, data[0]
      else:
        return TYPE_BYTE, list(data)
    val = []
    for i in range(0, size, 4):
      val.append(data[i:i + 4])
    if size == 4:
      return TYPE_INT, val[0]
    else:
      return TYPE_INT, val

  def GetEmpty(self, value_type):
    """Get an empty / zero value of the given type

    Returns:
      A single value of the given type
    """
    if value_type == TYPE_BYTE:
      return chr(0)
    elif value_type == TYPE_INT:
      return struct.pack('<I', 0)
    elif value_type == TYPE_STRING:
      return ''
    else:
      return True


class Node(object):
  """A device tree node

  Properties:
    offset: Integer offset in the device tree
    name: Device tree node tname
    path: Full path to node, along with the node name itself
    fdt: Device tree object
    subnodes: A list of subnodes for this node, each a Node object
    props: A dict of properties for this node, each a Prop object.
      Keyed by property name
  """
  def __init__(self, fdt, parent, offset, name, path):
    self.fdt = fdt
    self.parent = parent
    self.offset = offset
    self.name = name
    self.path = path
    self.subnodes = OrderedDict()
    self.props = OrderedDict()

  def FindNode(self, name):
    """Find a node given its name

    Args:
      name: Node name to look for

    Returns:
      Node object if found, else None
    """
    for subnode in self.subnodes.values():
      if subnode.name == name:
        return subnode
    return None

  def Offset(self):
    """Returns the offset of a node, after checking the cache

    This should be used instead of self.offset directly, to ensure that
    the cache does not contain invalid offsets.
    """
    self.fdt.CheckCache()
    return self.offset

  def Scan(self):
    """Scan a node's properties and subnodes

    This fills in the props and subnodes properties, recursively
    searching into subnodes so that the entire tree is built.
    """
    self.props = self.fdt.GetProps(self)
    phandle = self.props.get('phandle')
    if phandle:
      val = fdt_util.fdt32_to_cpu(phandle.value)
      self.fdt.phandle_to_node[val] = self

    offset = libfdt.fdt_first_subnode(self.fdt.GetFdt(), self.Offset())
    while offset >= 0:
      sep = '' if self.path[-1] == '/' else '/'
      name = self.fdt.fdt_obj.get_name(offset)
      path = self.path + sep + name
      node = Node(self.fdt, self, offset, name, path)
      self.subnodes[name] = node

      node.Scan()
      offset = libfdt.fdt_next_subnode(self.fdt.GetFdt(), offset)

  def Refresh(self, my_offset):
    """Fix up the offset for each node, recursively

    Note: This does not take account of property offsets - these will not
    be updated.
    """
    if self.offset != my_offset:
      #print '%s: %d -> %d\n' % (self.path, self._offset, my_offset)
      self.offset = my_offset
    offset = libfdt.fdt_first_subnode(self.fdt.GetFdt(), self.offset)
    for subnode in self.subnodes.values():
      subnode.Refresh(offset)
      offset = libfdt.fdt_next_subnode(self.fdt.GetFdt(), offset)

  def DeleteProp(self, prop_name):
    """Delete a property of a node

    The property is deleted and the offset cache is invalidated.

    Args:
      prop_name: Name of the property to delete

    Raises:
      ValueError if the property does not exist
    """
    CheckErr(libfdt.fdt_delprop(self.fdt.GetFdt(), self.Offset(), prop_name),
             "Node '%s': delete property: '%s'" % (self.path, prop_name))
    del self.props[prop_name]
    self.fdt.Invalidate()


class Fdt(object):
  """Provides simple access to a flat device tree blob using libfdts.

  Properties:
    infile: The File to read the dtb from
    _root: Root of device tree (a Node object)
  """
  def __init__(self, infile):
    self._root = None
    self._cached_offsets = False
    self.phandle_to_node = OrderedDict()
    self._fdt = bytearray(infile.read())
    self.fdt_obj = libfdt.Fdt(self._fdt)

  def LookupPhandle(self, phandle):
    """Look up a node by its phandle

    Args:
      phandle: Phandle to look up (integer > 0)

    Returns:
      Node object, or None if not found
    """
    return self.phandle_to_node.get(phandle)

  def Scan(self):
    """Scan a device tree, building up a tree of Node objects

    This fills in the self._root property

    Args:
      root: Ignored

    TODO(sjg@chromium.org): Implement the 'root' parameter
    """
    self._root = self.Node(self, None, 0, '/', '/')
    self._root.Scan()

  def GetRoot(self):
    """Get the root Node of the device tree

    Returns:
      The root Node object
    """
    return self._root

  def GetNode(self, path):
    """Look up a node from its path

    Args:
      path: Path to look up, e.g. '/microcode/update@0'

    Returns:
      Node object, or None if not found
    """
    node = self._root
    for part in path.split('/')[1:]:
      node = node.FindNode(part)
      if not node:
        return None
    return node

  def Flush(self, outfile):
    """Flush device tree changes to the given file

    Args:
      outfile: The file to write the device tree out to
    """
    outfile.write(self._fdt)

  def Pack(self):
    """Pack the device tree down to its minimum size

    When nodes and properties shrink or are deleted, wasted space can
    build up in the device tree binary.
    """
    CheckErr(libfdt.fdt_pack(self._fdt), 'pack')
    fdt_len = libfdt.fdt_totalsize(self._fdt)
    del self._fdt[fdt_len:]

  def GetFdt(self):
    """Get the contents of the FDT

    Returns:
      The FDT contents as a string of bytes
    """
    return self._fdt


  def GetProps(self, node):
    """Get all properties from a node.

    Args:
      node: A Node object to get the properties for.

    Returns:
      A dictionary containing all the properties, indexed by node name.
      The entries are Prop objects.

    Raises:
      ValueError: if the node does not exist.
    """
    props_dict = OrderedDict()
    poffset = libfdt.fdt_first_property_offset(self._fdt, node.offset)
    while poffset >= 0:
      p = self.fdt_obj.get_property_by_offset(poffset)
      prop = Prop(node.fdt, node, poffset, p.name, p.value)
      props_dict[prop.name] = prop

      poffset = libfdt.fdt_next_property_offset(self._fdt, poffset)
    return props_dict

  def Invalidate(self):
    """Mark our offset cache as invalid"""
    self._cached_offsets = False

  def CheckCache(self):
    """Refresh the offset cache if needed"""
    if self._cached_offsets:
      return
    self.Refresh()
    self._cached_offsets = True

  def Refresh(self):
    """Refresh the offset cache"""
    self._root.Refresh(0)

  def GetStructOffset(self, offset):
    """Get the file offset of a given struct offset

    Args:
      offset: Offset within the 'struct' region of the device tree

    Returns:
      Position of @offset within the device tree binary
    """
    return libfdt.fdt_off_dt_struct(self._fdt) + offset

  @classmethod
  def Node(cls, fdt, parent, offset, name, path):
    """Create a new node

    This is used by Fdt.Scan() to create a new node using the correct
    class.

    Args:
      fdt: Fdt object
      parent: Parent node, or None if this is the root node
      offset: Offset of node
      name: Node name
      path: Full path to node
    """
    node = Node(fdt, parent, offset, name, path)
    return node

def FdtScan(fname):
  """Returns a new Fdt object from the implementation we are using"""
  with open(fname) as fd:
    dtb = Fdt(fd)
  dtb.Scan()
  return dtb
