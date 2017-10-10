#!/usr/bin/env python2
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A command-line utility to access the Chrome OS master configuration.

Cros config is broken into two tools, cros_config and cros_config_host. This is
the latter; it is the build side version of cros_config. It is used by the build
system to access configuration details for for a Chrome OS device.
"""

from __future__ import print_function

import argparse
import sys

from libcros_config_host import CrosConfig


def ListModels(config):
  """Prints all models in a config to stdout, one per line.

  Args:
    config: A CrosConfig instance
  """
  for model_name in config.models.keys():
    print(model_name)

def GetProperty(models, path, prop):
  """Prints a property from the config tree for all models in the list models.

  Args:
    models: List of CrosConfig.Model for which to print the given property.
    path: The path (relative to a model) for the node containing the property.
    prop: The property to get (by name).
  """
  for model in models or []:
    config_prop = model.ChildPropertyFromPath(path, prop)
    print(config_prop.value if config_prop else '')


def GetFirmwareUris(models):
  """Prints space-separated firmware uris for all models in models.

  Args:
    models: List of CrosConfig.Model for which to print firmware URIs
  """
  for model in models or []:
    print(' '.join(model.GetFirmwareUris()))

def GetTouchFirmwareFiles(config):
  """Print a list of touch firmware files across all models

  The output is one line for the firmware file and one line for the symlink,
  e.g.:
     wacom/4209.hex
     wacom_firmware_reef.bin

  Args:
    config: A CrosConfig instance
  """
  for files in config.GetTouchFirmwareFiles():
    print(files.firmware)
    print(files.symlink)

def GetAudioFiles(config):
  """Print a list of audio files across all models

  The output is one line for the source file and one line for the install file,
  e.g.:
     ucm-config/bxtda7219max.reef.BASKING/bxtda7219max.reef.BASKING.conf
     /usr/share/alsa/ucm/bxtda7219max.basking/bxtda7219max.basking.conf

  Args:
    config: A CrosConfig instance
  """
  for files in config.GetAudioFiles():
    print(files.source)
    print(files.dest)

def GetParser(description):
  """Returns an ArgumentParser structured for the cros_config_host CLI.

  Args:
    description: A description of the entire script, and it's purpose in life.

  Returns:
    An ArgumentParser structured for the cros_config_host CLI.
  """
  parser = argparse.ArgumentParser(description)
  parser.add_argument('filepath',
                      help='The master config file path. Use - for stdin.')
  parser.add_argument('-m', '--model', type=str,
                      help='Which model to run the subcommand on.')
  parser.add_argument('-a', '--all-models', action='store_true',
                      help='Runs the subcommand on all models, one per line.')
  subparsers = parser.add_subparsers(dest='subcommand')
  # Parser: list-models
  subparsers.add_parser(
      'list-models',
      help='Lists all models in the Cros Configuration Database at <filepath>',
      epilog='Each model will be printed on its own line.')
  # Parser: get
  get_parser = subparsers.add_parser(
      'get',
      help='Gets a model property at the given path, with the given name.')
  get_parser.add_argument(
      'path',
      help='Relative path (within the model) to the property\'s parent node')
  get_parser.add_argument(
      'prop',
      help='The name of the property to get within the node at <path>.')
  # Parser: get-touch-firmware-files
  subparsers.add_parser(
      'get-touch-firmware-files',
      help='Lists groups of touch firmware files in sequence: first line is ' +
      'firmware file, second line is symlink name for /lib/firmware')
  subparsers.add_parser(
      'get-firmware-uris',
      help='Lists firmware uris for models. These uris can be used to fetch ' +
      'firmware files.')
  # Parser: get-audio-files
  subparsers.add_parser(
      'get-audio-files',
      help='Lists pairs of audio files in sequence: first line is ' +
      'the source file, second line is the full install pathname')
  return parser


def main(argv):
  """Chrome OS Configuration for Host

  This Python script is used on the host (primary purpose is being called from
  the shell during building). It is broken into a sub-command tree that allows
  for traversal of models and access to their properties within.
  """
  parser = GetParser(__doc__)
  # Parse argv
  opts = parser.parse_args(argv)
  config = None
  if opts.filepath == '-':
    config = CrosConfig(sys.stdin)
  else:
    with open(opts.filepath) as infile:
      config = CrosConfig(infile)
  # Get all models we are invoking on (if any).
  models = None
  if opts.model and opts.model in config.models:
    models = [config.models[opts.model]]
  elif opts.all_models:
    models = config.models.values()
  # Main command branch
  if opts.subcommand == 'list-models':
    ListModels(config)
  elif opts.subcommand == 'get':
    if not opts.model and not opts.all_models:
      print('You must specify --model or --all-models for this command. See '
            '--help for more info.')
      return
    GetProperty(models, opts.path, opts.prop)
  elif opts.subcommand == 'get-touch-firmware-files':
    GetTouchFirmwareFiles(config)
  elif opts.subcommand == 'get-firmware-uris':
    if not opts.model and not opts.all_models:
      print('You must specify --model or --all-models for this command. See '
            '--help for more info.')
      return
    GetFirmwareUris(models)
  elif opts.subcommand == 'get-audio-files':
    GetAudioFiles(config)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
