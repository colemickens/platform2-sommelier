#!/usr/bin/python2
# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Authoritative source for Chromium OS region/locale configuration.

Run this module to display all known regions (use --help to see options).
"""

from __future__ import print_function

import argparse
import collections
import json
import re
import sys

import yaml


# The regular expression to check values in Region.keyboards and Region.locales.
# Keyboards should come with xkb: protocol, or the input methods (ime:, m17n:).
# Examples: xkb:us:intl:eng, ime:ime:zh-t:cangjie
KEYBOARD_PATTERN = re.compile(r'^xkb:\w+:\w*:\w+$|'
                              r'^(ime|m17n|t13n):[\w:-]+$')
# Locale should be a combination of language and location.
# Examples: en-US, ja.
LOCALE_PATTERN = re.compile(r'^(\w+)(-[A-Z0-9]+)?$')


class Enum(frozenset):
  """An enumeration type.

  Usage:
    To create a enum object:
      dummy_enum = Enum(['A', 'B', 'C'])

    To access a enum object, use:
      dummy_enum.A
      dummy_enum.B
  """

  def __getattr__(self, name):
    if name in self:
      return name
    raise AttributeError


class RegionException(Exception):
  """Exception in Region handling."""
  pass


def MakeList(value):
  """Converts the given value to a list.

  Returns:
    A list of elements from "value" if it is iterable (except string);
    otherwise, a list contains only one element.
  """
  if (isinstance(value, collections.Iterable) and
      not isinstance(value, basestring)):
    return list(value)
  return [value]


class Region(object):
  """Comprehensive, standard locale configuration per country/region.

  See :ref:`regions-values` for detailed information on how to set these values.
  """
  # pylint gets confused by some of the docstrings.
  # pylint: disable=C0322

  # ANSI = US-like
  # ISO = UK-like
  # JIS = Japanese
  # KS = Korean
  # ABNT2 = Brazilian (like ISO but with an extra key to the left of the
  #   right shift key)
  KeyboardMechanicalLayout = Enum(['ANSI', 'ISO', 'JIS', 'KS', 'ABNT2'])

  region_code = None
  """A unique identifier for the region.  This may be a lower-case
  `ISO 3166-1 alpha-2 code
  <http://en.wikipedia.org/wiki/ISO_3166-1_alpha-2>`_ (e.g., ``us``),
  a variant within an alpha-2 entity (e.g., ``ca.fr``), or an
  identifier for a collection of countries or entities (e.g.,
  ``latam-es-419`` or ``nordic``).  See :ref:`region-codes`.

  Note that ``uk`` is not a valid identifier; ``gb`` is used as it is
  the real alpha-2 code for the UK."""

  keyboards = None
  """A list of keyboard layout identifiers (e.g., ``xkb:us:intl:eng``
  or ``m17n:ar``). This field was designed to be the physical keyboard layout
  in the beginning, and then becomes a list of OOBE keyboard selection, which
  then includes non-physical layout elements like input methods (``ime:``).
  To avoid confusion, physical layout is now defined by
  :py:attr:`keyboard_mechanical_layout`, and this is reserved for logical
  layouts.

  This is identical to the legacy VPD ``keyboard_layout`` value."""

  time_zones = None
  """A list of default `tz database time zone
  <http://en.wikipedia.org/wiki/List_of_tz_database_time_zones>`_
  identifiers (e.g., ``America/Los_Angeles``). See
  `timezone_settings.cc <http://goo.gl/WSVUeE>`_ for supported time
  zones.

  This is identical to the legacy VPD ``initial_timezone`` value."""

  locales = None
  """A list of default locale codes (e.g., ``en-US``); see
  `l10n_util.cc <http://goo.gl/kVkht>`_ for supported locales.

  This is identital to the legacy VPD ``initial_locale`` field."""

  keyboard_mechanical_layout = None
  """The keyboard's mechanical layout (``ANSI`` [US-like], ``ISO``
  [UK-like], ``JIS`` [Japanese], ``ABNT2`` [Brazilian] or ``KS`` [Korean])."""

  description = None
  """A human-readable description of the region.
  This defaults to :py:attr:`region_code` if not set."""

  notes = None
  """Implementation notes about the region.  This may be None."""

  numeric_id = None
  """An integer for mapping into Chrome OS HWID.
  Please never change this once it is assigned."""

  regulatory_domain = None
  """An ISO 3166-1 alpha 2 upper-cased two-letter region code for setting
  Wireless regulatory. See crosbug.com/p/38745 for more details.

  When omitted, this will derive from region_code."""

  FIELDS = ['region_code', 'keyboards', 'time_zones', 'locales', 'description',
            'keyboard_mechanical_layout', 'numeric_id', 'regulatory_domain']
  """Names of fields that define the region."""


  def __init__(self, region_code, keyboards, time_zones, locales,
               keyboard_mechanical_layout, description=None, notes=None,
               numeric_id=None, regdomain=None):
    """Constructor.

    Args:
      region_code: See :py:attr:`region_code`.
      keyboards: See :py:attr:`keyboards`.  A single string is accepted for
        backward compatibility.
      time_zones: See :py:attr:`time_zones`.
      locales: See :py:attr:`locales`.  A single string is accepted
        for backward compatibility.
      keyboard_mechanical_layout: See :py:attr:`keyboard_mechanical_layout`.
      description: See :py:attr:`description`.
      notes: See :py:attr:`notes`.
      numeric_id: See :py:attr:`numeric_id`.  This must be None or a
        non-negative integer.
      regdomain: See :py:attr:`regulatory_domain`.
    """

    def regdomain_from_region(region):
      if region.find('.') >= 0:
        region = region[:region.index('.')]
      if len(region) == 2:
        return region.upper()
      return None

    # Quick check: should be 'gb', not 'uk'
    if region_code == 'uk':
      raise RegionException("'uk' is not a valid region code (use 'gb')")

    self.region_code = region_code
    self.keyboards = MakeList(keyboards)
    self.time_zones = MakeList(time_zones)
    self.locales = MakeList(locales)
    self.keyboard_mechanical_layout = keyboard_mechanical_layout
    self.description = description or region_code
    self.notes = notes
    self.numeric_id = numeric_id
    self.regulatory_domain = (regdomain or regdomain_from_region(region_code))

    if self.numeric_id is not None:
      if not isinstance(self.numeric_id, int):
        raise TypeError('Numeric ID is %r but should be an integer' %
                        (self.numeric_id,))
      if self.numeric_id < 0:
        raise ValueError('Numeric ID is %r but should be non-negative' %
                         self.numeric_id)

    for f in (self.keyboards, self.locales):
      assert all(isinstance(x, str) for x in f), (
          'Expected a list of strings, not %r' % f)
    for f in self.keyboards:
      assert KEYBOARD_PATTERN.match(f), (
          'Keyboard pattern %r does not match %r' % (
              f, KEYBOARD_PATTERN.pattern))
    for f in self.locales:
      assert LOCALE_PATTERN.match(f), (
          'Locale %r does not match %r' % (
              f, LOCALE_PATTERN.pattern))
    assert (self.regulatory_domain and
            len(self.regulatory_domain) == 2 and
            self.regulatory_domain.upper() == self.regulatory_domain), (
                "Regulatory domain settings error for region %s" % region_code)

  def __repr__(self):
    return 'Region(%s)' % (', '.join([getattr(self, x) for x in self.FIELDS]))

  def GetFieldsDict(self):
    """Returns a dict of all substantive fields.

    notes and description are excluded.
    """
    return dict((k, getattr(self, k)) for k in self.FIELDS)

_KML = Region.KeyboardMechanicalLayout
REGIONS_LIST = [
    Region('au', 'xkb:us::eng', 'Australia/Sydney', 'en-AU', _KML.ANSI,
           'Australia', None, 1),
    Region('be', 'xkb:be::nld', 'Europe/Brussels', 'en-GB', _KML.ISO, 'Belgium',
           'Flemish (Belgian Dutch) keyboard; British English language for '
           'neutrality', 2),
    Region('br', 'xkb:br::por', 'America/Sao_Paulo', 'pt-BR', _KML.ABNT2,
           'Brazil (ABNT2)',
           ('ABNT2 = ABNT NBR 10346 variant 2. This is the preferred layout '
            'for Brazil. ABNT2 is mostly an ISO layout, but it 12 keys between '
            'the shift keys; see http://goo.gl/twA5tq'), 3),
    Region('br.abnt', 'xkb:br::por', 'America/Sao_Paulo', 'pt-BR', _KML.ISO,
           'Brazil (ABNT)',
           ('Like ABNT2, but lacking the extra key to the left of the right '
            'shift key found in that layout. ABNT2 (the "br" region) is '
            'preferred to this layout'), 4),
    Region('br.usintl', 'xkb:us:intl:eng', 'America/Sao_Paulo', 'pt-BR',
           _KML.ANSI, 'Brazil (US Intl)',
           'Brazil with US International keyboard layout. ABNT2 ("br") and '
           'ABNT1 ("br.abnt1 ") are both preferred to this.', 5),
    Region('ca.ansi', 'xkb:us::eng', 'America/Toronto', 'en-CA', _KML.ANSI,
           'Canada (US keyboard)',
           'Canada with US (ANSI) keyboard. Not for en/fr hybrid ANSI '
           'keyboards; for that you would want ca.hybridansi. See '
           'http://goto/cros-canada', 6),
    Region('ca.fr', 'xkb:ca::fra', 'America/Toronto', 'fr-CA', _KML.ISO,
           'Canada (French keyboard)',
           ('Canadian French (ISO) keyboard. The most common configuration for '
            'Canadian French SKUs.  See http://goto/cros-canada'), 7),
    Region('ca.hybrid', 'xkb:ca:eng:eng', 'America/Toronto', 'en-CA', _KML.ISO,
           'Canada (hybrid ISO)',
           ('Canada with hybrid (ISO) xkb:ca:eng:eng + xkb:ca::fra keyboard, '
            'defaulting to English language and keyboard.  Used only if there '
            'needs to be a single SKU for all of Canada.  See '
            'http://goto/cros-canada'), 8),
    Region('ca.hybridansi', 'xkb:ca:eng:eng', 'America/Toronto', 'en-CA',
           _KML.ANSI, 'Canada (hybrid ANSI)',
           ('Canada with hybrid (ANSI) xkb:ca:eng:eng + xkb:ca::fra keyboard, '
            'defaulting to English language and keyboard.  Used only if there '
            'needs to be a single SKU for all of Canada.  See '
            'http://goto/cros-canada'), 9),
    Region('ca.multix', 'xkb:ca:multix:fra', 'America/Toronto', 'fr-CA',
           _KML.ISO, 'Canada (multilingual)',
           ("Canadian Multilingual keyboard; you probably don't want this. See "
            'http://goto/cros-canada'), 10),
    Region('ch', 'xkb:ch::ger', 'Europe/Zurich', 'en-US', _KML.ISO,
           'Switzerland',
           'German keyboard, but US English to be language-neutral; used in '
           'the common case that there is only a single Swiss SKU.', 11),
    Region('de', 'xkb:de::ger', 'Europe/Berlin', 'de', _KML.ISO, 'Germany',
           None, 12),
    Region('es', 'xkb:es::spa', 'Europe/Madrid', 'es', _KML.ISO, 'Spain',
           None, 13),
    Region('fi', 'xkb:fi::fin', 'Europe/Helsinki', 'fi', _KML.ISO, 'Finland',
           None, 14),
    Region('fr', 'xkb:fr::fra', 'Europe/Paris', 'fr', _KML.ISO, 'France',
           None, 15),
    Region('gb', 'xkb:gb:extd:eng', 'Europe/London', 'en-GB', _KML.ISO, 'UK',
           None, 16),
    Region('ie', 'xkb:gb:extd:eng', 'Europe/Dublin', 'en-GB', _KML.ISO,
           'Ireland', None, 17),
    Region('in', 'xkb:us::eng', 'Asia/Calcutta', 'en-US', _KML.ANSI, 'India',
           None, 18),
    Region('it', 'xkb:it::ita', 'Europe/Rome', 'it', _KML.ISO, 'Italy',
           None, 19),
    Region('latam-es-419', 'xkb:es::spa', 'America/Mexico_City', 'es-419',
           _KML.ISO, 'Hispanophone Latin America',
           ('Spanish-speaking countries in Latin America, using the Iberian '
            '(Spain) Spanish keyboard, which is increasingly dominant in '
            'Latin America. Known to be correct for '
            'Chile, Colombia, Mexico, Peru; '
            'still unconfirmed for other es-419 countries. The old Latin '
            'American layout (xkb:latam::spa) has not been approved; before '
            'using that you must seek review through http://goto/vpdsettings. '
            'See also http://goo.gl/Iffuqh. Note that 419 is the UN M.49 '
            'region code for Latin America'), 20, 'MX'),
    Region('my', 'xkb:us::eng', 'Asia/Kuala_Lumpur', 'ms', _KML.ANSI,
           'Malaysia', None, 21),
    Region('nl', 'xkb:us:intl:eng', 'Europe/Amsterdam', 'nl', _KML.ANSI,
           'Netherlands', None, 22),
    Region('nordic', 'xkb:se::swe', 'Europe/Stockholm', 'en-US', _KML.ISO,
           'Nordics',
           ('Unified SKU for Sweden, Norway, and Denmark.  This defaults '
            'to Swedish keyboard layout, but starts with US English language '
            'for neutrality.  Use if there is a single combined SKU for Nordic '
            'countries.'), 23, 'SE'),
    Region('nz', 'xkb:us::eng', 'Pacific/Auckland', 'en-NZ', _KML.ANSI,
           'New Zealand', None, 24),
    Region('ph', 'xkb:us::eng', 'Asia/Manila', 'en-US', _KML.ANSI,
           'Philippines', None, 25),
    Region('ru', 'xkb:ru::rus', 'Europe/Moscow', 'ru', _KML.ANSI, 'Russia',
           'For R31+ only; R30 and earlier must use US keyboard for login', 26),
    Region('se', 'xkb:se::swe', 'Europe/Stockholm', 'sv', _KML.ISO, 'Sweden',
           ('Use this if there separate SKUs for Nordic countries (Sweden, '
            'Norway, and Denmark), or the device is only shipping to Sweden. '
            "If there is a single unified SKU, use 'nordic' instead."), 27),
    Region('sg', 'xkb:us::eng', 'Asia/Singapore', 'en-GB', _KML.ANSI,
           'Singapore', None, 28),
    Region('us', 'xkb:us::eng', 'America/Los_Angeles', 'en-US', _KML.ANSI,
           'United States', None, 29),
    Region('jp', 'xkb:jp::jpn', 'Asia/Tokyo', 'ja', _KML.JIS,
           'Japan', None, 30),
    Region('za', 'xkb:us:intl:eng', 'Africa/Johannesburg', 'en-ZA', _KML.ANSI,
           'South Africa', None, 31),
    Region('hk',
           ['xkb:us::eng', 'ime:zh-t:cangjie', 'ime:zh-t:quick',
            'ime:zh-t:array', 'ime:zh-t:dayi', 'ime:zh-t:zhuyin',
            'ime:zh-t:pinyin'],
           'Asia/Hong_Kong', ['zh-TW', 'en-GB', 'zh-CN'], _KML.ANSI,
           'Hong Kong', None, 33),
    Region('cz', ['xkb:cz::cze', 'xkb:cz:qwerty:cze'], 'Europe/Prague',
           ['cs', 'en-GB'], _KML.ISO, 'Czech Republic', None, 35),
    Region('th', ['xkb:us::eng', 'm17n:th', 'm17n:th_pattajoti', 'm17n:th_tis'],
           'Asia/Bangkok', ['th', 'en-GB'], _KML.ANSI, 'Thailand', None, 36),
    Region('tw',
           ['xkb:us::eng', 'ime:zh-t:zhuyin', 'ime:zh-t:array',
            'ime:zh-t:dayi', 'ime:zh-t:cangjie', 'ime:zh-t:quick',
            'ime:zh-t:pinyin'],
           'Asia/Taipei', ['zh-TW', 'en-US'], _KML.ANSI, 'Taiwan', None, 38),
    Region('pl', 'xkb:pl::pol', 'Europe/Warsaw', ['pl', 'en-GB'], _KML.ANSI,
           'Poland', None, 39),
]
"""A list of :py:class:`regions.Region` objects for
all **confirmed** regions.  A confirmed region is a region whose
properties are known to be correct and may be used to launch a device."""


UNCONFIRMED_REGIONS_LIST = []
"""A list of :py:class:`regions.Region` objects for
**unconfirmed** regions. These are believed to be correct but
unconfirmed, and all fields should be verified (and the row moved into
:py:data:`regions.Region.REGIONS_LIST`) before
launch. See <http://goto/vpdsettings>.

