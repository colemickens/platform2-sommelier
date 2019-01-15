#!/usr/bin/env python2
# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Linter for checking GN files used in platform2 projects."""

from __future__ import print_function

import collections
import os
import sys

# Find chromite!
chromite_root = os.path.join(os.path.dirname(os.path.realpath(__file__)),
                             '..', '..', '..')
sys.path.insert(0, chromite_root)

from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging


# Object holding the result of a lint check.
LintResult = collections.namedtuple('LintResult', (
    # The name of the linter checking.
    'linter',
    # The file the issue was found in.
    'file',
    # The message for this check.
    'msg',
    # The type of result -- logging.ERROR or logging.WARNING.
    'type',
))


def FilterFiles(files, extensions):
  """Filter out |files| based on |extensions|."""
  for f in files:
    # Chop of the leading period as we'll get back ".bin".
    extension = os.path.splitext(f)[1][1:]
    if extension not in extensions or os.path.basename(f).startswith('.'):
      logging.debug('Skipping %s', f)
      continue
    yield f


def FilterPaths(paths, extensions):
  """Walk |paths| recursively and filter out content in it."""
  for path in paths:
    if os.path.isdir(path):
      for root, _, files in os.walk(path):
        for gnfile in FilterFiles(files, extensions):
          yield os.path.join(root, gnfile)
    else:
      for gnfile in FilterFiles([path], extensions):
        yield gnfile


# Object holding linter settings.
LintSettings = collections.namedtuple('LinterSettings', (
    # Linters to skip.
    'skip',
    # Problems we found in the lint settings themselves.
    'errors',
))


def GetParser():
  """Return an argument parser."""
  parser = commandline.ArgumentParser(description=__doc__)
  parser.add_argument('--extensions', default='gn,gni',
                      help='Comma delimited file extensions to check. '
                           '(default: %(default)s)')
  parser.add_argument('files', nargs='*',
                      help='Files to run lint.')
  return parser


def CheckFormat(gnfile):
  """Check if the .gn file is formatted in the standard way by gn format."""
  ret = []
  linter = 'CheckFormat'
  try:
    gn_path = os.path.join(chromite_root, 'chroot', 'usr', 'bin', 'gn')
    result = cros_build_lib.RunCommand([gn_path, "format", "--dry-run", gnfile],
                                       error_code_ok=True)
  except cros_build_lib.RunCommandError as e:
    ret.append(LintResult(linter, gnfile,
                          "Failed to run gn format: %s" % e, logging.ERROR))
  else:
    if result.returncode == 0:
      # successful format, matches on disk.
      pass
    elif result.returncode == 1:
      ret.append(LintResult(
          linter, gnfile, "General failure while running gn format " \
          "(e.g. parse error)", logging.ERROR))
    elif result.returncode == 2:
      ret.append(LintResult(
          linter, gnfile,
          "Needs reformatting. Run following command: %s format %s" %
          (gn_path, gnfile), logging.ERROR))
    else:
      ret.append(LintResult(
          linter, gnfile, "Unknown error with gn format: " \
          "returncode=%i error=%s output=%s" %
          (result.returncode, result.error, result.output), logging.ERROR))
  return ret


def CheckGnFile(gnfile):
  """Check |gnfile| for common mistakes."""
  if not os.path.exists(gnfile):
    # The file has been deleted.
    return
  return CheckFormat(gnfile)


def main(argv):
  parser = GetParser()
  opts = parser.parse_args(argv)

  if not opts.files:
    logging.warning('No files provided to lint.  Doing nothing.')
    return 0

  extensions = set(opts.extensions.split(','))
  num_files = 0
  for gnfile in FilterPaths(opts.files, extensions):
    logging.debug('Checking %s', gnfile)
    issues = CheckGnFile(gnfile)
    if issues:
      logging.error('**** %s: found %i issue(s)', gnfile, len(issues))
      for issue in issues:
        logging.log(issue.type, '%s: %s', issue.linter, issue.msg)
      num_files += 1
  if num_files:
    logging.error('%i file(s) failed linting', num_files)
  return 1 if num_files else 0


if __name__ == '__main__':
  commandline.ScriptWrapperMain(lambda _: main)
