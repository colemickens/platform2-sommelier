#!/usr/bin/env python2

# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Updates testdata/ based on data pulled from Chromium sources."""

from __future__ import print_function

import argparse
import json
import logging
import os
import re
import shutil
import subprocess
import sys
import tempfile

import yaml


# URLs to GIT paths.
SRC_GIT_URL = 'https://chromium.googlesource.com/chromium/src'
SRC_REMOTE_BRANCH = 'remotes/origin/master'

TESTDATA_PATH = os.path.join(os.path.dirname(__file__), 'testdata')


def GetChromiumSource(temp_dir):
  """Gets Chromium source code under a temp directory.

  Args:
    temp_dir: a temp directory to store the Chromium source code.
  """
  subprocess.check_call(
      ['git', 'clone', SRC_GIT_URL, '--no-checkout', '--depth', '1'],
      cwd=temp_dir)


def WriteTestData(name, value):
  if not value:
    sys.exit('No values found for %s' % name)

  path = os.path.join(TESTDATA_PATH, name + '.yaml')
  logging.info('%s: writing %r', path, value)
  with open(path, 'w') as f:
    f.write('# Automatically generated from ToT Chromium sources\n'
            '# by update_testdata.py. Do not edit manually.\n'
            '\n')
    yaml.dump(value, f, default_flow_style=False)


def UpdateLocales(source_dir):
  """Updates locales.

  Valid locales are entries of the kAcceptLanguageList array in
  l10n_util.cc <http://goo.gl/z8XsZJ>.

  Args:
    source_dir: the directory storing Chromium source.
  """
  cpp_code = subprocess.check_output(
      ['git', 'show', SRC_REMOTE_BRANCH + ':ui/base/l10n/l10n_util.cc'],
      cwd=source_dir)
  match = re.search(r'static[^\n]+kAcceptLanguageList\[\] = \{(.+?)^\}',
                    cpp_code, re.DOTALL | re.MULTILINE)
  if not match:
    sys.exit('Unable to find language list')

  locales = re.findall(r'"(.+)"', match.group(1))
  if not locales:
    sys.exit('Unable to parse language list')

  WriteTestData('locales', sorted(locales))


def UpdateTimeZones(source_dir):
  """Updates time zones.

  Valid time zones are values of the kTimeZones array in timezone_settings.cc
  <http://goo.gl/WSVUeE>.

  Args:
    source_dir: the directory storing Chromium source.
  """
  cpp_code = subprocess.check_output(
      ['git', 'show',
       SRC_REMOTE_BRANCH + ':chromeos/settings/timezone_settings.cc'],
      cwd=source_dir)
  match = re.search(r'static[^\n]+kTimeZones\[\] = \{(.+?)^\}',
                    cpp_code, re.DOTALL | re.MULTILINE)
  if not match:
    sys.exit('Unable to find time zones')

  time_zones = re.findall(r'"(.+)"', match.group(1))
  if not time_zones:
    sys.exit('Unable to parse time zones')

  WriteTestData('time_zones', time_zones)


def UpdateMigrationMap(source_dir):
  """Updates the input method migration map.

  The source is the kEngineIdMigrationMap array in input_method_util.cc
  <http://goo.gl/cDO53r>.

  Args:
    source_dir: the directory storing Chromium source.
  """
  cpp_code = subprocess.check_output(
      ['git', 'show',
       (SRC_REMOTE_BRANCH +
        ':chrome/browser/chromeos/input_method/input_method_util.cc')],
      cwd=source_dir)
  match = re.search(r'kEngineIdMigrationMap\[\]\[2\] = \{(.+?)^\}',
                    cpp_code, re.DOTALL | re.MULTILINE)
  if not match:
    sys.exit('Unable to find kEngineIdMigrationMap')

  map_code = match.group(1)
  migration_map = re.findall(r'{"(.+?)", "(.+?)"}', map_code)
  if not migration_map:
    sys.exit('Unable to parse kEngineIdMigrationMap')

  WriteTestData('migration_map', migration_map)


def UpdateInputMethods(source_dir):
  """Updates input method IDs.

  This is the union of all 'id' fields in input_method/*.json
  <http://goo.gl/z4JGvK>.

  Args:
    source_dir: the directory storing Chromium source.
  """
  files = [line.strip() for line in subprocess.check_output(
      ['git', 'show', SRC_REMOTE_BRANCH +
       ':chrome/browser/resources/chromeos/input_method'],
      cwd=source_dir).split()]
  pattern = re.compile(r'\.json$')
  json_files = [f for f in files if pattern.search(f)]

  input_methods = set()
  for f in json_files:
    contents = json.loads(subprocess.check_output(
        ['git', 'show', (SRC_REMOTE_BRANCH +
                         ':chrome/browser/resources/chromeos/input_method/' +
                         f)],
        cwd=source_dir))
    for c in contents['input_components']:
      input_methods.add(str(c['id']))

  WriteTestData('input_methods', sorted(input_methods))


def main():
  parser = argparse.ArgumentParser(
      description=('Updates some constants in regions_unittest_data.py based '
                   'on data pulled from Chromium sources. This overwrites '
                   'files in testdata, which you must then submit.'))
  unused_args = parser.parse_args()
  logging.basicConfig(level=logging.INFO)

  temp_dir = tempfile.mkdtemp()
  try:
    GetChromiumSource(temp_dir)
    source_dir = os.path.join(temp_dir, 'src')
    UpdateLocales(source_dir)
    UpdateTimeZones(source_dir)
    UpdateInputMethods(source_dir)
  finally:
    shutil.rmtree(temp_dir)

  logging.info('Run "git diff %s" to see changes (if any).', TESTDATA_PATH)
  logging.info('Make sure to submit any changes to %s!', TESTDATA_PATH)


if __name__ == '__main__':
  main()
