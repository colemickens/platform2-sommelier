#!/usr/bin/env python2
# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for gnlint."""

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

import gnlint


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


class UtilityTests(cros_test_lib.MockTestCase):
  """Tests for utility funcs."""

  def testFilterFiles(self):
    """Check filtering of files based on extension works."""
    exp = [
        'cow.gn',
        'cow.gni',
    ]
    files = [
        '.gitignore',
        '.gitignore.gn',
        'cow.gn',
        'cow.gn.orig',
        'cow.gni',
        'gn',
        'README.md',
    ]
    extensions = set(('gn', 'gni'))
    result = sorted(gnlint.FilterFiles(files, extensions))
    self.assertEqual(result, exp)

  def testGetParser(self):
    """Make sure it doesn't crash."""
    parser = gnlint.GetParser()
    self.assertTrue(isinstance(parser, commandline.ArgumentParser))

  def testMain(self):
    """Make sure it doesn't crash."""
    gnlint.main(['foo'])

  def testMainErrors(self):
    """Make sure outputting results doesn't crash."""
    self.PatchObject(gnlint, 'CheckGnFile', return_value=[
        gnlint.LintResult('LintFunc', 'foo.gn', 'msg!', logging.ERROR),
    ])
    gnlint.main(['foo.gn'])


class FilesystemUtilityTests(cros_test_lib.MockTempDirTestCase):
  """Tests for utility funcs that access the filesystem."""

  def testCheckGnFile(self):
    """Check CheckGnFile tails down correctly."""
    content = '# gn file\n'
    gnfile = os.path.join(self.tempdir, 'asdf')
    osutils.WriteFile(gnfile, content)
    ret = gnlint.CheckGnFile(gnfile)
    self.assertEqual(ret, [])

  def testCheckFormatDetectError(self):
    """Check CheckGnFile detects non-standard format."""
    content = 'executable("foo"){\n}\n'   # no space after ')'
    gnfile = os.path.join(self.tempdir, 'asdf')
    osutils.WriteFile(gnfile, content)
    ret = gnlint.CheckGnFile(gnfile)
    self.assertEqual(len(ret), 1)

  def testFilterPaths(self):
    """Check filtering of files in subdirs."""
    subfile = os.path.join(self.tempdir, 'a/b/c.gn')
    osutils.Touch(subfile, makedirs=True)
    subdir = os.path.join(self.tempdir, 'src')
    for f in ('blah.gni', 'Makefile', 'source.cc'):
      osutils.Touch(os.path.join(subdir, f), makedirs=True)

    exp = sorted([
        os.path.join(subdir, 'blah.gni'),
        subfile,
    ])
    paths = [
        subdir,
        subfile,
    ]
    extensions = set(('gn', 'gni'))
    result = sorted(gnlint.FilterPaths(paths, extensions))
    self.assertEqual(result, exp)


def CreateTestData(flag_name, operator, value):
  """Creates a dictionary for testing simple assignment in a static_library."""
  # static_library("my_static_library") {
  #   <flag_name> <operator> [ <value> ]
  # }
  #
  # for example, when flag_name='cflags', operator='+=', value='"-lfoo"',
  # the result stands for a gn file like this:
  # static_library("my_static_library") {
  #   cflags += [ "-lfoo" ]
  # }
  if not isinstance(value, list):
    value = [value]
  value_list = []
  for item in value:
    value_list.append({'type': 'LITERAL', 'value': item})
  return {
      'child': [{
          'child': [{
              'child': [{
                  'type': 'LITERAL',
                  'value': '\"my_static_library\"'
              }],
              'type': 'LIST'
          }, {
              'child': [{
                  'child': [{
                      'type': 'IDENTIFIER',
                      'value': flag_name
                  }, {
                      'child': value_list,
                      'type': 'LIST'
                  }],
                  'type': 'BINARY',
                  'value': operator
              }],
              'type': 'BLOCK'
          }],
          'type': 'FUNCTION',
          'value': 'static_library'
      }],
      'type': 'BLOCK'
  }


class GnLintTests(LintTestCase):
  """Tests of various gn linters."""
  STUB_DATA = {'type': 'BLOCK'}

  def testGnLintLibFlags(self):
    """Verify GnLintLibFlags catches bad inputs."""

    self._CheckLinter(gnlint.GnLintLibFlags, [
        CreateTestData('ldflags', '=', '-lfoo'),
        CreateTestData('ldflags', '+=', '-lfoo'),
        CreateTestData('ldflags', '-=', '-lfoo'),
    ])

  def testGnLintVisibilityFlags(self):
    """Verify GnLintVisibilityFlags catches bad inputs."""
    self._CheckLinter(gnlint.GnLintVisibilityFlags, [
        CreateTestData('cflags', '=', '"-fvisibility"'),
        CreateTestData('cflags', '+=', '"-fvisibility"'),
        CreateTestData('cflags', '-=', '"-fvisibility=default"'),
        CreateTestData('cflags_c', '-=', '"-fvisibility=hidden"'),
        CreateTestData('cflags_cc', '-=', '"-fvisibility=internal"'),
    ])

  def testGnLintDefineFlags(self):
    """Verify GnLintDefineFlags catches bad inputs."""
    self._CheckLinter(gnlint.GnLintDefineFlags, [
        CreateTestData('cflags', '=', '"-D_FLAG"'),
        CreateTestData('cflags', '+=', '"-D_FLAG"'),
        CreateTestData('cflags', '-=', '"-D_FLAG=1"'),
        CreateTestData('cflags_c', '=', '"-D_FLAG=0"'),
        CreateTestData('cflags_cc', '=', '"-D_FLAG=\"something\""'),
    ])

  def testGnLintCommonTesting(self):
    """Verify GnLintCommonTesting catches bad inputs."""
    self._CheckLinter(gnlint.GnLintCommonTesting, [
        CreateTestData('libs', '=', '"gmock"'),
        CreateTestData('libs', '=', '"gtest"'),
        CreateTestData('libs', '=', ['"gmock"', '"gtest"'])
    ])


if __name__ == '__main__':
  cros_test_lib.main(module=__name__)
