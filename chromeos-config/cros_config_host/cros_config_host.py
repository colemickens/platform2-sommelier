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

from .libcros_config_host import CrosConfig


def ListModels(config):
  """Prints all models in a config to stdout, one per line.

  Args:
    config: A CrosConfig instance
    whitelabels: True to include whitelabel devices in the list
  """
  for model_name in config.GetModelList():
    print(model_name)

def GetProperty(models, path, prop):
  """Prints a property from the config tree for all models in the list models.

  Args:
    models: List of CrosConfig.Model for which to print the given property.
    path: The path (relative to a model) for the node containing the property.
    prop: The property to get (by name).
  """
  for model in models or []:
    config_prop = model.PathProperty(path, prop)
    print(config_prop.value if config_prop else '')


def GetFirmwareUris(config):
  """Prints space-separated firmware uris for all models in models.

  Args:
    config: A CrosConfig instance
  """
  print(' '.join(config.GetFirmwareUris()))

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

def GetFirmwareBuildTargets(config, target_type):
  """Lists all firmware build-targets of the given type, for all models.

  Args:
    config: A CrosConfig instance to load data from.
    target_type: A string name for what target type to get build-targets for.
  """
  for target in config.GetFirmwareBuildTargets(target_type):
    print(target)

def GetThermalFiles(config):
  """Print a list of thermal files across all models

  The output is one line for the source file (typically relative to ${FILESDIR})
  and one line for the install file, e.g.:
     astronaut/dptf.dv
     /etc/dptf/astronaut/dptf.dv

  Args:
    config: A CrosConfig instance
  """
  for files in config.GetThermalFiles():
    print(files.source)
    print(files.dest)

def FileTree(config, root):
  """Print a tree showing all files installed for this config

  The output is a tree structure printed one directory/file per line. Each file
  is shown with its size, or missing it if is not present.

  Args:
    config: A CrosConfig instance
    root: Path to the root directory for the board (e.g. '/build/reef-uni')
  """
  tree = config.GetFileTree()
  config.ShowTree(tree, root)

def WriteTargetDirectories():
  """Writes out a file containing the directory target info"""
  target_dirs = CrosConfig.GetTargetDirectories()
  print('''/*
 * This is a generated file, DO NOT EDIT!'
 *
 * This provides a map from property name to target directory for all PropFile
 * objects defined in the schema.
 */

/ {
\tchromeos {
\t\tschema {
\t\t\ttarget-dirs {''')
  for name in sorted(target_dirs.keys()):
    print('\t\t\t\t%s = "%s";' % (name, target_dirs[name]))
  print('''\t\t\t};
\t\t};
\t};
};
''')

def GetParser(description):
  """Returns an ArgumentParser structured for the cros_config_host CLI.

  Args:
    description: A description of the entire script, and it's purpose in life.

  Returns:
    An ArgumentParser structured for the cros_config_host CLI.
  """
  parser = argparse.ArgumentParser(description)
  parser.add_argument('-c', '--config',
                      help='Override the master config file path. Use - for '
                           'stdin.')
  parser.add_argument('-m', '--model', type=str,
                      help='Which model to run the subcommand on.')
  parser.add_argument('-a', '--all-models', action='store_true',
                      help='Runs the subcommand on all models, one per line.')
  subparsers = parser.add_subparsers(dest='subcommand')
  # Parser: list-models
  subparsers.add_parser(
      'list-models',
      help='Lists all models in the Cros Configuration Database.',
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
  # Parser: get-firmware-build-targets
  build_target_parser = subparsers.add_parser(
      'get-firmware-build-targets',
      help='Lists firmware build-targets for the given type, for all models.',
      epilog='Each build-target will be printed on its own line.')
  build_target_parser.add_argument(
      'type',
      help='The build-targets type to get (ex. coreboot, ec, depthcharge)')
  # Parser: get-thermal-files
  subparsers.add_parser(
      'get-thermal-files',
      help='Lists pairs of thermal files in sequence: first line is ' +
      'the relative source file file, second line is the full install pathname')
  # Parser: file-tree
  file_tree_parser = subparsers.add_parser(
      'file-tree',
      help='Shows all files installed by the BSP in a tree structure')
  file_tree_parser.add_argument(
      'root',
      help='Part to the root directory for this board')
  # Parser: write-target-dirs
  file_tree_parser = subparsers.add_parser(
      'write-target-dirs',
      help='Writes out a list of target directories for each PropFile element')
  return parser


def main(argv=None):
  """Chrome OS Configuration for Host

  This Python script is used on the host (primary purpose is being called from
  the shell during building). It is broken into a sub-command tree that allows
  for traversal of models and access to their properties within.
  """
  parser = GetParser(__doc__)
  # Parse argv
  if argv is None:
    argv = sys.argv[1:]
  opts = parser.parse_args(argv)
  if opts.subcommand == 'write-target-dirs':
    WriteTargetDirectories()
    return
  if opts.config == '-':
    config = CrosConfig(sys.stdin)
  elif opts.config:
    with open(opts.config) as infile:
      config = CrosConfig(infile)
  else:
    config = CrosConfig()
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
    GetFirmwareUris(config)
  elif opts.subcommand == 'get-audio-files':
    GetAudioFiles(config)
  elif opts.subcommand == 'get-firmware-build-targets':
    GetFirmwareBuildTargets(config, opts.type)
  elif opts.subcommand == 'get-thermal-files':
    GetThermalFiles(config)
  elif opts.subcommand == 'file-tree':
    FileTree(config, opts.root)


if __name__ == '__main__':
  sys.exit(main())
