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
import sys

from cros_config_schema import TransformConfig
from libcros_config_host_base import BaseFile, CrosConfigBaseImpl, DeviceConfig, FirmwareInfo, TouchFile

UNIBOARD_JSON_INSTALL_PATH = 'usr/share/chromeos-config/config.json'


class DeviceConfigJson(DeviceConfig):
  """JSON specific impl of DeviceConfig

  Properties:
    _config: Root dictionary element for a given config.
  """

  def __init__(self, config):
    self._config = config
    self._firmware_info = None

  def GetName(self):
    return self._config['name']

  def GetProperties(self, path):
    result = self._config
    for path_token in path[1:]:  # Burn the first '/' char
      if path_token in result:
        result = result[path_token]
      else:
        return {}
    return result

  def _GetValue(self, source, name):
    if name in source:
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
    if not firmware or self._GetValue(firmware, 'no-firmware'):
      return {}
    return firmware

  def GetFirmwareInfo(self):
    return self._firmware_info

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
    return self._GetFiles('/audio')

  def GetThermalFiles(self):
    return self._GetFiles('/thermal')

  def GetWallpaperFiles(self):
    result = set()
    print(str(self._config), file=sys.stderr)
    wallpaper = self._GetValue(self._config, 'wallpaper')
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
    names = set()
    for config in self._configs:
      fw = config.GetFirmwareConfig()
      if fw:
        fw_str = str(fw)
        shared_model = None
        if fw_str in fw_by_model:
          shared_model = fw_by_model[fw_str]
        else:
          fw_by_model[fw_str] = config.GetName()

        fw_signer_config = config.GetPropeties('/firmware-signing')
        key_id = config._GetValue(fw_signer_config, 'key-id')
        sig_in_customization_id = config._GetValue(
            fw_signer_config, 'sig-id-in-customization-id')
        have_image = True
        name = config.GetName()
        if sig_in_customization_id:
          have_image = False
          sig_id = name
        else:
          sig_id = 'sig-id-in-customization-id'
          name = "%s-%s" % (name, name)
          index = 0
          while name in names:
            name = "%s%s" % (name, str(index))
          names.add(name)


        build_config = config.GetProperties('/firmware/build-targets')
        if build_config:
          bios_build_target = build_config['coreboot']
          ec_build_target = build_config['ec']
        else:
          bios_build_target, ec_build_target = None, None
        create_bios_rw_image = False

        main_image_uri = config._GetValue(fw, 'main-image')
        main_rw_image_uri = config._GetValue(fw, 'main-rw-image')
        ec_image_uri = config._GetValue(fw, 'ec-image')
        pd_image_uri = config._GetValue(fw, 'pd-image')
        extra = config._GetValue(fw, 'extra')
        tools = config._GetValue(fw, 'tools')

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
        config._firmware_info = {name: info}

  def GetDeviceConfigs(self):
    return self._configs
