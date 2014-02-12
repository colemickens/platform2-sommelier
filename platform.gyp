# File format documentation:
# https://code.google.com/p/gyp/wiki/InputFormatReference
{
  'targets': [
    {
      'target_name': 'All',
      'type': 'none',
      'dependencies': [
        '<(DEPTH)/libchromeos/libchromeos-180609.gyp:*',
        '<(DEPTH)/libchromeos/libchromeos-242728.gyp:*',
        '<(DEPTH)/metrics/libmetrics-180609.gyp:*',
        '<(DEPTH)/metrics/libmetrics-242728.gyp:*',
        '<(DEPTH)/metrics/metrics.gyp:*',
        '<(DEPTH)/power_manager/power_manager.gyp:*',
        '<(DEPTH)/system_api/system_api.gyp:*',
      ],
      'conditions': [
        ['USE_cros_host == 0', {
          'conditions': [
            ['USE_cellular == 1', {
              'dependencies': [
                '<(DEPTH)/cromo/cromo.gyp:*',
                '<(DEPTH)/mist/mist.gyp:*',
              ],
            }],
            ['USE_crash_reporting == 1', {
              'dependencies': [
                '<(DEPTH)/crash-reporter/crash-reporter.gyp:*',
              ],
            }],
            ['USE_cros_disks == 1', {
              'dependencies': [
                '<(DEPTH)/cros-disks/cros-disks.gyp:*',
              ],
            }],
            ['USE_debugd == 1', {
              'dependencies': [
                '<(DEPTH)/debugd/debugd.gyp:*',
              ],
            }],
            ['USE_gdmwimax == 1', {
              'dependencies': [
                '<(DEPTH)/wimax_manager/wimax_manager.gyp:*',
              ],
            }],
            ['USE_profile == 1', {
              'dependencies': [
                '<(DEPTH)/chromiumos-wide-profiling/quipper.gyp:*',
              ],
            }],
            ['USE_shill == 1', {
              'dependencies': [
                '<(DEPTH)/shill/shill.gyp:*',
              ],
            }],
            ['USE_tpm == 1', {
              'dependencies': [
                '<(DEPTH)/chaps/chaps.gyp:*',
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
