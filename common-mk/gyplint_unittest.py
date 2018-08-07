#!/usr/bin/env python2
# -*- coding: utf-8 -*-
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for gyplint."""

from __future__ import print_function

import os
import sys

# Find chromite!
sys.path.insert(0, os.path.join(os.path.dirname(os.path.realpath(__file__)),
                                '..', '..', '..'))

from chromite.lib import commandline
from chromite.lib import cros_logging as logging
from chromite.lib import cros_test_lib
from chromite.lib import osutils

import gyplint


class LintTestCase(cros_test_lib.TestCase):
  """Helper for running linters."""

  def _CheckLinter(self, functor, inputs):
    """Make sure |functor| rejects every input in |inputs|."""
    # First run a sanity check.
    ret = functor(self.STUB_DATA)
    self.assertEqual(ret, [])

    # Then run through all the bad inputs.
    for x in inputs:
      ret = functor(x)
      self.assertNotEqual(ret, [])


class GypLintTests(LintTestCase):
  """Tests of various gyp linters."""

  STUB_DATA = {}

  def testGypLintLibFlags(self):
    """Verify GypLintLibFlags catches bad inputs."""
    self._CheckLinter(gyplint.GypLintLibFlags, (
        {'ldflags': ['-lfoo']},
        {'ldflags+': ['-lfoo']},
        {'ldflags!': ['-lfoo']},
    ))

  def testGypLintVisibilityFlags(self):
    """Verify GypLintVisibilityFlags catches bad inputs."""
    self._CheckLinter(gyplint.GypLintVisibilityFlags, (
        {'cflags': ['-fvisibility']},
        {'cflags+': ['-fvisibility']},
        {'cflags!': ['-fvisibility=default']},
        {'cflags_c': ['-fvisibility=hidden']},
        {'cflags_cc': ['-fvisibility=internal']},
    ))

  def testGypLintDefineFlags(self):
    """Verify GypLintDefineFlags catches bad inputs."""
    self._CheckLinter(gyplint.GypLintDefineFlags, (
        {'cflags': ['-D_FLAG']},
        {'cflags+': ['-D_FLAG']},
        {'cflags!': ['-D_FLAG=1']},
        {'cflags_c': ['-D_FLAG=0']},
        {'cflags_cc': ['-D_FLAG="something"']},
    ))

  def testGypLintCommonTesting(self):
    """Verify GypLintCommonTesting catches bad inputs."""
    self._CheckLinter(gyplint.GypLintCommonTesting, (
        {'libraries': ['-lgmock']},
        {'libraries': ['-lgtest']},
        {'libraries': ['-lgmock', '-lgtest']},
    ))

  def testGypLintStaticSharedLibMixing(self):
    """Verify GypLintStaticSharedLibMixing catches bad inputs."""
    self._CheckLinter(gyplint.GypLintStaticSharedLibMixing, (
        {
            'targets': [
                {
                    'target_name': 'libhammerd',
                    'type': 'static_library',
                },
                {
                    'target_name': 'libhammerd-api',
                    'type': 'shared_library',
                    'dependencies': ['libhammerd'],
                },
                {
                    'target_name': 'another-target',
                    'type': 'shared_library',
                    'dependencies': ['libhammerd'],
                },
            ],
        },
    ))

  def testGypLintOrderedFiles(self):
    """Verify GypLintOrderedFiles catches bad inputs."""
    self._CheckLinter(gyplint.GypLintOrderedFiles, (
        {'sources': ['b.h', 'b.cc']},
        {'sources': ['zzz.cc', 'a.h']},
    ))

  def testGypLintSourceFileNames(self):
    """Verify GypLintSourceFileNames catches bad filenames."""
    self._CheckLinter(gyplint.GypLintSourceFileNames, (
        {'sources': ['foo_unittest.cc']},
        {'sources': ['foo_unittest.h']},
        {'sources': ['foo_unittest.c']},
    ))

  def testGypLintPkgConfigs(self):
    """Verify GypLintPkgConfigs catches bad inputs."""
    self._CheckLinter(gyplint.GypLintPkgConfigs, (
        {'libraries': ['-lz']},
        {'libraries': ['-lssl']},
    ))


class LinesLintTests(LintTestCase):
  """Tests of various line based linters."""

  STUB_DATA = ['{', '}']

  def testLinesLintWhitespace(self):
    """Verify LinesLintWhitespace catches bad inputs."""
    self._CheckLinter(gyplint.LinesLintWhitespace, (
        # Tabs instead of spaces.
        ['{', '\t[]', '}'],
        # Trailing whitespace.
        ['{', '}  '],
        # Leading blanklines.
        ['', '{', '}'],
        # Trailing blanklines.
        ['{', '}', ''],
    ))

  def testLinesLintDanglingCommas(self):
    """Verify LinesLintDanglingCommas catches bad inputs."""
    self._CheckLinter(gyplint.LinesLintDanglingCommas, (
        ['{', '  [', '  ]', '}'],
        ['{', '  {', '  }', '}'],
        ['{', "  'foo': 'bar'", '}'],
    ))

  def testLinesLintSingleQuotes(self):
    """Verify LinesLintSingleQuotes catches bad inputs."""
    self._CheckLinter(gyplint.LinesLintSingleQuotes, (
        ['{', '  0: "blah"', '}'],
    ))

  def testLinesLintCuddled(self):
    """Verify LinesLintCuddled catches bad inputs."""
    self._CheckLinter(gyplint.LinesLintCuddled, (
        ['{', "  'foo': [ 'asdf',", ']', '}'],
        ['{', "  ['foo',", ']', '}'],
        ['{', "  {'foo': 'bar',", '}', '}'],
    ))

  def testLinesLintCuddledValid(self):
    """Allow various forms."""
    DATA = (
        "['deps != []', {",
        "'foo': [{",
        "['foo', 'bar'],",
        "'foo': ['foo', 'bar'],",
    )
    for s in DATA:
      self.assertEqual(gyplint.LinesLintCuddled([s]), [])

  def testLinesLintIndent(self):
    """Verify LinesLintIndent catches bad inputs."""
    self._CheckLinter(gyplint.LinesLintIndent, (
        ['  {'],
        ['', '   ['],
    ))

  def testLinesLintIndentValid(self):
    """Allow various valid indentation levels."""
    self.assertEqual(gyplint.LinesLintIndent([
        '{',
        # Increase by one level.
        "  'foo': [",
        "    'key',",
        # Decrease by one level.
        '  ],',
        '}',
        # Incrementally increase by one level.
        '[',
        '  [',
        '    [',
        '      [',
        '        [',
        # Then decrease back down.
        '        ],',
        '      ],',
        '    ],',
        '  ],',
        '],',
    ]), [])


class RawLintTests(LintTestCase):
  """Tests of various raw linters."""

  STUB_DATA = '{}\n'

  def testRawLintWhitespace(self):
    """Verify RawLintWhitespace catches bad inputs."""
    self._CheckLinter(gyplint.RawLintWhitespace, (
        # Missing trailing newline.
        '{}',
    ))


class UtilityTests(cros_test_lib.MockTestCase):
  """Tests for utility funcs."""

  def testWalkGyp(self):
    """Check we get called for all the nodes."""
    gyp = {
        'alist': [1, 2, 3],
        'adict': {
            'anotherlist': ['a', 'b', 'c'],
            'edict': {}
        },
    }
    exp = [
        ('alist', gyp['alist']),
        ('adict', gyp['adict']),
        ('anotherlist', gyp['adict']['anotherlist']),
        ('edict', gyp['adict']['edict']),
    ]

    visited = []
    def CheckNode(key, value):
      visited.append((key, value))
      return ['errmsg']
    ret = gyplint.WalkGyp(CheckNode, gyp)

    self.assertEqual(ret, ['errmsg'] * 4)
    # Since ordering of dict keys isn't guaranteed (nor do we care), sort them.
    self.assertEqual(sorted(visited), sorted(exp))

  def testCheckGyp(self):
    """Check CheckGyp doesn't crash."""
    ret = gyplint.CheckGyp('my.gyp', {})
    self.assertEqual(ret, [])

  def testCheckGypData(self):
    """Check CheckGypData doesn't crash."""
    ret = gyplint.CheckGypData('my.gyp', '{\n}\n')
    self.assertEqual(ret, [])

  def testCheckGypDataInvalidInput(self):
    """Check CheckGypData doesn't crash on bad gyps."""
    ret = gyplint.CheckGypData('my.gyp', '{"!')
    self.assertNotEqual(ret, [])
    self.assertEqual(len(ret), 1)
    self.assertTrue(isinstance(ret[0], gyplint.LintResult))

  def testFilterFiles(self):
    """Check filtering of files based on extension works."""
    exp = [
        'cow.gyp',
        'cow.gypi',
    ]
    files = [
        '.gitignore',
        '.gitignore.gyp',
        'cow.gyp',
        'cow.gyp.orig',
        'cow.gypi',
        'gyp',
        'README.md',
    ]
    extensions = set(('gyp', 'gypi'))
    result = sorted(gyplint.FilterFiles(files, extensions))
    self.assertEqual(result, exp)

  def testLineIsComment(self):
    """Make sure comment parsing is correct."""
    TRUE_INPUTS = (
        '# A comment',
        '  # A comment',
        '\t\t# A comment',
        ' #Comment!',
    )
    FALSE_INPUTS = (
        '  "# In a string"',
        '  [  # At the end.',
    )
    for s in TRUE_INPUTS:
      self.assertTrue(gyplint.LineIsComment(s))
    for s in FALSE_INPUTS:
      self.assertFalse(gyplint.LineIsComment(s))

  def testGetParser(self):
    """Make sure it doesn't crash."""
    parser = gyplint.GetParser()
    self.assertTrue(isinstance(parser, commandline.ArgumentParser))

  def testMain(self):
    """Make sure it doesn't crash."""
    gyplint.main(['foo'])

  def testMainErrors(self):
    """Make sure outputting results doesn't crash."""
    self.PatchObject(gyplint, 'CheckGypFile', return_value=[
        gyplint.LintResult('LintFunc', 'foo.gyp', 'msg!', logging.ERROR),
    ])
    gyplint.main(['foo.gyp'])


