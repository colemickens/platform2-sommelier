{
  'targets': [
    {
      'target_name': 'All',
      'type': 'none',
      'dependencies': [
        '<(DEPTH)/libchromeos/libchromeos-<(libbase_ver).gyp:*',
        '<(DEPTH)/metrics/metrics.gyp:*',
        '<(DEPTH)/system_api/system_api.gyp:*',
      ],
      'conditions': [
        ['USE_cros_host == 0', {
          'dependencies': [
            '<(DEPTH)/chaps/chaps.gyp:*',
            '<(DEPTH)/debugd/debugd.gyp:*',
            '<(DEPTH)/shill/shill.gyp:*',
          ],
          'conditions': [
            ['USE_cellular == 1', {
              'dependencies': [
                '<(DEPTH)/cromo/cromo.gyp:*',
                '<(DEPTH)/mist/mist.gyp:*',
              ],
            }],
            ['USE_cros_disks == 1', {
              'dependencies': [
                '<(DEPTH)/cros-disks/cros-disks.gyp:*',
              ],
            }],
            ['USE_gdmwimax == 1', {
              'dependencies': [
                '<(DEPTH)/wimax_manager/wimax_manager.gyp:*',
              ],
            }],
            ['USE_vpn == 1', {
              'dependencies': [
                '<(DEPTH)/vpn-manager/vpn-manager.gyp:*',
              ],
            }],
          ],
        }],
      ],
    }
  ],
}
