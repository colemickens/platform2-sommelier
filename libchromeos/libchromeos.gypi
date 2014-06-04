{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)'
      ],
      # This project has code that triggers warnings when using gtest.
      # Need to sort that out before we enable this.
      'enable_werror': 0,
    },
    'include_dirs': [
      '../libchromeos',
    ],
  },
  'targets': [
    {
      'target_name': 'libchromeos-<(libbase_ver)',
      'type': 'none',
      'dependencies': [
        'libchromeos-core-<(libbase_ver)',
        'libchromeos-cryptohome-<(libbase_ver)',
        'libchromeos-ui-<(libbase_ver)',
        'libpolicy-<(libbase_ver)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../libchromeos',
        ],
      },
      'includes': ['../../platform2/common-mk/deps.gypi'],
    },
    {
      'target_name': 'libchromeos-core-<(libbase_ver)',
      'type': 'shared_library',
      'variables': {
        'exported_deps': [
          'dbus-1',
          'dbus-c++-1',
          'dbus-glib-1',
          'glib-2.0',
        ],
        'deps': ['<@(exported_deps)'],
      },
      'cflags': [
        # TODO: crosbug.com/315233
        '-fvisibility=default',
      ],
      'all_dependent_settings': {
        'variables': {
          'deps': [
            '<@(exported_deps)',
          ],
        },
      },
      'sources': [
        'chromeos/dbus/abstract_dbus_service.cc',
        'chromeos/dbus/dbus.cc',
        'chromeos/process_information.cc',
        'chromeos/process.cc',
        'chromeos/secure_blob.cc',
        'chromeos/syslog_logging.cc',
        'chromeos/utility.cc',
      ],
    },
    {
      'target_name': 'libchromeos-cryptohome-<(libbase_ver)',
      'type': 'shared_library',
      'variables': {
        'exported_deps': ['openssl'],
        'deps': ['<@(exported_deps)'],
      },
      'cflags': [
        # TODO: crosbug.com/315233
        '-fvisibility=default',
      ],
      'all_dependent_settings': {
        'variables': {
          'deps': [
            '<@(exported_deps)',
          ],
        },
      },
      'sources': [
        'chromeos/cryptohome.cc',
      ],
    },
    {
      'target_name': 'libchromeos-ui-<(libbase_ver)',
      'type': 'shared_library',
      'variables': {
        'exported_deps': ['openssl'],
        'deps': ['<@(exported_deps)'],
      },
      'cflags': [
        '-fvisibility=default',
      ],
      'all_dependent_settings': {
        'variables': {
          'deps': [
            '<@(exported_deps)',
          ],
        },
      },
      'sources': [
        'chromeos/ui/chromium_command_builder.cc',
        'chromeos/ui/util.cc',
        'chromeos/ui/x_server_runner.cc',
      ],
    },
    {
      'target_name': 'libpolicy-<(libbase_ver)',
      'type': 'shared_library',
      'dependencies': [
        '../libchromeos/libpolicy.gyp:*',
        '../../platform2/common-mk/external_dependencies.gyp:policy-protos',
      ],
      'variables': {
        'exported_deps': [
          'glib-2.0',
          'openssl',
          'protobuf-lite',
        ],
        'deps': ['<@(exported_deps)'],
      },
      'all_dependent_settings': {
        'variables': {
          'deps': [
            '<@(exported_deps)',
          ],
        },
      },
      'ldflags': [
        '-Wl,--version-script,<(platform2_root)/libchromeos/libpolicy.ver',
      ],
      'sources': [
        'chromeos/policy/device_policy.cc',
        'chromeos/policy/device_policy_impl.cc',
        'chromeos/policy/libpolicy.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'libchromeos-<(libbase_ver)_unittests',
          'type': 'executable',
          'dependencies': [
            'libchromeos-<(libbase_ver)',
            'libchromeos-ui-<(libbase_ver)',
          ],
          'includes': ['../../platform2/common-mk/common_test.gypi'],
          'cflags': [
            '-Wno-format-zero-length',
          ],
          'conditions': [
            ['debug == 1', {
              'cflags': [
                '-fprofile-arcs',
                '-ftest-coverage',
                '-fno-inline',
              ],
              'libraries': [
                '-lgcov',
              ],
            }],
          ],
          'sources': [
            'chromeos/glib/object_unittest.cc',
            'chromeos/process_test.cc',
            'chromeos/secure_blob_unittest.cc',
            'chromeos/ui/chromium_command_builder_unittest.cc',
            'chromeos/ui/x_server_runner_unittest.cc',
            'chromeos/utility_test.cc',
            'testrunner.cc',
          ]
        },
        {
          'target_name': 'libpolicy-<(libbase_ver)_unittests',
          'type': 'executable',
          'dependencies': ['libpolicy-<(libbase_ver)'],
          'includes': ['../../platform2/common-mk/common_test.gypi'],
          'sources': [
            'chromeos/policy/tests/libpolicy_unittest.cc',
          ]
        },
      ],
    }],
  ],
}
