{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
      ],
    },
  },
  'targets' : [
    {
      'target_name': 'libcros_config',
      'type': 'shared_library',
      'cflags': [
        '-fvisibility=default',
      ],
      'includes': ['../common-mk/common_test.gypi'],
      'sources': [
        'libcros_config/cros_config.cc',
        'libcros_config/fake_cros_config.cc',
      ],
    },
    {
      'target_name': 'cros_config_unittest',
      'type': 'executable',
      'include_dirs': [
        'libcros_config',
      ],
      'dependencies': [
        'libcros_config',
      ],
      'ldflags': [
        '-lfdt',
      ],
      'sources': [
        'libcros_config/cros_config_unittest.cc',
      ]
    }
  ],
}
