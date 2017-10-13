#!/usr/bin/env python2
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The unit test suite for the CrosConfigHost CLI tool."""

from __future__ import print_function

import os
import subprocess
import unittest

import fdt_util

CLI_FILE = './cros_config_host_py/cros_config_host.py'
DTS_FILE = '../libcros_config/test.dts'


class CrosConfigHostTest(unittest.TestCase):
  """The unit test suite for the CrosConfigHost CLI tool."""
  def setUp(self):
    path = os.path.join(os.path.dirname(__file__), DTS_FILE)
    (self.dtb_file, self.temp_file) = fdt_util.EnsureCompiled(path)

  def tearDown(self):
    if self.temp_file is not None:
      os.remove(self.temp_file.name)

  def CheckManyLinesWithoutSpaces(self, output):
    # Expect there to be a few lines
    self.assertGreater(len(output.split()), 3)
    # Expect each line to not have spaces in it
    for line in output.split():
      self.assertFalse(' ' in line)
      self.assertNotEqual(line[-1:], ' ')
    # Expect the last thing in the output to be a newline
    self.assertEqual(output[-1:], os.linesep)

  def CheckManyLines(self, output):
    # Expect there to be a few lines
    self.assertGreater(len(output.split()), 3)
    # Expect each line to not end in space
    for line in output.split():
      self.assertNotEqual(line[-1:], ' ')
    # Expect the last thing in the output to be a newline
    self.assertEqual(output[-1:], os.linesep)


  def testReadStdin(self):
    call_args = '{} - list-models < {}'.format(CLI_FILE, self.dtb_file)
    output = subprocess.check_output(call_args, shell=True)
    self.CheckManyLinesWithoutSpaces(output)

  def testListModels(self):
    call_args = '{} {} list-models'.format(CLI_FILE, self.dtb_file).split()
    output = subprocess.check_output(call_args)
    self.CheckManyLinesWithoutSpaces(output)

  def testListModelsInvalid(self):
    call_args = '{} invalid.dtb list-models'.format(CLI_FILE).split()
    with open(os.devnull, 'w') as devnull:
      with self.assertRaises(subprocess.CalledProcessError):
        subprocess.check_call(call_args, stdout=devnull, stderr=devnull)

  def testGetPropSingle(self):
    call_args = '{} {} --model=pyro get / wallpaper'.format(
        CLI_FILE, self.dtb_file).split()
    output = subprocess.check_output(call_args)
    self.assertEqual(output, 'default' + os.linesep)

  def testGetPropSingleWrongModel(self):
    call_args = '{} {} --model=dne get / wallpaper'.format(
        CLI_FILE, self.dtb_file).split()
    output = subprocess.check_output(call_args)
    self.assertEqual(output, '')

  def testGetPropSingleWrongPath(self):
    call_args = '{} {} --model=pyro get /dne wallpaper'.format(
        CLI_FILE, self.dtb_file).split()
    output = subprocess.check_output(call_args)
    self.assertEqual(output, os.linesep)

  def testGetPropSingleWrongProp(self):
    call_args = '{} {} --model=pyro get / dne'.format(
        CLI_FILE, self.dtb_file).split()
    output = subprocess.check_output(call_args)
    self.assertEqual(output, os.linesep)

  def testGetPropAllModels(self):
    call_args = '{} {} --all-models get / wallpaper'.format(
        CLI_FILE, self.dtb_file).split()
    output = subprocess.check_output(call_args)
    self.assertEqual(output,
                     'default{ls}{ls}epic{ls}{ls}'.format(ls=os.linesep))

  def testGetFirmwareUris(self):
    call_args = '{} {} --model=pyro get-firmware-uris'.format(
        CLI_FILE, self.dtb_file).split()
    output = subprocess.check_output(call_args)
    self.CheckManyLines(output)

  def testGetSharedFirmwareUris(self):
    call_args = '{} {} --model=caroline get-firmware-uris'.format(
        CLI_FILE, self.dtb_file).split()
    output = subprocess.check_output(call_args)
    self.CheckManyLines(output)


if __name__ == '__main__':
  unittest.main()
