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
import json
import os
import sys

from .cros_config_schema import TransformConfig, GetNamedTuple
from ..libcros_config_host import BaseFile, FirmwareInfo, PathComponent, TouchFile, LIB_FIRMWARE

UNIBOARD_JSON_INSTALL_PATH = 'usr/share/chromeos-config/config.json'

class CrosConfigJson(object):
  """The ChromeOS Configuration API for the host.

  CrosConfig is the top level API for accessing ChromeOS Configs from the host.
  This is the JSON based implementation of this API.

  Properties:
    models: All models in the CrosConfig tree, in the form of a dictionary:
            <model name: string, model: CrosConfig.Model>
  """
  def __init__(self, infile=None):
    if not infile:
      if 'SYSROOT' not in os.environ:
        raise ValueError('No master configuration is available outside the '
                         'ebuild environemnt. You must specify one')
      fname = os.path.join(os.environ['SYSROOT'], UNIBOARD_JSON_INSTALL_PATH)
      with open(fname) as infile:
        self._json = json.loads(TransformConfig(infile.read()))
    else:
      self._json = json.loads(TransformConfig(infile.read()))

    self.models = {}
    for model in self._json['models']:
      self.models[model['name']] = CrosConfigJson.Model(model)

  def GetModelList(self):
    """Return a list of models

    Returns:
      List of model names, each a string
    """
    return sorted(self.models.keys())

  def GetFirmwareUris(self):
    """Returns a list of (string) firmware URIs.

    Generates and returns a list of firmeware URIs for all models. These URIs
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
      for file in model.GetTouchFirmwareFiles():
        file_set.add(file)

    return sorted(file_set, key=lambda file: file.firmware)

  def GetAudioFiles(self):
    """Get a list of unique audio files for all models

    Returns:
      List of BaseFile objects representing all the audio files referenced
      by all models
    """
    file_set = set()
    for model in self.models.values():
      for files in model.GetAudioFiles():
        file_set.add(files)

    return sorted(file_set, key=lambda files: files.source)

  class Model(object):
    """Represents a ChromeOS Configuration Model.

    Args:
      model: Dictionary of the model specific JSON configuration
    """
    def __init__(self, model):
      self._model = GetNamedTuple(model)
      self._model_dict = model
      self.name = self._model.name

    def GetFirmwareUris(self):
      """Returns a list of (string) firmware URIs.

      Generates and returns a list of firmware URIs for this model. These URIs
      can be used to pull down remote firmware packages.

      Returns:
        A list of (string) full firmware URIs, or an empty list on failure.
      """
      return self._model.firmware.bcs_uris

    def GetTouchFirmwareFiles(self):
      """Get a list of unique touch firmware files

      Returns:
        List of TouchFile objects representing the touch firmware referenced
        by this model
      """
      files = []
      if 'touch' in self._model_dict:
        touch = self._model_dict['touch']
        for key in touch:
          if 'firmware-bin' in touch[key]:
            files.append(TouchFile(
                touch[key]['firmware-bin'], touch[key]['firmware-symlink']))
      return files

    def GetAudioFiles(self):
      """Get a list of audio files

      Returns:
        List of audio files for the given model.
      """
      files = []
      for file in self._model.audio.main.files:
        files.append(BaseFile(file.source, file.dest))
      return files