class OptionsTests(cros_test_lib.TestCase):
  """Tests for option parsing."""

  def testEmpty(self):
    """We should get a dummy object when no inputs are specified."""
    ret = gyplint.ParseOptions([])
    self.assertTrue(isinstance(ret, gyplint.LintSettings))
    self.assertEqual(ret.skip, set())
    self.assertEqual(ret.errors, [])

  def testInvalid(self):
    """Flag invalid options as errors."""
    ret = gyplint.ParseOptions(['disab=foo'])
    self.assertNotEqual(ret.errors, [])

  def testUnknownLinters(self):
    """Flag unknown linters as errors."""
    ret = gyplint.ParseOptions(['disable=foo'])
    self.assertNotEqual(ret.errors, [])

  def testKnownLinters(self):
    """Don't flag known linters as errors."""
    ret = gyplint.ParseOptions([
        'disable=GypLintOrderedFiles',
        'disable=RawLintWhitespace',
        'disable=LinesLintWhitespace',
    ])
    self.assertEqual(ret.errors, [])

  def testDisabled(self):
    """Check that all the right values are extracted."""
    ret = gyplint.ParseOptions(['disable = some,msgs, can be found  '])
    exp = set(('some', 'msgs', 'can be found'))
    self.assertEqual(ret.skip, exp)


