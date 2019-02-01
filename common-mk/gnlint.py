#!/usr/bin/env python2
# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Linter for checking GN files used in platform2 projects."""

# Most apart of this linter utilize the token tree parser of the gn binary.
# For example,
#
# executable("my_target") {
#   sources = [ "foo.cc", "bar.cc" ]
# }
#
# is parsed to a token tree by gn-format subcommand, like this:
#
# BLOCK
#  FUNCTION(executable)
#   LIST
#    LITERAL("my_target")
#   BLOCK
#    BINARY(=)
#     IDENTIFIER(sources)
#     LIST
#      LITERAL("foo.cc")
#      LITERAL("bar.cc")
#
# The above example is expressed as a JSON like this and imported into dict:
# {
#    "type": "BLOCK",
#    "child": [ {
#       "type": "FUNCTION",
#       "value": "executable"
#       "child": [ {
#          "type": "LIST",
#          "child": [ {
#             "type": "LITERAL"],
#             "value": "\"my_target\""
#          } ]
#       }, {
#          "type": "BLOCK",
#          "child": [ {
#             "type": "BINARY",
#             "value": "=",
#             "child": [ {
#                "type": "IDENTIFIER",
#                "value": "sources"
#             }, {
#                "type": "LIST",
#                "child": [ {
#                   "type": "LITERAL",
#                   "value": "\"foo.cc\""
#                }, {
#                   "type": "LITERAL",
#                   "value": "\"bar.cc\""
#                } ]
#             } ]
#          } ]
#       } ]
#    } ]
# }
# The tree structure is expressed by "child" key having list of nodes in it.
# Every dict in the nested structure represents a single node.

from __future__ import print_function

import collections
import json
import os
import re
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
  issues = []
  linter = 'CheckFormat'
  try:
    gn_path = os.path.join(chromite_root, 'chroot', 'usr', 'bin', 'gn')
    result = cros_build_lib.RunCommand([gn_path, "format", "--dry-run", gnfile],
                                       error_code_ok=True)
  except cros_build_lib.RunCommandError as e:
    issues.append(LintResult(
        linter, gnfile, "Failed to run gn format: %s" % e, logging.ERROR))
  else:
    if result.returncode == 0:
      # successful format, matches on disk.
      pass
    elif result.returncode == 1:
      issues.append(LintResult(
          linter, gnfile, "General failure while running gn format " \
          "(e.g. parse error)", logging.ERROR))
    elif result.returncode == 2:
      issues.append(LintResult(
          linter, gnfile,
          "Needs reformatting. Run following command: %s format %s" %
          (gn_path, gnfile), logging.ERROR))
    else:
      issues.append(LintResult(
          linter, gnfile, "Unknown error with gn format: " \
          "returncode=%i error=%s output=%s" %
          (result.returncode, result.error, result.output), logging.ERROR))
  return issues


def WalkGn(functor, node):
  """Walk the token tree under |node|, calling |functor| on each node.

  Args:
    functor: A function to be applied.
    node: A dict representing a token subtree containing the target nodes.
  """
  if not isinstance(node, dict):
    logging.warning('Reached non-dict node. Skipping: %s', node)
    return
  functor(node)
  for n in node.get('child', []):
    WalkGn(functor, n)


def ExtractLiteralAssignment(node, target_variable_names):
  """Returns list of literals assigned, added or removed to either variable.

  If |node| assigns, adds or removes string literal values by a list to either
  of the target variable, returns list of all strings literals. Otherwise
  returns an empty list.

  Args:
    node: A dict representing a token subtree.
    target_variable_names: List of strings representing variable names to be
        detected for its modification.

  Returns:
    List of strings used with the assignment operators to either variable.
  """
  ASSIGNMENT = ['=', '+=', '-=']
  if node.get('type') != 'BINARY' or node.get('value') not in ASSIGNMENT:
    return []
  # Detected pattern is like:
  #    BINARY(=)
  #     IDENTIFIER(ldflags)
  #     LIST
  #      LITERAL("-l")
  child = node.get('child')
  # BINARY assignment node should have LHS and RHS.
  if not isinstance(child, list) or len(child) != 2:
    logging.warning('Unexpected tree structure. Skipping: %s', node)
    return []
  if child[0].get('value') not in target_variable_names:
    return []
  if child[1].get('type') != 'LIST':
    return []
  literals = []
  for element in child[1].get('child'):
    if element.get('type') != 'LITERAL':
      continue
    # Literal nodes of a string value contains double quotes.
    literals.append(element.get('value').strip('"'))
  return literals


def GnLintLibFlags(gndata):
  """-lfoo flags belong in 'libs' and not 'ldflags'.

  Args:
    gndata: A dict representing a token tree.

  Returns:
    List of detected LintResult.
  """

  def CheckNode(node):
    flags = ExtractLiteralAssignment(node, ['ldflags'])
    for flag in flags:
      if flag.startswith('-l'):
        issues.append(('Libraries should be specified by "libs", '
                       'not -l flags in "ldflags": %s') % flag)

  issues = []
  WalkGn(CheckNode, gndata)
  return issues


