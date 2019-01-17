{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'libmaitred',
      'type': 'static_library',
      'dependencies': [
        'vm-rpcs',
      ],
      'sources': [
        'maitred/init.cc',
        'maitred/service_impl.cc',
      ],
    },
    {
      'target_name': 'libgarcon',
      'type': 'static_library',
      'dependencies': [
        'container-rpcs',
      ],
      'sources': [
        'garcon/app_search.cc',
        'garcon/desktop_file.cc',
        'garcon/host_notifier.cc',
        'garcon/icon_finder.cc',
        'garcon/icon_index_file.cc',
        'garcon/ini_parse_util.cc',
        'garcon/mime_types_parser.cc',
        'garcon/package_kit_proxy.cc',
        'garcon/service_impl.cc',
      ],
    },
    {
      'target_name': 'libsyslog',
      'type': 'static_library',
      'dependencies': [
        'vm-rpcs',
      ],
      'variables': {
        'exported_deps': ['libminijail'],
        'deps': ['<@(exported_deps)'],
      },
      'all_dependent_settings': {
        'variables': {
          'deps': ['<@(exported_deps)'],
        },
      },
      'sources': [
        'syslog/collector.cc',
        'syslog/parser.cc',
      ],
    },
    {
      'target_name': 'maitred',
      'type': 'executable',
      'dependencies': ['libmaitred'],
      'sources': [
        'maitred/main.cc',
      ],
    },
    {
      'target_name': 'garcon',
      'type': 'executable',
      'dependencies': [
        'libgarcon',
      ],
      'sources': [
        'garcon/main.cc',
      ],
    },
    {
      'target_name': 'vm_syslog',
      'type': 'executable',
      'dependencies': ['libsyslog'],
      'sources': [
        'syslog/main.cc',
      ],
    },
    {
      'target_name': 'virtwl_guest_proxy',
      'type': 'executable',
      'ldflags': [
        '-pthread',
      ],
      'sources': [
        'virtwl_guest_proxy/main.c',
      ],
    },
    {
      'target_name': 'vshd',
      'type': 'executable',
      'dependencies': [
        'libvsh',
        'vsh-protos',
      ],
      'sources': [
        'vsh/vsh_forwarder.cc',
        'vsh/vshd.cc',
      ],
      'variables': {
        'deps': [
          'libbrillo-<(libbase_ver)',
        ],
      },
    },
    {
      'target_name': 'notification-protocol',
      'type': 'static_library',
      'variables': {
        'wayland_in_dir': 'notificationd/protocol',
        'wayland_out_dir': '.',
      },
      'sources': [
        'notificationd/protocol/notification-shell-unstable-v1.xml',
      ],
      'includes': ['sommelier/wayland-protocol.gypi'],
    },
    {
      'target_name': 'libnotificationd',
      'type': 'static_library',
      'variables': {
        'exported_deps': [
          'wayland-client',
          'wayland-server',
        ],
        'deps': [
          'dbus-1',
          'libbrillo-<(libbase_ver)',
          '<@(exported_deps)',
        ],
      },
      'dependencies': [
        'notification-protocol',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'notificationd/dbus_service.cc',
        'notificationd/notification_daemon.cc',
        'notificationd/notification_shell_client.cc',
      ],
      'defines': [
        'WL_HIDE_DEPRECATED',
      ],
    },
    {
      'target_name': 'notificationd',
      'type': 'executable',
      'variables': {
        'exported_deps': [
          'wayland-client',
          'wayland-server',
        ],
        'deps': [
          'dbus-1',
          'libbrillo-<(libbase_ver)',
          '<@(exported_deps)',
        ],
      },
      'dependencies': [
        'notification-protocol',
        'libnotificationd',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'notificationd/notificationd.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'maitred_service_test',
          'type': 'executable',
          'dependencies': [
            'libmaitred',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'maitred/service_impl_test.cc',
          ],
        },
        {
          'target_name': 'maitred_syslog_test',
          'type': 'executable',
          'dependencies': [
            'libsyslog',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'syslog/collector_test.cc',
            'syslog/parser_test.cc',
          ],
        },
        {
          'target_name': 'garcon_desktop_file_test',
          'type': 'executable',
          'dependencies': [
            'libgarcon',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'garcon/desktop_file_test.cc',
          ],
        },
        {
          'target_name': 'garcon_icon_index_file_test',
          'type': 'executable',
          'dependencies': [
            'libgarcon',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'garcon/icon_index_file_test.cc',
          ],
        },
        {
          'target_name': 'garcon_icon_finder_test',
          'type': 'executable',
          'dependencies': [
            'libgarcon',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'garcon/icon_finder_test.cc',
          ],
        },
        {
          'target_name': 'garcon_mime_types_parser_test',
          'type': 'executable',
          'dependencies': [
            'libgarcon',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'garcon/mime_types_parser_test.cc',
          ],
        },
        {
          'target_name': 'notificationd_test',
          'type': 'executable',
          'variables': {
            'exported_deps': [
              'wayland-client',
              'wayland-server',
            ],
            'deps': [
              'dbus-1',
              'libbrillo-<(libbase_ver)',
              'libchrome-test-<(libbase_ver)',
              '<@(exported_deps)',
            ],
          },
          'dependencies': [
            'notification-protocol',
            'libnotificationd',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'notificationd/dbus_service_test.cc',
          ],
        },
        {
          'target_name': 'garcon_app_search_test',
          'type': 'executable',
          'dependencies': [
            'libgarcon',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'garcon/app_search_test.cc',
          ],
        },
      ],
    }],
  ],
}
