#!/usr/bin/env python2
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit test for build_component"""

from __future__ import print_function
import unittest

from build_component import ParseVersion
from build_component import GetCurrentVersion
from build_component import DecideVersion
from build_component import IsPackage
from build_component import GetCurrentPackageVersion

class TestBuildComponent(unittest.TestCase):
  """Tests for build_component"""

  def test_ParseVersion(self):
    self.assertEqual(ParseVersion('1.2.3.4.5'), [1, 2, 3, 4])
    self.assertEqual(ParseVersion('1.2.3.4'), [1, 2, 3, 4])
    self.assertEqual(ParseVersion('1.2.3'), [1, 2, 3])
    self.assertEqual(ParseVersion('1.2'), [])
    self.assertEqual(ParseVersion('1.2.3.a'), [1, 2, 3])

  def test_GetCurrentVersion(self):
    paths = ['gs://1.0.0.1/',
             'gs://1.0.1/',
             'gs://1.1.0/']
    self.assertEqual(GetCurrentVersion(paths), ('1.1.0', 'gs://1.1.0/'))

    paths = ['gs://1.0.0.1/',
             'gs://1.0.1/',
             'gs://1.1.0']
    self.assertEqual(GetCurrentVersion(paths), ('1.0.1', 'gs://1.0.1/'))

    paths = ['gs://1.0.0.1/',
             'gs://1.0.1/',
             'gs://1.1/', # invalid version
             'gs://invalidpath/']
    self.assertEqual(GetCurrentVersion(paths), ('1.0.1', 'gs://1.0.1/'))

    paths = []
    self.assertEqual(GetCurrentVersion(paths), ('0.0.0.0', ''))

    paths = ['/invalid/path/',
             'gs://1.1.0']
    self.assertEqual(GetCurrentVersion(paths), ('0.0.0.0', ''))

  def test_DecideVersion(self):
    self.assertEqual(DecideVersion('1.0.0.3', '1.0.0.1'), '1.0.0.2')
    self.assertEqual(DecideVersion('1.0.0.3', '1.0.0.4'), '1.0.0.5')
    self.assertEqual(DecideVersion('1.0.0.r-1', '1.0.0'), '1.0.0.1')
    self.assertEqual(DecideVersion('1.0.0.r-1', '1.0.0.1'), '1.0.0.2')
    self.assertEqual(DecideVersion('1.1.0.r-1', '1.0.0'), '1.1.0.1')

  def test_IsPackage(self):
    self.assertEqual(IsPackage('abc-1.2.3-r0', 'abc'), True)
    self.assertEqual(IsPackage('abc-def-1.2.3-r0', 'abc-def'), True)
    self.assertEqual(IsPackage('abc-1.2.3-r0', 'abc-def'), False)
    self.assertEqual(IsPackage('abc-def-1.2.3-r0', 'abc'), False)
    self.assertEqual(IsPackage('abc-def-1.2.3', 'abc-def'), True)

  def test_GetCurrentPackageVersion(self):
    self.assertEqual(GetCurrentPackageVersion('', '[platform]'), '0.0.0.0')

if __name__ == '__main__':
  unittest.main()
