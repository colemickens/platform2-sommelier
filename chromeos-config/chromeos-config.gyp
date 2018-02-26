{
  'variables': {
    'USE_json%': '0',
  },
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
        'libcros_config/cros_config_fdt.cc',
        'libcros_config/cros_config_impl.cc',
        'libcros_config/fake_cros_config.cc',
        'libcros_config/identity.cc',
      ],
      'conditions': [
        ['USE_json == 1', {
          'sources': [
            'libcros_config/cros_config_json.cc',
          ],
          'defines': [
            'USE_JSON',
          ],
          'sources!': [
            'libcros_config/cros_config.cc',
          ]},
        ],
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
          'target_name': 'cros_config_unittest',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'include_dirs': [
            'libcros_config',
          ],
          'dependencies': [
            'libcros_config',
          ],
          'conditions': [
            ['USE_json == 1', {
              'defines': [
                'USE_JSON',
              ]},
            ],
          ],
          'sources': [
            'libcros_config/cros_config_unittest.cc',
          ],
        },
        {
          'target_name': 'cros_config_main_unittest',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'dependencies': [
            'cros_config',
          ],
          'conditions': [
            ['USE_json == 1', {
              'defines': [
                'USE_JSON',
              ]},
            ],
          ],
          'sources': [
            'cros_config_main_unittest.cc',
          ],
        },
        {
          'target_name': 'fake_cros_config_unittest',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'include_dirs': [
            'libcros_config',
          ],
          'dependencies': [
            'libcros_config',
          ],
          'sources': [
            'libcros_config/fake_cros_config_unittest.cc',
          ],
        },
      ],
    }],
  ],
}
