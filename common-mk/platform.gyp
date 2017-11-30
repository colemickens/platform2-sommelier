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
            ['USE_feedback == 1', {
              'dependencies': [
                '<(DEPTH)/feedback/feedback.gyp:*',
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
          'platform.gyp',
        ],
        'action_name': 'phony',
        'action': ['true'],
      }],
    },
  ],
}
