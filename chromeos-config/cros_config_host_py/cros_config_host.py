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
  model_names = [name for name, _ in config.models.iteritems()]
  for model_name in sorted(model_names):
    print(model_name)


def GetParser(description):
  """Returns an ArgumentParser structured for the cros_config_host CLI.

  Args:
    description: A description of the entire script, and it's purpose in life.

  Returns:
    An ArgumentParser structured for the cros_config_host CLI.
  """
  parser = argparse.ArgumentParser(description)
  parser.add_argument('filepath', help='The master configuration file path.')
  subparsers = parser.add_subparsers(dest='subcommand')
  # Parser: list-models
  subparsers.add_parser(
      'list-models',
      help='Lists all models in the Cros Configuration Database at <filepath>',
      epilog='Each model will be printed on it\'s own line.')
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
  config = CrosConfig(opts.filepath)
  if opts.subcommand == 'list-models':
    ListModels(config)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
