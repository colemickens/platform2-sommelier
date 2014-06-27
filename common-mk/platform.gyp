# File format documentation:
# https://code.google.com/p/gyp/wiki/InputFormatReference
{
  'targets': [
    {
      'target_name': 'All',
      'type': 'none',
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
                '<(DEPTH)/platform2/cromo/cromo.gyp:*',
                '<(DEPTH)/platform2/mist/mist.gyp:*',
              ],
            }],
            ['USE_crash_reporting == 1', {
              'dependencies': [
                '<(DEPTH)/platform2/crash-reporter/crash-reporter.gyp:*',
              ],
            }],
            ['USE_cros_disks == 1', {
              'dependencies': [
                '<(DEPTH)/platform2/cros-disks/cros-disks.gyp:*',
              ],
            }],
            ['USE_debugd == 1', {
              'dependencies': [
                '<(DEPTH)/platform2/debugd/debugd.gyp:*',
              ],
            }],
            ['USE_feedback == 1', {
              'dependencies': [
                '<(DEPTH)/platform2/feedback/feedback.gyp:*',
              ],
            }],
            ['USE_gdmwimax == 1', {
              'dependencies': [
                '<(DEPTH)/platform2/wimax_manager/wimax_manager.gyp:*',
              ],
            }],
            ['USE_lorgnette == 1', {
              'dependencies': [
                '<(DEPTH)/platform/lorgnette/lorgnette.gyp:*',
              ],
            }],
            ['USE_power_management == 1', {
              'dependencies': [
                '<(DEPTH)/platform/power_manager/power_manager.gyp:*',
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
                '<(DEPTH)/platform2/vpn-manager/vpn-manager.gyp:*',
              ],
            }],
          ],
        }],
      ],
    }
  ],
}
