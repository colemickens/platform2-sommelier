{
 'includes': [
    'libweave.gypi',
  ],
  'target_defaults': {
    'variables': {
      'deps': [
        'expat',
        'libcrypto',
      ],
    },
    'include_dirs': [
      '.',
      '../libweave/include',
      '../libweave/third_party/modp_b64/modp_b64/',
    ],
  },
  'targets': [
    {
      'target_name': 'libweave_common',
      'type': 'static_library',
      'cflags!': ['-fPIE'],
      'cflags': ['-fPIC'],
      'variables': {
        'deps': [
          'libchrome-<(libbase_ver)',
        ],
      },
      'sources': [
        '<@(weave_sources)',
      ],
    },
    {
      'target_name': 'libweave-<(libbase_ver)',
      'type': 'shared_library',
      'variables': {
        'deps': [
          'libchrome-<(libbase_ver)',
        ],
      },
      'includes': [
        '../common-mk/deps.gypi',
      ],
      'dependencies': [
        'libweave_common',
      ],
      'sources': [
        'src/empty.cc'
      ],
    },
    {
      'target_name': 'libweave-test-<(libbase_ver)',
      'type': 'static_library',
      'standalone_static_library': 1,
      'variables': {
        'deps': [
          'libchrome-<(libbase_ver)',
        ],
      },
      'sources': [
        '<@(weave_test_sources)',
      ],
      'includes': ['../common-mk/deps.gypi'],
    },

    {
      'target_name': 'libweave_base_common',
      'type': 'static_library',
      'cflags!': ['-fPIE'],
      'cflags': [
        '-fPIC',
        '-Wno-format-nonliteral',
        '-Wno-char-subscripts',
        '-Wno-deprecated-register',
      ],
      'include_dirs': [
        '../libweave/external',
      ],
      'sources': [
        '<@(weave_sources)',
        '<@(base_sources)',
      ],
    },
    {
      'target_name': 'libweave_base',
      'type': 'shared_library',
      'include_dirs': [
        '../libweave/external',
      ],
      'includes': [
        '../common-mk/deps.gypi',
      ],
      'dependencies': [
        'libweave_base_common',
      ],
      'sources': [
        'src/empty.cc'
      ],
    },
    {
      'target_name': 'libweave_base-test',
      'type': 'static_library',
      'standalone_static_library': 1,
      'include_dirs': [
        '../libweave/external',
      ],
      'sources': [
        '<@(weave_test_sources)',
      ],
      'includes': ['../common-mk/deps.gypi'],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'libweave_testrunner',
          'type': 'executable',
          'variables': {
            'deps': [
              'libchrome-<(libbase_ver)',
            ],
          },
          'dependencies': [
            'libweave_common',
            'libweave-test-<(libbase_ver)',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            '<@(weave_unittest_sources)',
          ],
        },
        {
          'target_name': 'libweave_base_testrunner',
          'type': 'executable',
          'cflags': ['-Wno-format-nonliteral'],
          'include_dirs': [
            '../libweave/external',
          ],
          'dependencies': [
            'libweave_base_common',
            'libweave_base-test',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            '<@(weave_unittest_sources)',
            '<@(base_unittests)',
          ],
        },
        {
          'target_name': 'libweave_exports_testrunner',
          'type': 'executable',
          'variables': {
            'deps': [
              'libchrome-<(libbase_ver)',
            ],
          },
          'dependencies': [
            'libweave-<(libbase_ver)',
            'libweave-test-<(libbase_ver)',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            '<@(weave_exports_unittest_sources)',
          ],
        },
        {
          'target_name': 'libweave_base_exports_testrunner',
          'type': 'executable',
          'cflags': ['-Wno-format-nonliteral'],
          'include_dirs': [
            '../libweave/external',
          ],
          'dependencies': [
            'libweave_base',
            'libweave_base-test',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            '<@(weave_exports_unittest_sources)',
          ],
        },
      ],
    }],
  ],
}
