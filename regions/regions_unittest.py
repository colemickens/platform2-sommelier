#!/usr/bin/python2
#
# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for regions.py.

These tests ensure that all regions in regions.py are valid.  The tests use
testdata pulled from the Chromium sources.
"""

from __future__ import print_function

import os
import StringIO
import unittest

from chromite.lib import cros_logging as logging
import regions
import yaml


class RegionTest(unittest.TestCase):
  """Tests for the Region class."""

  @classmethod
  def _ReadTestData(cls, name):
    """Reads a YAML-formatted test data file.

    Args:
      name: Name of file in the testdata directory.

    Returns:
      The parsed value.
    """
    with open(os.path.join(os.path.dirname(__file__),
                           'testdata', name + '.yaml')) as f:
      return yaml.load(f)

  @classmethod
  def setUpClass(cls):
    cls.locales = cls._ReadTestData('locales')
    cls.time_zones = cls._ReadTestData('time_zones')
    cls.migration_map = cls._ReadTestData('migration_map')
    cls.input_methods = cls._ReadTestData('input_methods')

  def _ResolveInputMethod(self, method):
    """Resolves an input method using the migration map.

    Args:
      method: An input method ID that may contain prefixes from the
          migration map.  (E.g., "m17n:ar", which contains the "m17n:" prefix.)

    Returns:
      The input method ID after mapping any prefixes.  (E.g., "m17n:ar" will
      be mapped to "vkd_".)
    """
    for k, v in self.migration_map:
      if method.startswith(k):
        method = v + method[len(k):]
    return method

  def testZoneInfo(self):
    all_regions = regions.BuildRegionsDict(include_all=True)

    # Make sure all time zones are present in /usr/share/zoneinfo.
    all_zoneinfos = [os.path.join('/usr/share/zoneinfo', tz)
                     for r in all_regions.values() for tz in r.time_zones]
    missing = [z for z in all_zoneinfos if not os.path.exists(z)]
    self.assertFalse(missing,
                     ('Missing zoneinfo files; are timezones misspelled?: %r' %
                      missing))

  def testBadLocales(self):
    self.assertRaisesRegexp(
        AssertionError, "Locale 'en-us' does not match", regions.Region,
        'us', 'xkb:us::eng', 'America/Los_Angeles', 'en-us', 'ANSI')

  def testBadKeyboard(self):
    self.assertRaisesRegexp(
        AssertionError, "Keyboard pattern 'xkb:us::' does not match",
        regions.Region, 'us', 'xkb:us::', 'America/Los_Angeles', 'en-US',
        'ANSI')

  def testKeyboardRegexp(self):
    self.assertTrue(regions.KEYBOARD_PATTERN.match('xkb:us::eng'))
    self.assertTrue(regions.KEYBOARD_PATTERN.match('ime:ko:korean'))
    self.assertTrue(regions.KEYBOARD_PATTERN.match('m17n:ar'))
    self.assertFalse(regions.KEYBOARD_PATTERN.match('m17n:'))
    self.assertFalse(regions.KEYBOARD_PATTERN.match('foo_bar'))

  def testTimeZones(self):
    for r in regions.BuildRegionsDict(include_all=True).values():
      for tz in r.time_zones:
        if tz not in self.time_zones:
          if r.region_code in regions.REGIONS:
            self.fail(
                'Missing time zone %r; does a new time zone need to be added '
                'to CrOS, or does testdata need to be updated?' % tz)
          else:
            # This is an unconfirmed region; just print a warning.
            logging.warn('Missing time zone %r; does a new time zone need to '
                         'be added to CrOS, or does testdata need to '
                         'be updated? (just a warning, since region '
                         '%r is not a confirmed region)', tz, r.region_code)

  def testLocales(self):
    missing = []
    for r in regions.BuildRegionsDict(include_all=True).values():
      for l in r.locales:
        if l not in self.locales:
          missing.append(l)
    self.assertFalse(
        missing,
        ('Missing locale; does testdata need to be updated?: %r' %
         missing))

  def testInputMethods(self):
    # Verify that every region is present in the dict.
    for r in regions.BuildRegionsDict(include_all=True).values():
      for k in r.keyboards:
        resolved_method = self._ResolveInputMethod(k)
        # Make sure the keyboard method is present.
        self.assertIn(
            resolved_method, self.input_methods,
            'Missing keyboard layout %r (resolved from %r)' % (
                resolved_method, k))

  def testFirmwareLocales(self):
    bmpblk_dir = os.path.join(
        os.environ.get('CROS_WORKON_SRCROOT'), 'src', 'platform', 'bmpblk')
    if not os.path.exists(bmpblk_dir):
      logging.warn('Skipping testFirmwareLocales, since %r is missing',
                   bmpblk_dir)
      return

    bmp_locale_dir = os.path.join(bmpblk_dir, 'strings', 'locale')
    for r in regions.BuildRegionsDict(include_all=True).values():
      for l in r.locales:
        paths = [os.path.join(bmp_locale_dir, l)]
        if '-' in l:
          paths.append(os.path.join(bmp_locale_dir, l.partition('-')[0]))
        if not any([os.path.exists(x) for x in paths]):
          if r.region_code in regions.REGIONS:
            self.fail(
                'For region %r, none of %r exists' % (r.region_code, paths))
          else:
            logging.warn('For region %r, none of %r exists; '
                         'just a warning since this region is not confirmed',
                         r.region_code, paths)

  def testYAMLOutput(self):
    output = StringIO.StringIO()
    regions.main(['--format', 'yaml'], output)
    data = yaml.load(output.getvalue())
    self.assertEquals(
        {'keyboards': ['xkb:us::eng'],
         'keyboard_mechanical_layout': 'ANSI',
         'locales': ['en-US'],
         'region_code': 'us',
         'numeric_id': 29,
         'description': 'United States',
         'time_zones': ['America/Los_Angeles']},
        data['us'])

  def testFieldsDict(self):
    # 'description' and 'notes' should be missing.
    self.assertEquals(
        {'keyboards': ['xkb:b::b'],
         'keyboard_mechanical_layout': 'e',
         'description': 'description',
         'locales': ['d'],
         'numeric_id': 11,
         'region_code': 'a',
         'time_zones': ['c']},
        (regions.Region('a', 'xkb:b::b', 'c', 'd', 'e', 'description', 'notes',
                        11).GetFieldsDict()))

  def testConsolidateRegionsDups(self):
    """Test duplicate handling.  Two identical Regions are OK."""
    # Make two copies of the same region.
    region_list = [regions.Region('a', 'xkb:b::b', 'c', 'd', 'e')
                   for _ in range(2)]
    # It's OK.
    self.assertEquals(
        {'a': region_list[0]}, regions.ConsolidateRegions(region_list))

    # Modify the second copy.
    region_list[1].keyboards = ['f']
    # Not OK anymore!
    self.assertRaisesRegexp(
        regions.RegionException, "Conflicting definitions for region 'a':",
        regions.ConsolidateRegions, region_list)

  def testNumericIds(self):
    """Make sure numeric IDs are unique and all regions have a numeric ID."""
    numeric_ids = set()
    for region in regions.BuildRegionsDict(include_all=True).values():
      if region.numeric_id is not None:
        self.assertNotIn(region.numeric_id, numeric_ids,
                         'Duplicate numeric ID %d in %s' % (
                             region.numeric_id, region.region_code))
        numeric_ids.add(region.numeric_id)

      # Confirmed regions only
      if region.region_code in regions.REGIONS:
        self.assertIsNotNone(region.numeric_id,
                             'Region %s has no numeric ID assigned' % (
                                 region.region_code))

if __name__ == '__main__':
  unittest.main()
