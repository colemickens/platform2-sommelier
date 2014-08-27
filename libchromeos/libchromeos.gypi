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
        'libchromeos-bootstat-<(libbase_ver)',
        'libchromeos-core-<(libbase_ver)',
        'libchromeos-cryptohome-<(libbase_ver)',
        'libchromeos-http-<(libbase_ver)',
        'libchromeos-minijail-<(libbase_ver)',
        'libchromeos-ui-<(libbase_ver)',
        'libpolicy-<(libbase_ver)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../libchromeos',
        ],
      },
      'includes': ['../common-mk/deps.gypi'],
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
        'chromeos/any.cc',
        'chromeos/asynchronous_signal_handler.cc',
        'chromeos/data_encoding.cc',
        'chromeos/dbus/abstract_dbus_service.cc',
        'chromeos/dbus/async_event_sequencer.cc',
        'chromeos/dbus/data_serialization.cc',
        'chromeos/dbus/dbus.cc',
        'chromeos/dbus/dbus_object.cc',
        'chromeos/dbus/exported_object_manager.cc',
        'chromeos/dbus/exported_property_set.cc',
        'chromeos/dbus/utils.cc',
        'chromeos/errors/error.cc',
        'chromeos/errors/error_codes.cc',
        'chromeos/mime_utils.cc',
        'chromeos/process.cc',
        'chromeos/process_information.cc',
        'chromeos/secure_blob.cc',
        'chromeos/strings/string_utils.cc',
        'chromeos/syslog_logging.cc',
        'chromeos/url_utils.cc',
      ],
    },
    {
      'target_name': 'libchromeos-http-<(libbase_ver)',
      'type': 'shared_library',
      'variables': {
        'exported_deps': [
          'libcurl',
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
      'cflags': [
        # TODO: crosbug.com/315233
        '-fvisibility=default',
      ],
      'sources': [
        'chromeos/http/http_connection_curl.cc',
        'chromeos/http/http_request.cc',
        'chromeos/http/http_transport_curl.cc',
        'chromeos/http/http_utils.cc',
      ],
    },
    {
      'target_name': 'libchromeos-test-<(libbase_ver)',
      'type': 'static_library',
      'standalone_static_library': 1,
      'dependencies': [
        'libchromeos-http-<(libbase_ver)',
      ],
      'sources': [
        'chromeos/http/http_connection_fake.cc',
        'chromeos/http/http_transport_fake.cc',
      ],
      'includes': ['../common-mk/deps.gypi'],
    },
    {
      'target_name': 'libchromeos-cryptohome-<(libbase_ver)',
      'type': 'shared_library',
      'variables': {
        'exported_deps': [
          'openssl',
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
        'chromeos/cryptohome.cc',
      ],
    },
    {
      'target_name': 'libchromeos-minijail-<(libbase_ver)',
      'type': 'shared_library',
      'libraries': [
        '-lminijail',
      ],
      'cflags': [
        '-fvisibility=default',
      ],
      'sources': [
        'chromeos/minijail/minijail.cc',
      ],
    },
    {
      'target_name': 'libchromeos-ui-<(libbase_ver)',
      'type': 'shared_library',
      'dependencies': [
        'libchromeos-bootstat-<(libbase_ver)',
      ],
      'cflags': [
        '-fvisibility=default',
      ],
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
        'libpolicy-includes',
        '../common-mk/external_dependencies.gyp:policy-protos',
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
    {
      'target_name': 'libchromeos-bootstat-<(libbase_ver)',
      'type': 'shared_library',
      'sources': [
        'chromeos/bootstat/bootstat_log.c',
      ],
      'cflags': [
        '-fvisibility=default',
      ],
      'libraries': [
        '-lrootdev',
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
            'libchromeos-test-<(libbase_ver)',
            'libchromeos-ui-<(libbase_ver)',
          ],
          'variables': {
            'deps': [
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'includes': ['../common-mk/common_test.gypi'],
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
            'chromeos/any_unittest.cc',
            'chromeos/any_internal_impl_unittest.cc',
            'chromeos/asynchronous_signal_handler_unittest.cc',
            'chromeos/data_encoding_unittest.cc',
            'chromeos/dbus/async_event_sequencer_unittest.cc',
            'chromeos/dbus/data_serialization_unittest.cc',
            'chromeos/dbus/dbus_object_unittest.cc',
            'chromeos/dbus/dbus_param_reader_unittest.cc',
            'chromeos/dbus/dbus_param_writer_unittest.cc',
            'chromeos/dbus/exported_object_manager_unittest.cc',
            'chromeos/dbus/exported_property_set_unittest.cc',
            'chromeos/errors/error_unittest.cc',
            'chromeos/glib/object_unittest.cc',
            'chromeos/http/http_utils_unittest.cc',
            'chromeos/mime_utils_unittest.cc',
            'chromeos/process_test.cc',
            'chromeos/secure_blob_unittest.cc',
            'chromeos/strings/string_utils_unittest.cc',
            'chromeos/ui/chromium_command_builder_unittest.cc',
            'chromeos/ui/x_server_runner_unittest.cc',
            'chromeos/url_utils_unittest.cc',
            'testrunner.cc',
          ]
        },
        {
          'target_name': 'libpolicy-<(libbase_ver)_unittests',
          'type': 'executable',
          'dependencies': ['libpolicy-<(libbase_ver)'],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'chromeos/policy/tests/libpolicy_unittest.cc',
          ]
        },
        {
          'target_name': 'libbootstat_unittests',
          'type': 'executable',
          'dependencies': [
            'libchromeos-bootstat-<(libbase_ver)',
          ],
          'includes': [
            '../common-mk/common_test.gypi',
          ],
          'sources': [
            'chromeos/bootstat/log_unit_tests.cc',
          ],
        },
      ],
    }],
  ],
}
