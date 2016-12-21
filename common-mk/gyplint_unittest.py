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


class LintTests(cros_test_lib.TestCase):
  """Tests of various linters."""

  def _CheckLinter(self, functor, inputs):
    """Make sure |functor| rejects every input in |inputs|."""
    # First run a sanity check.
    ret = functor({})
    self.assertEqual(ret, [])

    # Then run through all the bad inputs.
    for gyp in inputs:
      ret = functor(gyp)
      self.assertNotEqual(ret, 0)

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

  def testGypLintPkgConfigs(self):
    """Verify GypLintPkgConfigs catches bad inputs."""
    self._CheckLinter(gyplint.GypLintPkgConfigs, (
        {'libraries': ['-lz']},
        {'libraries': ['-lssl']},
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
    ret = gyplint.CheckGypData('my.gyp', '{}')
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