class FilesystemUtilityTests(cros_test_lib.MockTempDirTestCase):
  """Tests for utility funcs that access the filesystem."""

  def testCheckGypFile(self):
    """Check CheckGypFile tails down correctly."""
    m = self.PatchObject(gyplint, 'CheckGypData', return_value=[])
    content = 'gyyyyyyyyp file'
    gypfile = os.path.join(self.tempdir, 'asdf')
    osutils.WriteFile(gypfile, content)
    ret = gyplint.CheckGypFile(gypfile)
    m.assert_called_once_with(gypfile, content)
    self.assertEqual(ret, [])

  def testFilterPaths(self):
    """Check filtering of files in subdirs."""
    subfile = os.path.join(self.tempdir, 'a/b/c.gyp')
    osutils.Touch(subfile, makedirs=True)
    for f in ('blah.gypi', 'Makefile', 'source.cc'):
      osutils.Touch(os.path.join(self.tempdir, f))

    exp = sorted([
        os.path.join(self.tempdir, 'blah.gypi'),
        os.path.join(self.tempdir, 'a/b/c.gyp'),
    ])
    paths = [
        self.tempdir,
    ]
    extensions = set(('gyp', 'gypi'))
    result = sorted(gyplint.FilterPaths(paths, extensions))
    self.assertEqual(result, exp)


if __name__ == '__main__':
  cros_test_lib.main(module=__name__)
