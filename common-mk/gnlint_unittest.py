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


if __name__ == '__main__':
  cros_test_lib.main(module=__name__)
