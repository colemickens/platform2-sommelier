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
                '<(DEPTH)/attestation/attestation.gyp:*',
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
            ['USE_feedback == 1', {
              'dependencies': [
                '<(DEPTH)/feedback/feedback.gyp:*',
              ],
            }],
            ['USE_lorgnette == 1', {
              'dependencies': [
                '<(DEPTH)/lorgnette/lorgnette.gyp:*',
              ],
            }],
            ['USE_profile == 1', {
              'dependencies': [
                '<(DEPTH)/chromiumos-wide-profiling/quipper.gyp:*',
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
    },
    {
      # Add a fake action rule so that in the absence of any targets needing to
      # be built, we don't generate an empty ninja build rules file.
      'target_name': 'phony',
      'type': 'none',
      'actions': [{
        'inputs': [],
        # "outputs" can not be empty. Set it to refer to this gyp file so
        # ninja never actually runs this rule.
        'outputs': [
          'platform.gyp'
        ],
        'action_name': 'phony',
        'action': ['true']
      }],
    },
  ]
}
