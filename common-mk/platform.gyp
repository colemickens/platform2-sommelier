# File format documentation:
# https://code.google.com/p/gyp/wiki/InputFormatReference
{
  'targets': [
    {
      'target_name': 'All',
      'type': 'none',
      'dependencies': [
        '<(DEPTH)/platform/libchromeos/libchromeos-271506.gyp:*',
        '<(DEPTH)/platform/metrics/libmetrics-271506.gyp:*',
        '<(DEPTH)/platform/metrics/metrics.gyp:*',
        '<(DEPTH)/platform/power_manager/power_manager.gyp:*',
        '<(DEPTH)/platform/system_api/system_api.gyp:*',
      ],
      'conditions': [
        ['USE_cros_host == 0', {
          'conditions': [
            ['USE_attestation == 1', {
              'dependencies': [
                '<(DEPTH)/platform2/attestation/attestation.gyp:*',
              ],
            }],
            ['USE_buffet == 1', {
              'dependencies': [
                '<(DEPTH)/platform2/buffet/buffet.gyp:*',
              ],
            }],
            ['USE_cellular == 1', {
              'dependencies': [
                '<(DEPTH)/platform/cromo/cromo.gyp:*',
                '<(DEPTH)/platform/mist/mist.gyp:*',
              ],
            }],
            ['USE_crash_reporting == 1', {
              'dependencies': [
                '<(DEPTH)/platform/crash-reporter/crash-reporter.gyp:*',
              ],
            }],
            ['USE_cros_disks == 1', {
              'dependencies': [
                '<(DEPTH)/platform/cros-disks/cros-disks.gyp:*',
              ],
            }],
            ['USE_debugd == 1', {
              'dependencies': [
                '<(DEPTH)/platform/debugd/debugd.gyp:*',
              ],
            }],
            ['USE_feedback == 1', {
              'dependencies': [
                '<(DEPTH)/platform2/feedback/feedback.gyp:*',
              ],
            }],
            ['USE_gdmwimax == 1', {
              'dependencies': [
                '<(DEPTH)/platform/wimax_manager/wimax_manager.gyp:*',
              ],
            }],
            ['USE_lorgnette == 1', {
              'dependencies': [
                '<(DEPTH)/platform/lorgnette/lorgnette.gyp:*',
              ],
            }],
            ['USE_profile == 1', {
              'dependencies': [
                '<(DEPTH)/platform/chromiumos-wide-profiling/quipper.gyp:*',
              ],
            }],
            ['USE_shill == 1', {
              'dependencies': [
                '<(DEPTH)/platform/shill/shill.gyp:*',
              ],
            }],
            ['USE_tpm == 1', {
              'dependencies': [
                '<(DEPTH)/platform/chaps/chaps.gyp:*',
              ],
            }],
            ['USE_vpn == 1', {
              'dependencies': [
                '<(DEPTH)/platform/vpn-manager/vpn-manager.gyp:*',
              ],
            }],
          ],
        }],
      ],
    }
  ],
}
