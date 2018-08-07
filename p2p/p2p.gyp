# TODO: Rename these files to pass this check.
# gyplint: disable=GypLintSourceFileNames

{
  'target_defaults': {
    'variables': {
      'deps': [
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
      ],
    },
    'cflags': [
      '-ffunction-sections',
      '-Wextra',
      # To allow { 0 } initializations.
      '-Wno-missing-field-initializers',
    ],
    'cflags_cc': [
      '-fno-strict-aliasing',
    ],
    'ldflags': [
      '-Wl,--gc-sections',
    ],
  },
  'targets': [
    # common utils.
    {
      'target_name': 'libp2p-util',
      'type': 'static_library',
      'variables': {
        'exported_deps': [
          'glib-2.0',
          'gobject-2.0',
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
      'link_settings': {
        'variables': {
          'deps': [
            '<@(exported_deps)',
          ],
        },
        'libraries': [
          '-lrt',
        ],
      },
      'sources': [
        'common/clock.cc',
        'common/server_message.cc',
        'common/util.cc',
      ],
    },
    # p2p-client
    {
      'target_name': 'libp2p-client',
      'type': 'static_library',
      'variables': {
        'exported_deps': [
          'avahi-client',
          'avahi-glib',
          'glib-2.0',
          'gobject-2.0',
          'libmetrics-<(libbase_ver)',
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
        'client/peer_selector.cc',
        'client/service_finder.cc',
      ],
    },
    {
      'target_name': 'p2p-client',
      'type': 'executable',
      'dependencies': [
        'libp2p-util',
        'libp2p-client',
      ],
      'sources': [
        'client/main.cc',
      ],
    },
    # p2p-server
    {
      'target_name': 'libp2p-server',
      'type': 'static_library',
      'variables': {
        'exported_deps': [
          'avahi-client',
          'avahi-glib',
          'gio-2.0',
          'gio-unix-2.0',
          'glib-2.0',
          'gobject-2.0',
          'libmetrics-<(libbase_ver)',
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
        'server/file_watcher.cc',
        'server/http_server.cc',
        'server/peer_update_manager.cc',
        'server/service_publisher.cc',
      ],
    },
    {
      'target_name': 'p2p-server',
      'type': 'executable',
      'dependencies': [
        'libp2p-util',
        'libp2p-server',
      ],
      'sources': [
        'server/main.cc',
      ],
    },
    # p2p-http-server
    {
      'target_name': 'libp2p-http-server',
      'type': 'static_library',
      'variables': {
        'exported_deps': [
          'avahi-client',
          'avahi-glib',
          'glib-2.0',
          'libmetrics-<(libbase_ver)',
        ],
        'deps': ['<@(exported_deps)'],
      },
      'all_dependent_settings': {
        'variables': {
          'deps': [
            '<@(exported_deps)',
          ],
        },
        'link_settings': {
          'libraries': [
            '-lattr',
          ],
        },
      },
      'sources': [
        'http_server/connection_delegate.cc',
        'http_server/server.cc',
      ],
    },
    {
      'target_name': 'p2p-http-server',
      'type': 'executable',
      'dependencies': [
        'libp2p-util',
        'libp2p-http-server',
      ],
      'sources': [
        'http_server/main.cc',
      ],
    },
  ],

  # Test only.
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        # common test-utils.
        {
          'target_name': 'libp2p-testutil',
          'type': 'static_library',
          'variables': {
            'exported_deps': [
              'gobject-2.0',
              'libchrome-test-<(libbase_ver)',
            ],
            'deps': ['<@(exported_deps)'],
          },
          'includes': ['../common-mk/common_test.gypi'],
          'all_dependent_settings': {
            'variables': {
              'deps': [
                '<@(exported_deps)',
              ],
            },
          },
          'link_settings': {
            'variables': {
              'deps': [
                '<@(exported_deps)',
              ],
            },
          },
          'sources': [
            'common/testutil.cc',
          ],
        },
        # common tests
        {
          'target_name': 'p2p-common-unittests',
          'type': 'executable',
          'dependencies': [
            'libp2p-util',
            'libp2p-testutil',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'sources': [
            'common/server_message_unittest.cc',
            'common/struct_serializer_unittest.cc',
            'common/testutil_unittest.cc',
          ],
        },
        # p2p-client tests
        {
          'target_name': 'p2p-client-unittests',
          'type': 'executable',
          'dependencies': [
            'libp2p-util',
            'libp2p-testutil',
            'libp2p-client',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'sources': [
            'client/fake_service_finder.cc',
            'client/peer_selector_unittest.cc',
          ],
        },
        # p2p-server tests
        {
          'target_name': 'p2p-server-unittests',
          'type': 'executable',
          'dependencies': [
            'libp2p-util',
            'libp2p-testutil',
            'libp2p-server',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'sources': [
            'server/file_watcher_unittest.cc',
            'server/http_server_unittest.cc',
            'server/peer_update_manager_unittest.cc',
          ],
        },
        # p2p-http-server tests
        {
          'target_name': 'p2p-http-server-unittests',
          'type': 'executable',
          'dependencies': [
            'libp2p-util',
            'libp2p-testutil',
            'libp2p-http-server',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'sources': [
            'http_server/connection_delegate_unittest.cc',
            'http_server/server_unittest.cc',
          ],
        },
      ],
    }],
  ],
}
