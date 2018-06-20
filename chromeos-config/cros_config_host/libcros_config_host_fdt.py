# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chrome OS Configuration access library (FDT).

Chrome OS Configuration access library for a master configuration using flat
device tree.
"""

from __future__ import print_function

from collections import OrderedDict

import os
import fdt
import validate_config

from libcros_config_host_base import BaseFile, CrosConfigBaseImpl, DeviceConfig
from libcros_config_host_base import FirmwareInfo, TouchFile

def GetFilename(node_path, props, fname_template):
  """Create a filename based on the given template and properties

  Args:
    node_path: Path of the node generating this filename (for error
        reporting only)
    props: Dict of properties which can be used in the template:
        key: Variable name
        value: Value of that variable
    fname_template: Filename template
  """
  template = fname_template.replace('$', '')
  try:
    return template.format(props, **props)
  except KeyError as e:
    raise ValueError(("node '%s': Format string '%s' has properties '%s' "
                      "but lacks '%s'")
                     % (node_path, template, props.keys(), e.message))


def GetPropFilename(node_path, props, fname_prop):
  """Create a filename based on the given template and properties

  Args:
    node_path: Path of the node generating this filename (for error
        reporting only)
    props: Dict of properties which can be used in the template:
        key: Variable name
        value: Value of that variable
    fname_prop: Name of property containing the template
  """
  template = props[fname_prop]
  return GetFilename(node_path, props, template)


class CrosConfigFdt(CrosConfigBaseImpl):
  """Flat Device Tree implementation of CrosConfig.

  This uses a device-tree file to hold this config. This provides efficient
  run-time access since there is no need to parse the whole file. It also
  supports links from one node to another, reducing redundancy in the config.

  Properties:
    phandle_to_node:
        Map of phandles to the assocated CrosConfigFdt.Node:
        key: Integer phandle value (>= 1)
        value: Associated CrosConfigFdt.Node object
    family: Family node (CrosConfigFdt.Node object)
    models: All models in the CrosConfigFdt tree, in the form of a
            dictionary:
            <model name: string, model: CrosConfigFdt.Node>
    phandle_props: Set of properties which can be phandles (i.e. point to
        another part of the config)
    root: Root node (CrosConigImpl.Node object)
    validator: Validator for the config (CrosConfigValidator object)
  """
  def __init__(self, infile):
    self.infile = infile
    self.models = OrderedDict()
    self.validator = validate_config.GetValidator()
    self.phandle_props = self.validator.GetPhandleProps()
    self._fdt = fdt.Fdt(self.infile)
    self._fdt.Scan()
    self.phandle_to_node = {}
    self.root = CrosConfigFdt.MakeNode(self, self._fdt.GetRoot())
    self.family = self.root.subnodes['chromeos'].subnodes['family']

  def GetDeviceConfigs(self):
    return self.models.values()

  def _GetProperty(self, absolute_path, property_name):
    """Internal function to read a property from anywhere in the tree

    Args:
      absolute_path: within the root node (e.g. '/chromeos/family/firmware')
      property_name: Name of property to get

    Returns:
      Property object, or None if not found
    """
    return self.root.PathProperty(absolute_path, property_name)

  def GetNode(self, absolute_path):
    """Internal function to get a node from anywhere in the tree

    Args:
      absolute_path: within the root node (e.g. '/chromeos/family/firmware')

    Returns:
      Node object, or None if not found
    """
    return self.root.PathNode(absolute_path)

  def GetFamilyNode(self, relative_path):
    return self.family.PathNode(relative_path)

  def GetFamilyProperty(self, relative_path, property_name):
    """Read a property from a family node

    Args:
      relative_path: Relative path within the family (e.g. '/firmware')
      property_name: Name of property to get

    Returns:
      Property object, or None if not found
    """
    return self.family.PathProperty(relative_path, property_name)

  @staticmethod
  def GetTargetDirectories():
    """Gets a dict of directory targets for each PropFile property

    Returns:
      Dict:
        key: Property name
        value: Ansolute path for this property
    """
    validator = validate_config.GetValidator()
    return validator.GetTargetDirectories()

  @staticmethod
  def GetPhandleProperties():
    """Gets a dict of properties which contain phandles

    Returns:
      Dict:
        key: Property name
        value: Ansolute path for this property
    """
    validator = validate_config.GetValidator()
    return validator.GetPhandleProps()

  class Node(DeviceConfig):
    """Represents a single node in the CrosConfig tree, including Model.

    A node can have several subnodes nodes, as well as several properties. Both
    can be accessed via .subnodes and .properties respectively. A few helpers
    are also provided to make node traversal a little easier.

    Properties:
      name: The name of the this node.
      subnodes: Child nodes, in the form of a dictionary:
                <node name: string, child node: CrosConfigFdt.Node>
      properties: All properties attached to this node in the form of a
                  dictionary: <name: string,
                  property: CrosConfigFdt.Property>
    """

    def __init__(self, cros_config, fdt_node):
      self.cros_config = cros_config
      self.subnodes = OrderedDict()
      self.properties = OrderedDict()
      self.default = None
      self.submodels = {}
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

    def GetName(self):
      return self.name

    def GetProperties(self, path):
      result = {}
      node = self.PathNode(path)
      if node and node.properties:
        for prop in node.properties.values():
          result[prop.name] = prop.value
      return result

    def FollowShare(self):
      """Follow a node's shares property

      Some nodes support sharing the properties of another node, e.g. firmware
      and whitelabel. This follows that share if it can find it. We don't need
      to be too careful to ignore invalid properties (e.g. whitelabel can only
      be in a model node) since validation takes care of that.

      Returns:
        Node that the share points to, or None if none
      """
      # TODO(sjg@chromium.org):
      # Note that the 'or i in self.subnodes' part is for yaml, where we use a
      # json file within libcros_config_host and this does not support links
      # between nodes (instead every use of a node blows it out fully at that
      # point in the tree). For now this is modelled as a subnode with the
      # name of the phandle, but at some point this will move to merging the
      # target node's properties with this node, so this whole function will
      # become unnecessary.
      share_prop = [i for i in self.cros_config.phandle_props
                    if i in self.properties or i in self.subnodes]
      if share_prop:
        return self.FollowPhandle(share_prop[0])
      return None

    def PathNode(self, relative_path):
      """Returns the CrosConfigFdt.Node at the relative path.

      This method is useful for accessing a nested child object at a relative
      path from a Node (or Model). The path must be separated with '/'
      delimiters. Return None if the path is invalid.

      Args:
        relative_path: A relative path string separated by '/', '/thermal'

      Returns:
        A CrosConfigFdt.Node at the path, or None if it doesn't exist
      """
      if not relative_path:
        return self
      path_parts = [path for path in relative_path.split('/') if path]
      if not path_parts:
        return self
      part = path_parts[0]
      if part in self.subnodes:
        sub_node = self.subnodes[part]

      # Handle a 'shares' property, which means we can grab nodes / properties
      # from the associated node.
      else:
        shared = self.FollowShare()
        if shared and part in shared.subnodes:
          sub_node = shared.subnodes[part]
        else:
          return None
      return sub_node.PathNode('/'.join(path_parts[1:]))

    def Property(self, property_name):
      """Get a property from a node

      This is a trivial function but it does provide some insulation against our
      internal data structures.

      Args:
        property_name: Name of property to find

      Returns:
        CrosConfigFdt.Property object that was found, or None if none
      """
      return self.properties.get(property_name)

    def GetStr(self, property_name):
      """Get a string value from a property

      Args:
        property_name: Name of property to access

      Returns:
        String value of property, or '' if not present
      """
      prop = self.Property(property_name)
      return prop.value if prop else ''

    def GetStrList(self, property_name):
      """Get a string-list value from a property

      Args:
        property_name: Name of property to access

      Returns:
        List of strings representing the value of the property, or [] if not
        present
      """
      prop = self.Property(property_name)
      if not prop:
        return []
      if not isinstance(prop.value, list):
        return [prop.value]
      return prop.value

    def GetBool(self, property_name):
      """Get a boolean value from a property

      Args:
        property_name: Name of property to access

      Returns:
        True if the property is present, False if not
      """
      return property_name in self.properties

    def GetProperty(self, path, name):
      result = self.PathProperty(path, name)
      return result.value if result else ''

    def PathProperty(self, relative_path, property_name):
      """Returns the value of a property relatative to this node

      This function honours the 'shared' property, by following the phandle and
      searching there, at any component of the path. It also honours the
      'default' property which is defined for nodes.

      Args:
        relative_path: A relative path string separated by '/', e.g. '/thermal'
        property_name: Name of property to look up, e.g 'dptf-dv'

      Returns:
        String value of property, or None if not found
      """
      child_node = self.PathNode(relative_path)
      if not child_node:
        shared = self.FollowShare()
        if shared:
          child_node = shared.PathNode(relative_path)
      if child_node:
        prop = child_node.properties.get(property_name)
        if not prop:
          shared = child_node.FollowShare()
          if shared:
            prop = shared.properties.get(property_name)
        if prop:
          return prop
      if self.default:
        return self.default.PathProperty(relative_path, property_name)
      return None

    @staticmethod
    def MergeProperties(props, node, ignore=''):
      if node:
        for name, prop in node.properties.iteritems():
          if (name not in props and not name.endswith('phandle') and
              name != ignore):
            props[name] = prop.value

    def GetMergedProperties(self, node, phandle_prop):
      """Obtain properties in two nodes linked by a phandle

      This is used to create a dict of the properties in a main node along with
      those found in a linked node. The link is provided by a property in the
      main node containing a single phandle pointing to the linked node.

      The result is a dict that combines both nodes' properties, with the
      linked node filling in anything missing. The main node's properties take
      precedence.

      Phandle properties and 'reg' properties are not included. The 'default'
      node is checked as well.

      Args:
        node: Main node to obtain properties from
        phandle_prop: Name of the phandle property to follow to get more
            properties

      Returns:
        dict containing property names and values from both nodes:
          key: property name
          value: property value
      """
      # First get all the property keys/values from the main node
      props = OrderedDict((prop.name, prop.value)
                          for prop in node.properties.values()
                          if prop.name not in [phandle_prop, 'bcs-type', 'reg'])

      # Follow the phandle and add any new ones we find
      self.MergeProperties(props, node.FollowPhandle(phandle_prop), 'bcs-type')
      if self.default:
        # Get the path of this node relative to its model. For example:
        # '/chromeos/models/pyro/thermal' will return '/thermal' in subpath.
        # Once crbug.com/775229 is completed, we will be able to do this in a
        # nicer way.
        _, _, _, _, subpath = node.GetPath().split('/', 4)
        default_node = self.default.PathNode(subpath)
        if default_node:
          self.MergeProperties(props, default_node, phandle_prop)
          self.MergeProperties(props, default_node.FollowPhandle(phandle_prop))
      return OrderedDict(sorted(props.iteritems()))

    def ScanSubnodes(self):
      """Collect a list of submodels"""
      if (self.name in self.cros_config.models and
          'submodels' in self.subnodes.keys()):
        for name, subnode in self.subnodes['submodels'].subnodes.iteritems():
          self.submodels[name] = subnode

    def SubmodelPathProperty(self, submodel_name, relative_path, property_name):
      """Reads a property from a submodel.

      Args:
        submodel_name: Submodel to read from
        relative_path: A relative path string separated by '/'.
        property_name: Name of property to read

      Returns:
        Value of property as a string, or None if not found
      """
      submodel = self.submodels.get(submodel_name)
      if not submodel:
        return None
      return submodel.PathProperty(relative_path, property_name)

    def GetFirmwareConfig(self):
      """Returns a map hierarchy of the firmware config."""
      firmware = self.PathNode('/firmware')
      if not firmware or firmware.GetBool('no-firmware'):
        return {}
      return self.GetMergedProperties(firmware, 'shares')

    def SetupModelProps(self, props):
      props['model'] = self.name
      props['MODEL'] = self.name.upper()

    def GetTouchFirmwareFiles(self):
      files = {}
      for device_name, props in self.GetTouchBspInfo():
        # Add a special property for the capitalised model name
        self.SetupModelProps(props)
        fw_prop_name = 'firmware-bin'
        fw_target_dir = self.cros_config.validator.GetModelTargetDir(
            '/touch/ANY', fw_prop_name)
        if not fw_target_dir:
          raise ValueError(("node '%s': Property '%s' does not have a " +
                            'target directory (internal error)') %
                           (device_name, fw_prop_name))
        sym_prop_name = 'firmware-sym'
        sym_target_dir = self.cros_config.validator.GetModelTargetDir(
            '/touch/ANY', 'firmware-symlink')
        if not sym_target_dir:
          raise ValueError(("node '%s': Property '%s' does not have a " +
                            'target directory (internal error)') %
                           (device_name, sym_prop_name))
        src = GetPropFilename(self.GetPath(), props, fw_prop_name)
        dest = src
        sym_fname = GetPropFilename(self.GetPath(), props, 'firmware-symlink')
        files[device_name] = TouchFile(src, os.path.join(fw_target_dir, dest),
                                       os.path.join(sym_target_dir, sym_fname))

      return files.values()

    def GetTouchBspInfo(self):
      touch = self.PathNode('/touch')
      if touch:
        for device in touch.subnodes.values():
          props = self.GetMergedProperties(device, 'touch-type')
          yield [device.name, props]

    def AllPathNodes(self, relative_path, whitelabel=False):
      """List all path nodes which match the relative path (including submodels)

      This looks in the model and all its submodels for this relative path.

      Args:
        relative_path: A relative path string separated by '/', '/thermal'
        whitelabel: True to look in the whitelabels subnode as well (for
            whiltelabel alternative schema)

      Returns:
        Dict of:
          key: Name of this model/submodel
          value: Node object for this model/submodel
      """
      path_nodes = {}
      node = self.PathNode(relative_path)
      if node:
        path_nodes[self.name] = node
      for submodel_node in self.submodels.values():
        node = submodel_node.PathNode(relative_path)
        if node:
          path_nodes[submodel_node.name] = node
      if whitelabel:
        whitelabels = self.PathNode('/whitelabels')
        if whitelabels:
          for node in whitelabels.subnodes.values():
            path_nodes[node.name] = node
      return path_nodes

    def GetArcFiles(self):
      files = {}
      prop = 'hw-features'
      arc = self.PathNode('/arc')
      target_dir = self.cros_config.validator.GetModelTargetDir('/arc', prop)
      if arc and prop in arc.properties:
        files['base'] = BaseFile(
            arc.properties[prop].value,
            os.path.join(target_dir, arc.properties[prop].value))
      return files.values()

    def GetAudioFiles(self):
      card = None  # To keep pylint happy since we use it in this function:
      name = ''

      def _AddAudioFile(prop_name, dest_template, dirname=''):
        """Helper to add a single audio file

        If present in the configuration, this adds an audio file containing the
        source and destination file.
        """
        if prop_name in props:
          target_dir = self.cros_config.validator.GetModelTargetDir(
              '/audio/ANY', prop_name)
          if not target_dir:
            raise ValueError(
                ("node '%s': Property '%s' does not have a " +
                 'target directory (internal error)') % (card.name, prop_name))
          files[name, prop_name] = BaseFile(
              GetPropFilename(self.GetPath(), props, prop_name),
              os.path.join(target_dir, dirname,
                           GetFilename(self.GetPath(), props, dest_template)))

      files = {}
      audio_nodes = self.AllPathNodes('/audio')
      for name, audio in audio_nodes.iteritems():
        for card in audio.subnodes.values():
          # First get all the property keys/values from the current node
          props = self.GetMergedProperties(card, 'audio-type')
          self.SetupModelProps(props)

          cras_dir = props.get('cras-config-dir')
          if not cras_dir:
            raise ValueError(
                ("node '%s': Should have a cras-config-dir") % (card.GetPath()))
          _AddAudioFile('volume', '{card}', cras_dir)
          _AddAudioFile('dsp-ini', 'dsp.ini', cras_dir)

          # Allow renaming this file to something other than HiFi.conf
          _AddAudioFile(
              'hifi-conf',
              os.path.join('{card}.{ucm-suffix}',
                           os.path.basename(props['hifi-conf'])))
          _AddAudioFile('alsa-conf',
                        '{card}.{ucm-suffix}/{card}.{ucm-suffix}.conf')

          # Non-Intel platforms don't use topology-bin
          topology = props.get('topology-bin')
          if topology:
            _AddAudioFile('topology-bin',
                          os.path.basename(props.get('topology-bin')))

      return files.values()

    def GetThermalFiles(self):
      files = {}
      prop = 'dptf-dv'
      for name, thermal in self.AllPathNodes('/thermal').iteritems():
        target_dir = self.cros_config.validator.GetModelTargetDir(
            '/thermal', prop)
        if prop in thermal.properties:
          files[name] = BaseFile(
              thermal.properties[prop].value,
              os.path.join(target_dir, thermal.properties[prop].value))
      return files.values()

    def GetFirmwareInfo(self):
      whitelabel = self.FollowPhandle('whitelabel')
      base_model = whitelabel if whitelabel else self
      firmware_node = self.PathNode('/firmware')
      base_firmware_node = base_model.PathNode('/firmware')

      # If this model shares firmware with another model, get our
      # images from there.
      image_node = base_firmware_node.FollowPhandle('shares')
      if image_node:
        # Override the node - use the shared firmware instead.
        node = image_node
        shared_model = image_node.name
      else:
        node = base_firmware_node
        shared_model = None
      key_id = firmware_node.GetStr('key-id')
      if firmware_node.GetBool('no-firmware'):
        return {}

      have_image = True
      if (whitelabel and base_firmware_node and
          base_firmware_node.Property('sig-id-in-customization-id')):
        # For zero-touch whitelabel devices, we don't need to generate anything
        # since the device will never report this model at runtime. The
        # signature ID will come from customization_id.
        have_image = False

      # Firmware configuration supports both sharing the same fw image across
      # multiple models and pinning specific models to different fw revisions.
      # For context, see:
      # https://chromium.googlesource.com/chromiumos/platform2/+/master/chromeos-config/README.md
      #
      # In order to support this, the firmware build target needs to be
      # decoupled from models (since it can be shared).  This was supported
      # in the config with 'build-targets', which drives the actual firmware
      # build/targets.
      #
      # This takes advantage of that same config to determine what the target
      # FW image will be named in the build output.  This allows a many to
      # many mapping between models and firmware images.
      build_node = node.PathNode('build-targets')
      if build_node:
        bios_build_target = build_node.GetStr('coreboot')
        ec_build_target = build_node.GetStr('ec')
      else:
        bios_build_target, ec_build_target = None, None

      main_image_uri = node.GetStr('main-image')
      main_rw_image_uri = node.GetStr('main-rw-image')
      ec_image_uri = node.GetStr('ec-image')
      pd_image_uri = node.GetStr('pd-image')
      create_bios_rw_image = node.GetBool('create-bios-rw-image')
      extra = node.GetStrList('extra')

      tools = node.GetStrList('tools')

      whitelabels = self.PathNode('/whitelabels')
      if whitelabels or firmware_node.GetBool('sig-id-in-customization-id'):
        sig_id = 'sig-id-in-customization-id'
      else:
        sig_id = self.name

      info = FirmwareInfo(self.name, shared_model, key_id, have_image,
                          bios_build_target, ec_build_target, main_image_uri,
                          main_rw_image_uri, ec_image_uri, pd_image_uri, extra,
                          create_bios_rw_image, tools, sig_id)

      # Handle the alternative schema, where whitelabels are in a single model
      # and have whitelabel tags to distinguish them.
      result = OrderedDict()
      result[self.name] = info
      if whitelabels:
        # Sort these to get a deterministic ordering with yaml (for tests).
        for name in sorted(whitelabels.subnodes):
          whitelabel = whitelabels.subnodes[name]
          key_id = whitelabel.GetStr('key-id')
          whitelabel_name = '%s-%s' % (base_model.name, whitelabel.name)
          result[whitelabel_name] = info._replace(
              model=whitelabel_name,
              key_id=key_id,
              have_image=False,
              sig_id=whitelabel_name)
      return result

    def GetWallpaperFiles(self):
      """Get a set of wallpaper files used for this model"""
      wallpapers = set()
      for node in self.AllPathNodes('/', whitelabel=True).values():
        wallpaper = node.properties.get('wallpaper')
        if wallpaper:
          wallpapers.add(wallpaper.value)
      return wallpapers

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

  class Property(object):
    """FDT implementation of a property

    Properties:
      name: The name of the property.
      value: The value of the property.
      type: The Python type of the property.
    """
    def __init__(self, fdt_prop):
      self._fdt_prop = fdt_prop
      self.name = fdt_prop.name
      self.value = fdt_prop.value
      self.type = fdt_prop.type

    def GetPhandle(self):
      """Get the value of a property as a phandle

      Returns:
        Property's phandle as an integer (> 0)
      """
      return self._fdt_prop.GetPhandle()
