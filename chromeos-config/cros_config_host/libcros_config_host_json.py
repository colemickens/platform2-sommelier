# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Crome OS Configuration access library.

Provides build-time access to the master configuration on the host. It is used
for reading from the master configuration. Consider using cros_config_host.py
for CLI access to this library.
"""

from __future__ import print_function

from collections import OrderedDict
import json

from cros_config_schema import TransformConfig
from libcros_config_host_base import BaseFile, CrosConfigBaseImpl, DeviceConfig
from libcros_config_host_base import FirmwareInfo, TouchFile

UNIBOARD_JSON_INSTALL_PATH = 'usr/share/chromeos-config/config.json'


class DeviceConfigJson(DeviceConfig):
  """JSON specific impl of DeviceConfig

  Properties:
    _config: Root dictionary element for a given config.
  """

  def __init__(self, config):
    self._config = config
    self.firmware_info = OrderedDict()

  def GetName(self):
    return str(self._config['name'])

  def GetProperties(self, path):
    result = self._config
    if path != '/':
      for path_token in path[1:].split('/'):  # Burn the first '/' char
        if path_token in result:
          result = result[path_token]
        else:
          return {}
    return result

  def GetProperty(self, path, name):
    props = self.GetProperties(path)
    if props and name in props:
      return str(props[name])
    return ''

  def GetValue(self, source, name):
    if name in source:
      val = source[name]
      if isinstance(val, basestring):
        return str(val)
      return source[name]
    return None

  def _GetFiles(self, path):
    result = []
    file_region = self.GetProperties(path)
    if file_region and 'files' in file_region:
      for item in file_region['files']:
        result.append(BaseFile(item['source'], item['destination']))
    return result

  def GetFirmwareConfig(self):
    firmware = self.GetProperties('/firmware')
    if not firmware or self.GetValue(firmware, 'no-firmware'):
      return {}
    return firmware

  def GetFirmwareInfo(self):
    return self.firmware_info

  def GetTouchFirmwareFiles(self):
    result = []
    touch = self.GetProperties('/touch')
    if touch and 'files' in touch:
      for item in touch['files']:
        result.append(
            TouchFile(item['source'], item['destination'], item['symlink']))

    return result

  def GetArcFiles(self):
    return self._GetFiles('/arc')

  def GetAudioFiles(self):
    return self._GetFiles('/audio/main')

  def GetThermalFiles(self):
    return self._GetFiles('/thermal')

  def GetWallpaperFiles(self):
    result = set()
    wallpaper = self.GetValue(self._config, 'wallpaper')
    if wallpaper:
      result.add(wallpaper)
    return result


class CrosConfigJson(CrosConfigBaseImpl):
  """JSON specific impl of CrosConfig

  Properties:
    _json: Root json for the entire config.
    _configs: List of DeviceConfigJson instances.
  """

  def __init__(self, infile):
    self._json = json.loads(TransformConfig(infile.read()))
    self._configs = []
    for config in self._json['chromeos']['models']:
      self._configs.append(DeviceConfigJson(config))

    sorted(self._configs, key=lambda x: str(x.GetProperties('/identity')))

    # TODO(shapiroc): This is mess and needs considerable rework on the fw
    # side to cleanup, but for now, we're sticking with it in order to
    # finish migration to YAML.
    fw_by_model = {}
    processed = set()
    for config in self._configs:
      fw = config.GetFirmwareConfig()
      identity = str(config.GetProperties('/identity'))
      if fw and identity not in processed:
        fw_str = str(fw)
        shared_model = None
        if fw_str in fw_by_model:
          shared_model = fw_by_model[fw_str]
        else:
          fw_by_model[fw_str] = config.GetName()

        build_config = config.GetProperties('/firmware/build-targets')
        if build_config:
          bios_build_target = config.GetValue(build_config, 'coreboot')
          ec_build_target = config.GetValue(build_config, 'ec')
        else:
          bios_build_target, ec_build_target = None, None
        create_bios_rw_image = False

        main_image_uri = config.GetValue(fw, 'main-image')
        main_rw_image_uri = config.GetValue(fw, 'main-rw-image')
        ec_image_uri = config.GetValue(fw, 'ec-image')
        pd_image_uri = config.GetValue(fw, 'pd-image') or ''
        extra = config.GetValue(fw, 'extra') or []
        tools = config.GetValue(fw, 'tools') or []

        fw_signer_config = config.GetProperties('/firmware-signing')
        key_id = config.GetValue(fw_signer_config, 'key-id')
        sig_in_customization_id = config.GetValue(fw_signer_config,
                                                  'sig-id-in-customization-id')

        have_image = True
        name = config.GetName()

        if sig_in_customization_id:
          key_id = ''
          sig_id = 'sig-id-in-customization-id'
        else:
          sig_id = name
          processed.add(identity)

        info = FirmwareInfo(
            name,
            shared_model,
            key_id,
            have_image,
            bios_build_target,
            ec_build_target,
            main_image_uri,
            main_rw_image_uri,
            ec_image_uri,
            pd_image_uri,
            extra,
            create_bios_rw_image,
            tools,
            sig_id)
        config.firmware_info[name] = info

        if sig_in_customization_id:
          for config in self._configs:
            if config.GetName() == name:
              identity = str(config.GetProperties('/identity'))
              processed.add(identity)
              fw_signer_config = config.GetProperties('/firmware-signing')
              key_id = config.GetValue(fw_signer_config, 'key-id')
              whitelabel_name = '%s-%s' % (name, config.GetProperty(
                  '/identity', 'customization-id'))
              config.firmware_info[whitelabel_name] = info._replace(
                  model=whitelabel_name,
                  key_id=key_id,
                  have_image=False,
                  sig_id=whitelabel_name)

  def GetDeviceConfigs(self):
    return self._configs
