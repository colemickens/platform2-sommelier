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
      'sources': [
        'libcros_config/cros_config.cc',
        'libcros_config/cros_config_impl.cc',
        'libcros_config/cros_config_json.cc',
        'libcros_config/fake_cros_config.cc',
        'libcros_config/identity.cc',
        'libcros_config/identity_arm.cc',
        'libcros_config/identity_x86.cc',
      ],
      'link_settings': {
        'libraries': [
          '-lfdt',
        ],
      },
    },
    {
      'target_name': 'cros_config',
      'type': 'executable',
      'variables': {
        'deps': [
          'libbrillo-<(libbase_ver)',
        ],
      },
      'dependencies': ['libcros_config'],
      'sources': [
        'cros_config_main.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'cros_config_test',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'include_dirs': [
            'libcros_config',
          ],
          'dependencies': [
            'libcros_config',
          ],
          'sources': [
            'libcros_config/cros_config_test.cc',
          ],
        },
        {
          'target_name': 'cros_config_main_test',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'dependencies': [
            'cros_config',
          ],
          'sources': [
            'cros_config_main_test.cc',
          ],
        },
        {
          'target_name': 'fake_cros_config_test',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'include_dirs': [
            'libcros_config',
          ],
          'dependencies': [
            'libcros_config',
          ],
          'sources': [
            'libcros_config/fake_cros_config_test.cc',
          ],
        },
      ],
    }],
  ],
}