Currently, non-Latin keyboards must use an underlying Latin keyboard
for VPD. (This assumption should be revisited when moving items to
:py:data:`regions.Region.REGIONS_LIST`.)  This is
currently being discussed on <http://crbug.com/325389>.

Some timezones may be missing from ``timezone_settings.cc`` (see
http://crosbug.com/p/23902).  This must be rectified before moving
items to :py:data:`regions.Region.REGIONS_LIST`.
"""

INCOMPLETE_REGIONS_LIST = []
"""A list of :py:class:`regions.Region` objects for
**incomplete** regions.  These may contain incorrect information, and all
fields must be reviewed before launch. See http://goto/vpdsettings.
"""


def ConsolidateRegions(regions):
  """Consolidates a list of regions into a dict.

  Args:
    regions: A list of Region objects.  All objects for any given
      region code must be identical or we will throw an exception.
      (We allow duplicates in case identical region objects are
      defined in both regions.py and the overlay, e.g., when moving
      items to the public overlay.)

  Returns:
    A dict from region code to Region.

  Raises:
    RegionException: If there are multiple regions defined for a given
      region, and the values for those regions differ.
  """
  # Build a dict from region_code to the first Region with that code.
  region_dict = {}
  for r in regions:
    existing_region = region_dict.get(r.region_code)
    if existing_region:
      if existing_region.GetFieldsDict() != r.GetFieldsDict():
        raise RegionException(
            'Conflicting definitions for region %r: %r, %r' %
            (r.region_code, existing_region.GetFieldsDict(),
             r.GetFieldsDict()))
    else:
      region_dict[r.region_code] = r

  return region_dict


def BuildRegionsDict(include_all=False):
  """Builds a dictionary mapping from code to :py:class:`regions.Region` object.

  The regions include:

  * :py:data:`regions.REGIONS_LIST`
  * :py:data:`regions_overlay.REGIONS_LIST`
  * Only if ``include_all`` is true:

    * :py:data:`regions.UNCONFIRMED_REGIONS_LIST`
    * :py:data:`regions.INCOMPLETE_REGIONS_LIST`

  A region may only appear in one of the above lists, or this function
  will (deliberately) fail.
  """
  regions = list(REGIONS_LIST)
  if include_all:
    regions += UNCONFIRMED_REGIONS_LIST + INCOMPLETE_REGIONS_LIST

  # Build dictionary of region code to list of regions with that
  # region code.  Check manually for duplicates, since the region may
  # be present both in the overlay and the public repo.
  return ConsolidateRegions(regions)


REGIONS = BuildRegionsDict()


def main(args=sys.argv[1:], out=None):
  parser = argparse.ArgumentParser(description=(
      'Display all known regions and their parameters. '))
  parser.add_argument('--format',
                      choices=('human-readable', 'csv', 'json', 'yaml'),
                      default='human-readable',
                      help='Output format (default=%(default)s)')
  parser.add_argument('--all', action='store_true',
                      help='Include unconfirmed and incomplete regions')
  parser.add_argument('--output', default=None,
                      help='Specify output file')
  args = parser.parse_args(args)

  regions_dict = BuildRegionsDict(args.all)
  if out is None:
    if args.output is None:
      out = sys.stdout
    else:
      out = open(args.output, 'w')

  # Handle YAML and JSON output.
  if args.format == 'yaml' or args.format == 'json':
    data = {}
    for region in regions_dict.values():
      item = {}
      for field in Region.FIELDS:
        item[field] = getattr(region, field)
      data[region.region_code] = item
    if args.format == 'yaml':
      yaml.dump(data, out)
    else:
      json.dump(data, out)
    return

  # Handle CSV or plain-text output: build a list of lines to print.
  lines = [Region.FIELDS]

  def CoerceToString(value):
    """Returns the arguments in simple string type.

    If value is a list, concatenate its values with commas.  Otherwise, just
    return value.
    """
    if isinstance(value, list):
      return ','.join(value)
    else:
      return str(value)
  for region in sorted(regions_dict.values(), key=lambda v: v.region_code):
    lines.append([CoerceToString(getattr(region, field))
                  for field in Region.FIELDS])

  if args.format == 'csv':
    # Just print the lines in CSV format.
    for l in lines:
      print(','.join(l))
  elif args.format == 'human-readable':
    num_columns = len(lines[0])

    # Calculate maximum length of each column.
    max_lengths = []
    for column_no in xrange(num_columns):
      max_lengths.append(max(len(line[column_no]) for line in lines))

    # Print each line, padding as necessary to the max column length.
    for line in lines:
      for column_no in xrange(num_columns):
        out.write(line[column_no].ljust(max_lengths[column_no] + 2))
      out.write('\n')
  else:
    exit('Sorry, unknown format specified: %s' % args.format)


if __name__ == '__main__':
  main()
