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
        'libcros_config/fake_cros_config.cc',
      ],
      'link_settings': {
        'libraries': [
          '-lfdt',
        ],
      },
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'cros_config_unittest',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'include_dirs': [
            'libcros_config',
          ],
          'dependencies': [
            'libcros_config',
          ],
          'sources': [
            'libcros_config/cros_config_unittest.cc',
          ],
        },
      ],
    }],
  ],
}
