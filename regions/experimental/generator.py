#!/usr/bin/env python
"""Region database generator from public documents."""

import pycountry

# Load existing database.
import regions


def LoadZoneTable():
  # zone.tab from http://www.iana.org/time-zones
  zones = {}
  with open('zone.tab') as f:
    for line in f:
      if line.startswith('#'):
        continue
      # format: code coordinates TZ comments
      code, unused, tz = line.split('\t')[0:3]
      if code not in zones:
        zones[code] = []
      zones[code] += [tz.strip()]
  return zones


def LoadCountryTable(tzinfo):
  # http://download.geonames.org/export/dump/countryInfo.txt
  regions = {}
  skip_regions = (['BV', 'HM', 'CS', 'XK', 'AN'] +  # reserved regions
                  ['HM', 'AQ'] +  # broken data
                  ['CA'])  # We only have ca.variants in region database.

  with open('countryInfo.txt') as f:
    for line in f:
      if line.startswith('#'):
        continue

      # format: [0]ISO3166-1a2, [4]Country, [5]Capital, [15]Languages
      data = line.split('\t')
      code = data[0]
      if code in skip_regions:
        continue
      if code not in tzinfo:
        exit('Region code <%s> has no timezone info!' % code)

      # Try to build XKB.
      kbd_region = code.lower()
      lang = data[15].partition(',')[0].partition('-')[0]
      if len(lang) != 2:
        lang = 'en'
        kbd_region = 'us'

      regions[code] = dict(
          region_code=code.lower(),
          description=data[4],
          keyboards=str('xkb:%s::%s' % (
              kbd_region, pycountry.languages.get(alpha2=lang).terminology)),
          time_zones=tzinfo[code],
          locales=data[15].strip(',').split(','),
      )
  return regions


def main():
  def flat(v):
    if type(v) is list and len(v) == 1:
      return v[0]
    return v

  tzinfo = LoadZoneTable()
  base = max(r.numeric_id for r in regions.REGIONS.values()) + 1
  for region in LoadCountryTable(tzinfo).values():
    code = region['region_code']
    if code in regions.REGIONS:
      continue
    # Region takes: code, kbd, tz, locale, layout, description, comment, numid.
    print ("Region('%s', %r, %r, %r, _KML.ANSI, '%s')," %
           (code, region['keyboards'], flat(region['time_zones']),
            flat(region['locales']), region['description']))
    base += 1


if __name__ == '__main__':
  main()