def GnLintVisibilityFlags(gndata):
  """Packages should not change -fvisibility settings.

  Args:
    gndata: A dict representing a token tree.

  Returns:
    List of detected LintResult.
  """

  def CheckNode(node):
    flags = ExtractLiteralAssignment(node, ['cflags', 'cflags_c', 'cflags_cc'])
    for flag in flags:
      if flag.startswith('-fvisibility'):
        issues.append('do not use -fvisibility; to export symbols, use '
                      'brillo/brillo_export.h instead')

  issues = []
  WalkGn(CheckNode, gndata)
  return issues


def GnLintDefineFlags(gndata):
  """-D flags should be in 'defines', not cflags.

  Args:
    gndata: A dict representing a token tree.

  Returns:
    List of detected LintResult.
  """

  def CheckNode(node):
    flags = ExtractLiteralAssignment(node, ['cflags', 'cflags_c', 'cflags_cc'])
    for flag in flags:
      if flag.startswith('-D'):
        issues.append('-D flags should be in "defines": %s' % flag)

  issues = []
  WalkGn(CheckNode, gndata)
  return issues


def GnLintCommonTesting(gndata):
  """Packages should use common_test.gypi instead of -lgtest/-lgmock.

  Args:
    gndata: A dict representing a token tree.

  Returns:
    List of detected LintResult.
  """

  def CheckNode(node):
    flags = ExtractLiteralAssignment(node, ['libs'])
    if 'gtest' in flags or 'gmock' in flags:
      issues.append('use common-mk/common_test.gypi for tests instead of '
                    'linking against -lgtest/-lgmock directly')

  issues = []
  WalkGn(CheckNode, gndata)
  return issues


# The regex used to find gnlint options in the file.
# This matches the regex pylint uses.
OPTIONS_RE = re.compile(r'^\s*#.*\bgnlint:\s*([^\n;]+)', flags=re.MULTILINE)

# Object holding linter settings.
LintSettings = collections.namedtuple('LintSettings', (
    # Linters to skip.
    'skip',
    # Problems we found in the lint settings themselves.
    'issues',
))


def ParseOptions(options, name=None):
  """Parse out the linter settings from |options|.

  Currently we support:
    disable=<linter name>

  Args:
    options: A list of linter options (e.g. ['foo=bar']).
    name: The file we're parsing.

  Returns:
    A LintSettings object.
  """
  skip = set()
  issues = []

  # Parse all the gnlint directives.
  for option in options:
    key, value = option.split('=', 1)
    key = key.strip()
    value = value.strip()

    # Parse each sub-option.
    if key == 'disable':
      skip.update(x.strip() for x in value.split(','))
    else:
      issues.append(
          LintResult('ParseOptions', name,
                     'unknown gnlint option: %s' % (key,), logging.ERROR))

  # Validate the options.
  all_linters = FindLinters([])
  bad_linters = skip - all_linters
  if bad_linters:
    issues.append(
        LintResult('ParseOptions', name, 'unknown linters: %s' % (bad_linters,),
                   logging.ERROR))

  return LintSettings(skip, issues)


_ALL_LINTERS = {
    'GnLintLibFlags': GnLintLibFlags,
    'GnLintVisibilityFlags': GnLintVisibilityFlags,
    'GnLintDefineFlags': GnLintDefineFlags,
    'GnLintCommonTesting': GnLintCommonTesting,
}


def FindLinters(skip):
  """Return all linters excluding ones in |skip|.

  Args:
    skip: A string list of linter names to be skipped.

  Returns:
    List of linter functions.
  """
  return set(f for name, f in _ALL_LINTERS.items() if name not in skip)


def RunLinters(name, gndata, settings=None):
  """Run linters agains |gndata|.

  Args:
    name: A string representing the filename. For printing issues.
    gndata: A dict representing a token tree. See the comment in "Linters and
        its helper functions" section for details.
    settings: An optional LintSettings object.

  Returns:
    List of detected LintResult.
  """
  issues = []

  if settings is None:
    settings = ParseOptions([])
    issues += settings.issues

  for linter in FindLinters(settings.skip):
    for result in linter(gndata):
      issues.append(LintResult(linter, name, result, logging.ERROR))
  return issues


def CheckGnFile(gnfile):
  """Check |gnfile| for common mistakes.

  Args:
    gnfile: A string representing the filename of the GN file.

  Returns:
    List of detected LintResult.
  """
  if not os.path.exists(gnfile):
    # The file has been deleted.
    return
  issues = []
  issues += CheckFormat(gnfile)
  # Parse the gnlint flags in the file.
  with open(gnfile) as fp:
    data = fp.read()
  settings = ParseOptions(OPTIONS_RE.findall(data))
  issues += settings.issues

  # Parse and check
  gn_path = os.path.join(chromite_root, 'chroot', 'usr', 'bin', 'gn')
  try:
    command_result = cros_build_lib.RunCommand(
        [gn_path, 'format', '--dump-tree=json', gnfile],
        combine_stdout_stderr=True,
        redirect_stdout=True)
  except cros_build_lib.RunCommandError as e:
    issues.append(LintResult('gn.input.CheckedEval', gnfile,
                             'Failed to run gn format: %s' % e, logging.ERROR))
    return issues

  try:
    data = json.loads(command_result.output)
  except Exception as e:
    issues.append(LintResult('gn.input.CheckedEval', gnfile,
                             'invalid format: %s' % e, logging.ERROR))
    return issues
  issues += RunLinters(gnfile, data, settings)
  return issues


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
