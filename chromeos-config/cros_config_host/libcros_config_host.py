# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Crome OS Configuration access library.

Provides build-time access to the master configuration on the host. It is used
for reading from the master configuration. Consider using cros_config_host.py
for CLI access to this library.
"""

from __future__ import print_function

from collections import namedtuple, OrderedDict
import os
import sys

from . import fdt, validate_config

# Represents a single touch firmware file which needs to be installed:
#   firmware: source filename of firmware file. This is installed in a
#       directory in the root filesystem
#   symlink: name of symbolic link to put in LIB_FIRMWARE to point to the touch
#       firmware. This is where Linux finds the firmware at runtime.
TouchFile = namedtuple('TouchFile', ['firmware', 'symlink'])

# Represents a single file which needs to be installed:
#   source: Source filename within ${FILESDIR}
#   dest: Destination filename in the root filesystem
BaseFile = namedtuple('BaseFile', ['source', 'dest'])

# Represents information needed to create firmware for a model:
#   model: Name of model (e.g 'reef'). Also used as the signature ID for signing
#   shared_model: Name of model containing the shared firmware used by this
#       model, or None if this model has its own firmware images
#   key_id: Key ID used to sign firmware for this model (e.g. 'REEF')
#   have_image: True if we need to generate a setvars.sh file for this model.
#       If this is False it indicates that the model will never be detected at
#       run-time since it is a zero-touch whitelabel model. The signature ID
#       will be obtained from the customization_id in VPD when needed. Signing
#       instructions should still be generated for this model.
#   bios_build_target: Build target to use to build the BIOS, or None if none
#   ec_build_target: Build target to use to build the EC, or None if none
#   main_image_uri: URI to use to obtain main firmware image (e.g.
#       'bcs://Caroline.2017.21.1.tbz2')
#   ec_image_uri: URI to use to obtain the EC (Embedded Controller) firmware
#       image
#   pd_image_uri: URI to use to obtain the PD (Power Delivery controller)
#       firmware image
#   extra: List of extra files to include in the firmware update, each a string
#   create_bios_rw_image: True to create a RW BIOS image
#   tools: List of tools to include in the firmware update
#   sig_id: Signature ID to put in the setvars.sh file. This is normally the
#       same as the model, since that is what we use for signature ID. But for
#       zero-touch whitelabel this is 'sig-id-in-customization-id' since we do
#       not know the signature ID until we look up in VPD.
FirmwareInfo = namedtuple(
    'FirmwareInfo',
    ['model', 'shared_model', 'key_id', 'have_image',
     'bios_build_target', 'ec_build_target', 'main_image_uri',
     'main_rw_image_uri', 'ec_image_uri', 'pd_image_uri',
     'extra', 'create_bios_rw_image', 'tools', 'sig_id'])

# Known directories for installation
# TODO(sjg@chromium.org): Move these to the schema
LIB_FIRMWARE = '/lib/firmware'

UNIBOARD_DTB_INSTALL_PATH = 'usr/share/chromeos-config/config.dtb'


class PathComponent(object):
  """A component in a directory/file tree

  Properties:
    name: Name this component
    children: Dict of children:
      key: Name of child
      value: PathComponent object for child
  """
  def __init__(self, name):
    self.name = name
    self.children = dict()

  def AddPath(self, path):
    parts = path.split('/', 1)
    part = parts[0]
    rest = parts[1] if len(parts) > 1 else ''
    child = self.children.get(part)
    if not child:
      child = PathComponent(part)
      self.children[part] = child
    if rest:
      child.AddPath(rest)

  def ShowTree(self, base_path, path='', indent=0):
    """Show a tree of file paths

    This shows a component and all its children. Nodes can either be directories
    or files. Each file is shown with its size, or 'missing' if not found.

    Args:
      base_path: Base path where the actual files can be found
      path: Path of this component relative to the root (e.g. 'etc/cras/)
      indent: Indent level we are up to (0 = first)
    """
    path = os.path.join(path, self.name)
    fname = os.path.join(base_path, path)
    if os.path.isdir(fname):
      status = ''
    elif os.path.exists(fname):
      status = os.stat(fname).st_size
    else:
      status = 'missing'
    print('%-10s%s%s%s' % (status, '   ' * indent, self.name,
                           self.children and '/' or ''))
    for child in sorted(self.children.keys()):
      self.children[child].ShowTree(base_path, path, indent + 1)


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
    raise ValueError(("node '%s': Format string '%s' has properties '%s' " +
                      "but lacks '%s'") %
                     (node_path, template, props.keys(), e.message))

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


class CrosConfig(object):
  """The ChromeOS Configuration API for the host.

  CrosConfig is the top level API for accessing ChromeOS Configs from the host.

  Properties:
    models: All models in the CrosConfig tree, in the form of a dictionary:
            <model name: string, model: CrosConfig.Model>
    phandle_to_node: Map of phandles to the assocated CrosConfig.Node:
        key: Integer phandle value (>= 1)
        value: Associated CrosConfig.Node object
  """
  def __init__(self, infile=None):
    if not infile:
      if 'SYSROOT' not in os.environ:
        raise ValueError('No master configuration is available outside the '
                         'ebuild environemnt. You must specify one')
      fname = os.path.join(os.environ['SYSROOT'], UNIBOARD_DTB_INSTALL_PATH)
      with open(fname) as infile:
        self._fdt = fdt.Fdt(infile)
    else:
      self._fdt = fdt.Fdt(infile)
    self._fdt.Scan()
    self.phandle_to_node = {}
    self.models = OrderedDict()
    self.root = CrosConfig.MakeNode(self, self._fdt.GetRoot())
    self.validator = validate_config.GetValidator()
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
    if fdt_node.parent and fdt_node.parent.name == 'models':
      node = CrosConfig.Model(cros_config, fdt_node)
      cros_config.models[node.name] = node
    else:
      node = CrosConfig.Node(cros_config, fdt_node)
    if 'phandle' in node.properties:
      phandle = fdt_node.props['phandle'].GetPhandle()
      cros_config.phandle_to_node[phandle] = node
    for subnode in fdt_node.subnodes.values():
      node.subnodes[subnode.name] = CrosConfig.MakeNode(cros_config, subnode)
    node.ScanSubnodes()
    return node

  def _GetProperty(self, absolute_path, property_name):
    """Internal function to read a property from anywhere in the tree

    Args:
      absolute_path: within the root node (e.g. '/chromeos/family/firmware')
      property_name: Name of property to get

    Returns:
      Property object, or None if not found
    """
    return self.root.PathProperty(absolute_path, property_name)

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

  def GetFirmwareUris(self):
    """Returns a list of (string) firmware URIs.

    Generates and returns a list of firmeware URIs for all model. These URIs
    can be used to pull down remote firmware packages.

    Returns:
      A list of (string) full firmware URIs, or an empty list on failure.
    """
    uris = set()
    for model in self.models.values():
      uris.update(set(model.GetFirmwareUris()))
    return sorted(list(uris))

  def GetTouchFirmwareFiles(self):
    """Get a list of unique touch firmware files for all models

    Returns:
      List of TouchFile objects representing all the touch firmware referenced
      by all models
    """
    file_set = set()
    for model in self.models.values():
      for files in model.GetTouchFirmwareFiles().values():
        file_set.add(files)

    return sorted(file_set, key=lambda files: files.firmware)

  def GetAudioFiles(self):
    """Get a list of unique audio files for all models

    Returns:
      List of BaseFile objects representing all the audio files referenced
      by all models
    """
    file_set = set()
    for model in self.models.values():
      for files in model.GetAudioFiles().values():
        file_set.add(files)

    return sorted(file_set, key=lambda files: files.source)

  def GetFirmwareBuildTargets(self, target_type):
    """Returns a list of all firmware build-targets of the given target type.

    Args:
      target_type: A string type for the build-targets to return

    Returns:
      A list of all build-targets of the given type, for all models.
    """
    build_targets = [model.PathProperty('/firmware/build-targets', target_type)
                     for model in self.models.values()]
    if target_type == 'ec':
      build_targets += [model.PathProperty('/firmware/build-targets', 'cr50')
                        for model in self.models.values()]
    # De-duplicate
    build_targets_dedup = {target.value if target else None
                           for target in build_targets if target}
    return list(build_targets_dedup)

  def GetThermalFiles(self):
    """Get a list of unique thermal files for all models

    Returns:
      List of BaseFile objects representing all the audio files referenced
      by all models
    """
    file_set = set()
    for model in self.models.values():
      for files in model.GetThermalFiles().values():
        file_set.add(files)

    return sorted(file_set, key=lambda files: files.source)

  def ShowTree(self, base_path, tree):
    print('%-10s%s' % ('Size', 'Path'))
    tree.ShowTree(base_path)

  def GetFileTree(self):
    """Get a tree of all files installed by the config

    This looks at all available config that installs files in the root and
    returns them as a tree structure. This can be passed to ShowTree(), which
    is the only feature currently implemented which uses this tree.

    Returns:
        PathComponent object containin the root component
    """
    paths = set()
    for item in self.GetAudioFiles():
      paths.add(item.dest)
    for item in self.GetTouchFirmwareFiles():
      # TODO(sjg@chromium.org): Move these constant paths into cros_config so
      # that ebuilds do not need to specify them. crbug.com/769575
      paths.add(os.path.join('/opt/google/touch/firmware', item.firmware))
      paths.add(os.path.join(LIB_FIRMWARE, item.symlink))
    root = PathComponent('')
    for path in paths:
      root.AddPath(path[1:])

    return root

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

  def GetModelList(self):
    """Return a list of models

    Returns:
      List of model names, each a string
    """
    return sorted(self.models.keys())

  def GetFirmwareScript(self):
    """Obtain the packer script to use for this family

    Returns:
      Filename of packer script to use (e.g. 'updater4.sh')
    """
    return self.GetFamilyProperty('/firmware', 'script').value

  def GetFirmwareInfo(self):
    firmware_info = OrderedDict()
    for name in self.GetModelList():
      firmware_info[name] = self.models[name].GetFirmwareInfo()
    return firmware_info

  class Node(object):
    """Represents a single node in the CrosConfig tree, including Model.

    A node can have several subnodes nodes, as well as several properties. Both
    can be accessed via .subnodes and .properties respectively. A few helpers
    are also provided to make node traversal a little easier.

    Properties:
      name: The name of the this node.
      subnodes: Child nodes, in the form of a dictionary:
                <node name: string, child node: CrosConfig.Node>
      properties: All properties attached to this node in the form of a
                  dictionary: <name: string, property: CrosConfig.Property>
    """
    def __init__(self, cros_config, fdt_node):
      self.cros_config = cros_config
      self._fdt_node = fdt_node
      self.name = fdt_node.name
      # Subnodes are set up in Model.ScanSubnodes()
      self.subnodes = OrderedDict()
      self.properties = OrderedDict((n, CrosConfig.Property(p))
                                    for n, p in fdt_node.props.iteritems())
      self.default = None

    def ScanSubnodes(self):
      """Do any post-processing needed after the node's subnodes are present"""
      pass

    def FollowShare(self):
      """Follow a node's shares property

      Some nodes support sharing the properties of another node, e.g. firmware
      and whitelabel. This follows that share if it can find it. We don't need
      to be too careful to ignore invalid properties (e.g. whitelabel can only
      be in a model node) since validation takes care of that.

      Returns:
        Node that the share points to, or None if none
      """
      share_prop = [i for i in ['shares', 'whitelabel'] if i in self.properties]
      if share_prop:
        return self.FollowPhandle(share_prop[0])
      return None

    def PathNode(self, relative_path):
      """Returns the CrosConfig.Node at the relative path.

      This method is useful for accessing a nested child object at a relative
      path from a Node (or Model). The path must be separated with '/'
      delimiters. Return None if the path is invalid.

      Args:
        relative_path: A relative path string separated by '/', '/thermal'

      Returns:
        A CrosConfig.Node at the path, or None if it doesn't exist.
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
        CrosConfig.Property object that waws found, or None if none
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
      return prop.value

    def GetBool(self, property_name):
      """Get a boolean value from a property

      Args:
        property_name: Name of property to access

      Returns:
        True if the property is present, False if not
      """
      return property_name in self.properties

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

    @staticmethod
    def MergeProperties(props, node, ignore=''):
      if node:
        for name, prop in node.properties.iteritems():
          if (name not in props and not name.endswith('phandle') and
              name != ignore):
            props[name] = prop.value

    def GetMergedProperties(self, _, phandle_prop):
      """Obtain properties in two nodes linked by a phandle

      This is used to create a dict of the properties in a main node along with
      those found in a linked node. The link is provided by a property in the
      main node containing a single phandle pointing to the linked node.

      The result is a dict that combines both nodes' properties, with the
      linked node filling in anything missing. The main node's properties take
      precedence.

      Phandle properties and 'reg' properties are not included.

      Args:
        phandle_prop: Name of the phandle property to follow

      Returns:
        dict containing property names and values from both nodes:
          key: property name
          value: property value
      """
      # First get all the property keys/values from the current node
      props = OrderedDict((prop.name, prop.value)
                          for prop in self.properties.values()
                          if prop.name not in [phandle_prop, 'reg'])

      # Follow the phandle and add any new ones we find
      self.MergeProperties(props, self.FollowPhandle(phandle_prop))
      return props

  class Model(Node):
    """Represents a ChromeOS Configuration Model.

    A CrosConfig.Model is a subclass of CrosConfig.Node, meaning it can be
    traversed in the same manner. It also exposes helper functions
    specific to ChromeOS Config models.
    """
    def __init__(self, cros_config, fdt_node):
      super(CrosConfig.Model, self).__init__(cros_config, fdt_node)
      self.default = self.FollowPhandle('default')
      self.submodels = {}

    def ScanSubnodes(self):
      """Collect a list of submodels"""
      if 'submodels' in self.subnodes.keys():
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

    def GetMergedProperties(self, node, phandle_prop):
      props = node.GetMergedProperties(None, phandle_prop)
      if self.default:
        # Get the path of this node relative to its model. For example:
        # '/chromeos/models/pyro/thermal' will return '/thermal' in subpath.
        # Once crbug.com/775229 is completed, we will be able to do this in a
        # nicer way.
        _, _, _, _, subpath = node._fdt_node.path.split('/', 4)
        default_node = self.default.PathNode(subpath)
        self.MergeProperties(props, default_node, phandle_prop)
        self.MergeProperties(props, default_node.FollowPhandle(phandle_prop))
      return props

    def GetFirmwareUris(self):
      """Returns a list of (string) firmware URIs.

      Generates and returns a list of firmeware URIs for this model. These URIs
      can be used to pull down remote firmware packages.

      Returns:
        A list of (string) full firmware URIs, or an empty list on failure.
      """
      firmware = self.PathNode('/firmware')
      if not firmware:
        return []
      shared = firmware.FollowPhandle('shares')
      props = self.GetMergedProperties(firmware, 'shares')
      if shared:
        base_model = shared.name
      else:
        base_model = self.name

      if 'bcs-overlay' not in props:
        return []
      # Strip "overlay-" from bcs_overlay
      bcs_overlay = props['bcs-overlay'][8:]
      valid_images = [p for n, p in props.iteritems()
                      if n.endswith('-image') and p.startswith('bcs://')]
      # Strip "bcs://" from bcs_from images (to get the file names only)
      file_names = [p[6:] for p in valid_images]
      uri_format = ('gs://chromeos-binaries/HOME/bcs-{bcs}/overlay-{bcs}/'
                    'chromeos-base/chromeos-firmware-{base_model}/{fname}')
      return [uri_format.format(bcs=bcs_overlay, model=self.name, fname=fname,
                                base_model=base_model)
              for fname in file_names]

    def SetupModelProps(self, props):
      props['model'] = self.name
      props['MODEL'] = self.name.upper()

    def GetTouchFirmwareFiles(self):
      """Get a list of unique touch firmware files

      Returns:
        List of TouchFile objects representing the touch firmware referenced
        by this model
      """
      touch = self.PathNode('/touch')
      files = {}
      if touch:
        for device in touch.subnodes.values():
          props = self.GetMergedProperties(device, 'touch-type')

          # Add a special property for the capitalised model name
          self.SetupModelProps(props)
          files[device.name] = TouchFile(
              GetPropFilename(self._fdt_node.path, props, 'firmware-bin'),
              GetPropFilename(self._fdt_node.path, props, 'firmware-symlink'))
      return files

    def AllPathNodes(self, relative_path):
      """List all path nodes which match the relative path (including submodels)

      This looks in the model and all its submodels for this relative path.

      Args:
        relative_path: A relative path string separated by '/', '/thermal'

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
      return path_nodes

    def GetAudioFiles(self):
      """Get a list of audio files

      Returns:
        Dict of BaseFile objects representing the audio files referenced
        by this model:
          key: (model, property)
          value: BaseFile object
      """
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
            raise ValueError(("node '%s': Property '%s' does not have a " +
                              "target directory (internal error)") %
                             (card.name, prop_name))
          files[name, prop_name] = BaseFile(
              GetPropFilename(self._fdt_node.path, props, prop_name),
              os.path.join(
                  target_dir,
                  dirname,
                  GetFilename(self._fdt_node.path, props, dest_template)))

      files = {}
      audio_nodes = self.AllPathNodes('/audio')
      for name, audio in audio_nodes.iteritems():
        for card in audio.subnodes.values():
          # First get all the property keys/values from the current node
          props = self.GetMergedProperties(card, 'audio-type')
          self.SetupModelProps(props)

          cras_dir = props.get('cras-config-dir')
          if not cras_dir:
            raise ValueError(("node '%s': Should have a cras-config-dir") %
                             (card._fdt_node.path))
          _AddAudioFile('volume', '${card}', cras_dir)
          _AddAudioFile('dsp-ini', 'dsp.ini', cras_dir)

          # Allow renaming this file to something other than HiFi.conf
          _AddAudioFile('hifi-conf',
                        os.path.join('${card}.${ucm-suffix}',
                                     os.path.basename(props['hifi-conf'])))
          _AddAudioFile('alsa-conf',
                        '${card}.${ucm-suffix}/${card}.${ucm-suffix}.conf')

          # Non-Intel platforms don't use topology-bin
          topology = props.get('topology-bin')
          if topology:
            _AddAudioFile('topology-bin',
                          os.path.basename(props.get('topology-bin')))

      return files

    def GetThermalFiles(self):
      """Get a dict of thermal files

      Returns:
        Dict of BaseFile objects representing the thermal files referenced
        by this model:
          key: property
          value: BaseFile object
      """
      files = {}
      prop = 'dptf-dv'
      thermal = self.PathNode('/thermal')
      target_dir = self.cros_config.validator.GetModelTargetDir('/thermal',
                                                                prop)
      if thermal:
        files['base'] = BaseFile(
            thermal.properties[prop].value,
            os.path.join(target_dir, thermal.properties[prop].value))
      return files

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

      if firmware_node.GetBool('sig-id-in-customization-id'):
        sig_id = 'sig-id-in-customization-id'
      else:
        sig_id = self.name

      return FirmwareInfo(self.name, shared_model, key_id, have_image,
                          bios_build_target, ec_build_target,
                          main_image_uri, main_rw_image_uri, ec_image_uri,
                          pd_image_uri, extra, create_bios_rw_image, tools,
                          sig_id)

  class Property(object):
    """Represents a single property in a ChromeOS Configuration.

    Properties:
      name: The name of the property.
      value: The value of the property.
      type: The FDT type of the property (for now).
    """
    def __init__(self, fdt_prop):
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
