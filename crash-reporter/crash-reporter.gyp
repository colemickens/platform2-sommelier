{
  'targets': [
    {
      'target_name': 'libcrash',
      'type': 'static_library',
      'variables': {
        'exported_deps': [
          'libchrome-<(libbase_ver)',
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
      'sources': [
        'paths.cc',
        'util.cc',
      ],
    },
    {
      'target_name': 'libcrash_reporter',
      'type': 'static_library',
      'variables': {
        'exported_deps': [
          'libbrillo-<(libbase_ver)',
          'libchrome-<(libbase_ver)',
          'libdebugd-client',
          'libpcrecpp',
          'libsession_manager-client',
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
      'defines': [
        'USE_DIRENCRYPTION=<(USE_direncryption)',
      ],
      'sources': [
        'chrome_collector.cc',
        'crash_collector.cc',
        'ec_collector.cc',
        'kernel_collector.cc',
        'kernel_warning_collector.cc',
        'selinux_violation_collector.cc',
        'service_failure_collector.cc',
        'udev_collector.cc',
        'unclean_shutdown_collector.cc',
        'user_collector.cc',
        'user_collector_base.cc',
      ],
      'conditions': [
        ['USE_cheets == 1', {
          'sources': [
            'arc_collector.cc',
          ],
        }],
        ['USE_direncryption == 1', {
          'link_settings': {
            'libraries': [
              '-lkeyutils',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'crash_reporter',
      'type': 'executable',
      'variables': {
        'deps': [
          'dbus-1',
          'libmetrics-<(libbase_ver)',
          'libminijail',
        ],
      },
      'dependencies': [
        'libcrash',
        'libcrash_reporter',
      ],
      'defines': [
        'USE_CHEETS=<(USE_cheets)',
      ],
      'sources': [
        'crash_reporter.cc',
      ],
    },
    {
      'target_name': 'crash_sender',
      'type': 'executable',
      'variables': {
        'deps': [
          'libbrillo-<(libbase_ver)',
          'libchrome-<(libbase_ver)',
          'libminijail',
        ],
      },
      'dependencies': [
        'libcrash',
      ],
      'sources': [
        'crash_sender.cc',
        'crash_sender_util.cc',
      ],
    },
    {
      'target_name': 'list_proxies',
      'type': 'executable',
      'variables': {
        'deps': [
          'dbus-1',
          'libbrillo-<(libbase_ver)',
          'libchrome-<(libbase_ver)',
        ],
      },
      'sources': [
        'list_proxies.cc',
      ],
    },
    {
      'target_name': 'anomaly_collector',
      'type': 'executable',
      'variables': {
        'lexer_out_dir': 'crash-reporter',
        'deps': [
          'libmetrics-<(libbase_ver)',
        ],
      },
      # -D_GNU_SOURCE is needed for asprintf in anomaly_collector.l
      'defines': ['_GNU_SOURCE'],
      'link_settings': {
        'libraries': [
          '-lfl',
        ],
      },
      'sources': [
        'anomaly_collector.l',
      ],
      'includes': ['../common-mk/lex.gypi'],
    },
  ],
  'conditions': [
    ['USE_cheets == 1', {
      'targets': [
        {
          'target_name': 'core_collector',
          'type': 'executable',
          'variables': {
            'USE_amd64%': 0,
            'USE_cros_i686%': 0,
            'deps': [
              'breakpad-client',
            ],
          },
          'sources': [
            'core-collector/core_collector.cc',
            'core-collector/coredump_writer.cc',
            'core-collector/coredump_writer.h',
          ],
          'conditions': [
            # This condition matches the "use_i686" helper in the "cros-i686"
            # eclass. The "amd64" check allows the "cros_i686" USE flag to be
            # enabled for an overlay inherited by non-x86 boards.
            ['USE_cros_i686 == 1 and USE_amd64 == 1', {
              'cflags!': ['-fPIE'],
              'ldflags!': ['-pie'],
            }],
          ],
        },
      ],
    }],
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'crash_reporter_test',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'dependencies': [
            'libcrash',
            'libcrash_reporter',
          ],
          'sources': [
            'chrome_collector_test.cc',
            'crash_collector_test.cc',
            'crash_collector_test.h',
            'crash_reporter_logs_test.cc',
            'crash_sender_util.cc',
            'crash_sender_util_test.cc',
            'ec_collector_test.cc',
            'kernel_collector_test.cc',
            'kernel_collector_test.h',
            'kernel_warning_collector_test.cc',
            'paths_test.cc',
            'selinux_violation_collector_test.cc',
            'service_failure_collector_test.cc',
            'test_util.cc',
            'testrunner.cc',
            'udev_collector_test.cc',
            'unclean_shutdown_collector_test.cc',
            'user_collector_test.cc',
            'util_test.cc',
          ],
          'conditions': [
            ['USE_cheets == 1', {
              'sources': [
                'arc_collector_test.cc',
              ],
            }],
          ],
        },
        {
          'target_name': 'anomaly_collector_test',
          'type': 'none',
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)',
              'files': [
                'TEST_SELINUX',
                'TEST_SERVICE_FAILURE',
                'TEST_WARNING',
                'TEST_WARNING_OLD',
                'TEST_WARNING_OLD_ARM64',
                'anomaly_collector_test.sh',
                'anomaly_collector_test_reporter.sh',
                'crash_reporter_logs.conf',
              ],
            },
          ],
        },
      ],
    }],
  ],
}
