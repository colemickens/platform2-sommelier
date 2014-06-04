{
  'targets': [
    {
      'target_name': 'libpolicy-includes',
      'type': 'none',
      'copies': [
        {
          'destination': '<(SHARED_INTERMEDIATE_DIR)/include/policy',
          'files': [
            'chromeos/policy/device_policy.h',
            'chromeos/policy/device_policy_impl.h',
            'chromeos/policy/libpolicy.h',
            'chromeos/policy/mock_libpolicy.h',
            'chromeos/policy/mock_device_policy.h',
          ],
        },
      ],
    },
  ],
}
