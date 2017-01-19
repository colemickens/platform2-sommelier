#!/usr/bin/env python2
# -*- coding: utf-8 -*-
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Linter for checking GYP files used in platform2 projects."""

from __future__ import print_function

import collections
import os
import re
import sys

import gyp_compiler

# Find chromite!
sys.path.insert(0, os.path.join(os.path.dirname(os.path.realpath(__file__)),
                                '..', '..', '..'))

from chromite.lib import commandline
from chromite.lib import cros_logging as logging


# Object holding the result of a lint check.
LintResult = collections.namedtuple('LintResult', (
    # The name of the linter checking.
    'linter',
    # The file the issue was found in.
    'file',
    # The message for this check.
    'msg',
    # The type of result -- error, warning, etc...
    'type',
))


def WalkGyp(functor, gypdata):
  """Walk |gypdata| and call |functor| on each node."""
  def WalkNode(node):
    ret = []
    if isinstance(node, dict):
      for k, v in node.items():
        ret += functor(k, v)
        ret += WalkNode(v)
    elif isinstance(node, (tuple, list)):
      for v in node:
        ret += WalkNode(v)
    return ret

  return WalkNode(gypdata)


def GypLintLibFlags(gypdata):
  """-lfoo flags belong in 'libraries' and not 'ldflags'."""
  def CheckNode(key, value):
    ret = []
    if key.startswith('ldflags'):
      for flag in value:
        if flag.startswith('-l'):
          ret.append('-l flags should be in "libraries", not "ldflags": %s' %
                     (flag,))
    return ret

  return WalkGyp(CheckNode, gypdata)


def GypLintVisibilityFlags(gypdata):
  """Packages should not change -fvisibility settings."""
  def CheckNode(key, value):
    ret = []
    if key.startswith('cflags'):
      for flag in value:
        if flag.startswith('-fvisibility'):
          ret.append('do not use -fvisibility; to export symbols, use '
                     'brillo/brillo_export.h instead')
    return ret

  return WalkGyp(CheckNode, gypdata)


def GypLintDefineFlags(gypdata):
  """-D flags should be in 'defines', not cflags."""
  def CheckNode(key, value):
    ret = []
    if key.startswith('cflags'):
      for flag in value:
        if flag.startswith('-D'):
          ret.append('-D flags should be in "defines", not "%s": %s' %
                     (key, flag))
    return ret

  return WalkGyp(CheckNode, gypdata)


def GypLintCommonTesting(gypdata):
  """Packages should use common_test.gypi instead of -lgtest/-lgmock."""
  def CheckNode(key, value):
    ret = []
    if key.startswith('ldflags') or key.startswith('libraries'):
      if '-lgtest' in value or '-lgmock' in value:
        ret.append('use common-mk/common_test.gypi for tests instead of '
                   'linking against -lgtest/-lgmock directly')
    return ret

  return WalkGyp(CheckNode, gypdata)


# It's not easy to auto-discover pkg-config files as we don't require a chroot
# or a fully installed sysroot to run this linter.  Plus, there's no clean way
# to correlate -lfoo names with pkg-config .pc file names.  List the packages
# that we tend to use in platform2 projects.
KNOWN_PC_FILES = {
    '-lblkid': 'blkid',
    '-lcap': 'libcap',
    '-lcrypto': 'libcrypto',
    '-ldbus-1': 'dbus-1',
    '-ldbus-c++-1': 'dbus-c++-1',
    '-ldbus-glib-1': 'dbus-glib-1',
    '-lexpat': 'expat',
    '-lfuse': 'fuse',
    '-lglib-2.0': 'glib-2.0',
    '-lgobject-2.0': 'gobject-2.0',
    '-lgthread-2.0': 'gthread-2.0',
    '-lminijail': 'libminijail',
    '-lpcre': 'libpcre',
    '-lpcrecpp': 'libpcrecpp',
    '-lpcreposix': 'libpcreposix',
    '-lprotobuf': 'protobuf',
    '-lprotobuf-lite': 'protobuf-lite',
    '-lssl': 'libssl',
    '-ludev': 'libudev',
    '-lusb-1.0': 'libusb-1.0',
    '-luuid': 'uuid',
    '-lz': 'zlib',
}
KNOWN_PC_LIBS = frozenset(KNOWN_PC_FILES.keys())

def GypLintPkgConfigs(gypdata):
  """Use pkg-config files for known libs instead of manual -lfoo linkage."""
  def CheckNode(key, value):
    ret = []
    if key.startswith('ldflags') or key.startswith('libraries'):
      for v in KNOWN_PC_LIBS & set(value):
        ret.append(('use pkg-config instead: delete "%s" from "%s" and add '
                    '"%s" to "deps"') % (v, key, KNOWN_PC_FILES[v]))
    return ret

  return WalkGyp(CheckNode, gypdata)


# The regex used to find gyplint options in the file.
# This matches the regex pylint uses.
OPTIONS_RE = re.compile(r'^\s*#.*\bgyplint:\s*([^\n;]+)', flags=re.MULTILINE)

# Object holding linter settings.
LintSettings = collections.namedtuple('LinterSettings', (
    # Linters to skip.
    'skip',
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

  for option in options:
    key, value = option.split('=', 1)
    key = key.strip()
    value = value.strip()

    if key == 'disable':
      skip.update(x.strip() for x in value.split(','))
    else:
      raise ValueError('%s: unknown gyplint option: %s' % (name, key))

  return LintSettings(skip)


def CheckGyp(name, gypdata, settings=None):
  """Check |gypdata| for common mistakes."""
  if settings is None:
    settings = ParseOptions([])

  linters = [x for x in globals()
             if x.startswith('GypLint') and x not in settings.skip]

  ret = []
  for linter in linters:
    functor = globals().get(linter)
    for result in functor(gypdata):
      ret.append(LintResult(linter, name, result, logging.ERROR))
  return ret


def CheckGypData(name, data):
  """Check |data| (gyp file as a string) for common mistakes."""
  try:
    gypdata = gyp_compiler.CheckedEval(data)
  except Exception as e:
    return [LintResult('gyp.input.CheckedEval', name, 'invalid format: %s' % e,
                       logging.ERROR)]

  # Parse the gyplint flags in the file.
  m = OPTIONS_RE.findall(data)
  settings = ParseOptions(m, name=name)

  return CheckGyp(name, gypdata, settings)


def CheckGypFile(gypfile):
  """Check |gypfile| for common mistakes."""
  with open(gypfile) as fp:
    return CheckGypData(gypfile, fp.read())


def FilterFiles(files, extensions):
  """Filter out |files| based on |extensions|."""
  for f in files:
    # Chop of the leading period as we'll get back ".bin".
    extension = os.path.splitext(f)[1][1:]
    if extension in extensions and not os.path.basename(f).startswith('.'):
      logging.debug('Checking %s', f)
      yield f
    else:
      logging.debug('Skipping %s', f)


def FilterPaths(paths, extensions):
  """Walk |paths| recursively and filter out content in it."""
  files = []
  dirs = []
  for path in paths:
    if os.path.isdir(path):
      dirs.append(path)
    else:
      files.append(path)

  for gypfile in FilterFiles(files, extensions):
    yield gypfile

  for gypdir in dirs:
    for root, _, files in os.walk(gypdir):
      for gypfile in FilterFiles(files, extensions):
        yield os.path.join(root, gypfile)


def GetParser():
  """Return an argument parser."""
  parser = commandline.ArgumentParser(description=__doc__)
  parser.add_argument('--extensions', default='gyp,gypi',
                      help='Comma delimited file extensions to check. '
                           '(default: %(default)s)')
  parser.add_argument('files', nargs='*',
                      help='Files to run lint.')
  return parser


def main(argv):
  parser = GetParser()
  opts = parser.parse_args(argv)

  if not opts.files:
    logging.warning('No files provided to lint.  Doing nothing.')
    return 0

  extensions = set(opts.extensions.split(','))
  num_files = 0
  for gypfile in FilterPaths(opts.files, extensions):
    results = CheckGypFile(gypfile)
    if results:
      logging.error('**** %s: found %i issue(s)', gypfile, len(results))
      for result in results:
        logging.log(result.type, '%s: %s', result.linter, result.msg)
      num_files += 1
  if num_files:
    logging.error('%i file(s) failed linting', num_files)
  return 1 if num_files else 0


if __name__ == '__main__':
  commandline.ScriptWrapperMain(lambda _: main)
