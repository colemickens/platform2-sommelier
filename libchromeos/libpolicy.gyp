{
  'targets': [
    {
      'target_name': 'libpolicy-protos',
      'type': 'static_library',
      'variables': {
        'proto_in_dir': '<(sysroot)/usr/include/proto',
        'proto_out_dir': 'include/bindings',
      },
      'cflags': [
        '-fvisibility=hidden',
      ],
      'sources': [
        '<(proto_in_dir)/chrome_device_policy.proto',
        '<(proto_in_dir)/device_management_backend.proto',
      ],
      'includes': ['../common-mk/protoc.gypi'],
    },
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
            'chromeos/policy/mock_device_policy.h',
          ],
        },
      ],
    },
  ],
}
