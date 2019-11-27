#!/usr/bin/env python2
# -*- coding: utf-8 -*-
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Linter for checking GYP files used in platform2 projects."""

from __future__ import print_function

import collections
import difflib
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


def GypLintDefines(gypdata):
  """Flags in 'defines' should have valid names."""
  def CheckNode(key, value):
    ret = []
    if key.startswith('defines'):
      for flag in value:
        # People sometimes typo the name.
        if flag.startswith('-D'):
          ret.append('defines do not use -D prefixes: use "%s" instead of "%s"'
                     % (flag[2:], flag))
        else:
          # Make sure the name is valid CPP.
          name = flag.split('=', 1)[0]
          if not re.match(r'^[a-zA-Z0-9_]+$', name):
            ret.append('invalid define name: %s' % (name,))
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


def GypLintStaticSharedLibMixing(gypdata):
  """Static libs linked into shared libs need special PIC handling.

  Normally static libs are built using PIE because they only get linked into
  PIEs.  But if someone tries linking the static libs into a shared lib, we
  need to make sure the code is built using PIC.

  Note: We don't do an inverse check (PIC static libs not used by shared libs)
  as the static libs might be installed by the ebuild.  Not that we want to
  encourage that situation, but it is what it is ...
  """
  # Record static_libs that build as PIE, and all the deps of shared_libs.
  # Afterwards, we'll sanity check all the shared lib deps.
  pie_static_libs = []
  shared_lib_deps = {}
  def CheckNode(key, value):
    if key != 'targets':
      return []

    for target in value:
      ttype = target.get('type')
      name = target['target_name']

      if ttype == 'static_library':
        if (not '-fPIE' in target.get('cflags!', []) or
            not '-fPIC' in target.get('cflags', [])):
          pie_static_libs.append(name)
      elif ttype == 'shared_library':
        assert name not in shared_lib_deps, 'duplicate target: %s' % name
        shared_lib_deps[name] = target.get('dependencies', [])

    return []

  # We build up the full state first rather than check it as we go as gyp
  # files do not force target ordering.
  WalkGyp(CheckNode, gypdata)

  # Now with the full state, run the checks.
  ret = []
  for pie_lib in pie_static_libs:
    # Pull out all shared libs that depend on static PIE libs.
    dependency_libs = [
        shared_lib for shared_lib, deps in shared_lib_deps.items()
        if pie_lib in deps
    ]
    if dependency_libs:
      ret.append(('static library "%(pie)s" must be compiled as PIC, not PIE, '
                  'because it is linked into the shared libraries %(pic)s; '
                  'add this to the "%(pie)s" target to fix: '
                  "'cflags!': ['-fPIE'], 'cflags': ['-fPIC']")
                 % {'pie': pie_lib, 'pic': dependency_libs})
  return ret


def GypLintOrderedFiles(gypdata):
  """Files should be kept sorted.

  Python's sorted() function relies on __cmp__ of the objects it is sorting.
  Since we're feeding it ASCII strings, it will do byte-wise comparing.  This
  matches clang-format behavior when it sorts include files.
  """
  def CheckNode(key, value):
    ret = []
    if key == 'sources':
      lines = list(difflib.unified_diff(value, sorted(value), lineterm=''))
      if lines:
        # Skip the first three entries as those are the diff header.
        ret.append('source files should be kept sorted:\n%s' %
                   ('\n'.join(lines[3:]),))
    return ret

  return WalkGyp(CheckNode, gypdata)


def GypLintSourceFileNames(gypdata):
  """Enforce various filename conventions."""
  re_unittest = re.compile(r'_unittest\.(cc|c|h)$')

  def CheckNode(key, value):
    ret = []
    if key == 'sources':
      for path in value:
        # Enforce xxx_test.cc naming.
        if re_unittest.search(path):
          ret.append('%s: rename unittest file to "%s"' %
                     (path, path.replace('_unittest', '_test')))
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
    '-lvboot_host': 'vboot_host',
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


def RawLintWhitespace(data):
  """Make sure whitespace is sane."""
  ret = []
  if not data.endswith('\n'):
    ret.append('missing newline at end of file')
  return ret


def LineIsComment(line):
  """Whether this entire line is a comment (and whitespace).

  Note: This doesn't handle inline comments like:
    [  # Blah blah.
  But we discourage those in general, so shouldn't be a problem.
  """
  return line.lstrip().startswith('#')


def LinesLintWhitespace(lines):
  """Make sure whitespace is sane."""
  ret = []

  for i, line in enumerate(lines, 1):
    if '\t' in line:
      ret.append('use spaces, not tabs: line %i: %s' % (i, line))

    if line.rstrip() != line:
      ret.append('delete trailing whitespace: line %i: %s' % (i, line))

    if LineIsComment(line):
      comment = line.lstrip()
      if len(comment) > 1 and comment[1] != ' ':
        ret.append('add space after # mark: line %i: %s' % (i, line))

  if lines:
    if not lines[0]:
      ret.append('delete leading blanklines')

    if not lines[-1]:
      ret.append('delete trailing blanklines')

  return ret


def LinesLintDanglingCommas(lines):
  """Check for missing dangling commas."""
  ret = []
  for i, line in enumerate(lines, 1):
    if not LineIsComment(line):
      if len(line) > 1 and line.endswith(('}', ']', "'")):
        ret.append('add a dangling comma: line %i: %s' % (i, line))
  return ret


def LinesLintSingleQuotes(lines):
  """Check for double quote usage."""
  ret = []
  for i, line in enumerate(lines, 1):
    if not LineIsComment(line):
      if line.endswith('"'):
        ret.append('use single quotes instead of double quotes: line %i: %s' %
                   (i, line))
  return ret


def LinesLintCuddled(lines):
  """Allow for cuddled braces only with one liners.

  This is OK:
    'dependencies': ['libwimax_manager'],
  But not this:
    'dependencies': ['libwimax_manager',
                     'libfoo'],
  """
  ret = []

  named_element = re.compile(r"^'[^']+': ([[{].*)$")

  for i, line in enumerate(lines, 1):
    if LineIsComment(line):
      continue

    check = line.lstrip()

    # Handle named elements like:
    # 'foo': [
    m = named_element.match(check)
    if m:
      check = m.group(1)

    # Skip lines that are just one char -- can't be dangling!
    if len(check) <= 1:
      continue

    # Skip lines that don't have a list or dict.
    if check[0] not in ('[', '{'):
      continue

    # Ignore conditional forms:
    # ['deps != []', {
    if check.startswith("['") and check.endswith(', {'):
      continue

    # Ignore lists of objects form:
    # 'foo': [{
    if check == '[{':
      continue

    # Ignore one-line forms:
    # ['foo', 'bar'],
    #
    # Note: This will not catch lists of lists like:
    # 'item': [ ['foo', 'bar'],
    #           ['hungry', 'cat'] ],
    # But that's OK because those will usually get caught by the indentation
    # checker.  We'd need to have the AST available here to support it.
    if check[-1] == ',' and (check[0], check[-2]) == ('[', ']'):
      continue

    # If we're still here, the line is bad.
    ret.append('uncuddle lists/dicts with multiple elements: line %i: %s' %
               (i, line))

  return ret


def LinesLintIndent(lines):
  """Make sure indentation levels are correct."""
  ret = []

  indent = 0
  hanging_indent = 0
  for i, line in enumerate(lines, 1):
    # Remove any leading whitespace & inline comments.
    stripped = line.split('#', 1)[0].strip()

    # Always allow blank lines.
    if not stripped:
      continue

    # If the line is closing an array/dict, we move up one indentation level.
    if stripped[0] in (']', '}'):
      indent -= 2

    # Make sure this line has the right indentation.
    num_spaces = len(line) - len(line.lstrip())
    if num_spaces == indent + hanging_indent:
      hanging_indent = 0
      if stripped[-1] in ('[', '{'):
        indent += 2
      elif stripped[-1] in (':',):
        hanging_indent = 2
      elif stripped[0] in (']', '}'):
        pass
      elif stripped.endswith(','):
        pass
      elif LineIsComment(line):
        pass
      else:
        ret.append('internal error: unknown (to the linter) syntax: line %i: %s'
                   % (i, line))
      continue

    ret.append('indent is incorrect: %i: %s' % (i, line))

  return ret


# The regex used to find gyplint options in the file.
# This matches the regex pylint uses.
OPTIONS_RE = re.compile(r'^\s*#.*\bgyplint:\s*([^\n;]+)', flags=re.MULTILINE)

# Object holding linter settings.
LintSettings = collections.namedtuple('LinterSettings', (
    # Linters to skip.
    'skip',
    # Problems we found in the lint settings themselves.
    'errors',
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
  errors = []

  # Parse all the gyplint directives.
  for option in options:
    key, value = option.split('=', 1)
    key = key.strip()
    value = value.strip()

    # Parse each sub-option.
    if key == 'disable':
      skip.update(x.strip() for x in value.split(','))
    else:
      errors.append(LintResult('ParseOptions', name,
                               'unknown gyplint option: %s' % (key,),
                               logging.ERROR))

  # Validate the options.
  all_linters = FindAllLinters()
  bad_linters = skip - all_linters
  if bad_linters:
    errors.append(LintResult('ParseOptions', name,
                             'unknown linters: %s' % (bad_linters,),
                             logging.ERROR))

  return LintSettings(skip, errors)


def FindLinters(prefix):
  """Find all linters starting with |prefix|."""
  return set(x for x in globals() if x.startswith(prefix))


def FindAllLinters():
  """Return all valid linters we know about."""
  return set(
      FindLinters('GypLint') |
      FindLinters('RawLint') |
      FindLinters('LinesLint')
  )


def RunLinters(prefix, name, data, settings=None):
  """Run linters starting with |prefix| against |data|."""
  ret = []

  if settings is None:
    settings = ParseOptions([])
    ret += settings.errors

  linters = [x for x in FindLinters(prefix) if x not in settings.skip]

  for linter in linters:
    functor = globals().get(linter)
    for result in functor(data):
      ret.append(LintResult(linter, name, result, logging.ERROR))
  return ret


def CheckGyp(name, gypdata, settings=None):
  """Check |gypdata| for common mistakes."""
  return RunLinters('GypLint', name, gypdata, settings=settings)


def CheckGypData(name, data):
  """Check |data| (gyp file as a string) for common mistakes."""
  try:
    gypdata = gyp_compiler.CheckedEval(data)
  except Exception as e:
    return [LintResult('gyp.input.CheckedEval', name, 'invalid format: %s' % e,
                       logging.ERROR)]

  ret = []

  # Parse the gyplint flags in the file.
  m = OPTIONS_RE.findall(data)
  settings = ParseOptions(m)

  ret += settings.errors
  lines = data.splitlines()

  # Run linters on the raw data first (for style/syntax).
  ret += RunLinters('RawLint', name, data, settings=settings)
  ret += RunLinters('LinesLint', name, lines, settings=settings)
  # Then run linters against the parsed AST.
  ret += CheckGyp(name, gypdata, settings)

  return ret


def CheckGypFile(gypfile):
  """Check |gypfile| for common mistakes."""
  if not os.path.exists(gypfile):
    # The file has been deleted.
    return
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
