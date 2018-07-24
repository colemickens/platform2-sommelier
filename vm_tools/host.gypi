{
  'target_defaults': {
    'variables': {
      'deps': [
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'maitred_client',
      'type': 'executable',
      'dependencies': ['vm-rpcs'],
      'sources': [
        'maitred/client.cc',
      ],
    },
    {
      'target_name': 'libforwarder',
      'type': 'static_library',
      'dependencies': ['vm-rpcs'],
      'sources': [
        'syslog/forwarder.cc',
        'syslog/scrubber.cc',
      ],
    },
    {
      'target_name': 'vmlog_forwarder',
      'type': 'executable',
      'dependencies': [
        'libforwarder',
        'vm-rpcs',
      ],
      'sources': [
        'syslog/host_server.cc',
      ],
    },
    {
      'target_name': 'vsh',
      'type': 'executable',
      'variables': {
        'deps': ['system_api'],
      },
      'dependencies': [
        'libvsh',
        'vsh-protos',
      ],
      'sources': [
        'vsh/scoped_termios.cc',
        'vsh/vsh.cc',
        'vsh/vsh_client.cc',
      ],
    },
    {
      'target_name': 'libconcierge',
      'type': 'static_library',
      'dependencies': [
        'vm-rpcs',
        'container-rpcs',
      ],
      'variables': {
        'exported_deps': [
          'libqcow_utils',
          'system_api',
        ],
        'deps': ['<@(exported_deps)'],
      },
      'all_dependent_settings': {
        'variables': {
          'deps': ['<@(exported_deps)'],
        },
      },
      'link_settings': {
        'libraries': [
          '-lgpr',
        ],
      },
      'sources': [
        'concierge/mac_address_generator.cc',
        'concierge/service.cc',
        'concierge/ssh_keys.cc',
        'concierge/startup_listener_impl.cc',
        'concierge/subnet_pool.cc',
        'concierge/tap_device_builder.cc',
        'concierge/virtual_machine.cc',
        'concierge/vsock_cid_pool.cc',
      ],
    },
    {
      'target_name': 'vm_concierge',
      'type': 'executable',
      'dependencies': [
        'libconcierge',
      ],
      'sources': [
        'concierge/main.cc',
      ],
    },
    {
      'target_name': 'concierge_client',
      'type': 'executable',
      'variables': {
        'deps': [
          'libqcow_utils',
          'protobuf',
          'system_api',
        ],
      },
      'sources': [
        'concierge/client.cc',
      ],
    },
    {
      'target_name': 'libcicerone',
      'type': 'static_library',
      'dependencies': [
        'container-rpcs',
        'tremplin-rpcs',
      ],
      'variables': {
        'exported_deps': [
          'system_api',
        ],
        'deps': ['<@(exported_deps)'],
      },
      'all_dependent_settings': {
        'variables': {
          'deps': ['<@(exported_deps)'],
        },
      },
      'link_settings': {
        'libraries': [
          '-lgpr',
        ],
      },
      'sources': [
        'cicerone/container.cc',
        'cicerone/container_listener_impl.cc',
        'cicerone/service.cc',
        'cicerone/tremplin_listener_impl.cc',
        'cicerone/virtual_machine.cc',
      ],
    },
    {
      'target_name': 'vm_cicerone',
      'type': 'executable',
      'dependencies': [
        'libcicerone',
      ],
      'sources': [
        'cicerone/main.cc',
      ],
    },
    {
      'target_name': 'cicerone_client',
      'type': 'executable',
      'variables': {
        'deps': [
          'protobuf',
          'system_api',
        ],
      },
      'sources': [
        'cicerone/client.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'syslog_forwarder_test',
          'type': 'executable',
          'dependencies': [
            'libforwarder',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'syslog/forwarder_unittest.cc',
            'syslog/scrubber_unittest.cc',
          ],
        },
        {
          'target_name': 'cicerone_test',
          'type': 'executable',
          'dependencies': [
            'libcicerone',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'cicerone/virtual_machine_unittest.cc',
          ],
        },
        {
          'target_name': 'concierge_test',
          'type': 'executable',
          'dependencies': [
            'libconcierge',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'concierge/mac_address_generator_unittest.cc',
            'concierge/subnet_pool_unittest.cc',
            'concierge/virtual_machine_unittest.cc',
          ],
        },
      ],
    }],
  ],
}
