{
  'target_defaults': {
    'variables': {
      'deps': [
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
      ],
    },
    'defines': [
      'USE_CROSVM_WL_DMABUF=<(USE_crosvm_wl_dmabuf)',
    ],
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
          'libshill-client',
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
        'concierge/plugin_vm.cc',
        'concierge/seneschal_server_proxy.cc',
        'concierge/service.cc',
        'concierge/shill_client.cc',
        'concierge/ssh_keys.cc',
        'concierge/startup_listener_impl.cc',
        'concierge/subnet.cc',
        'concierge/subnet_pool.cc',
        'concierge/tap_device_builder.cc',
        'concierge/termina_vm.cc',
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
        'cicerone/tzif_parser.cc',
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
    {
      'target_name': 'libseneschal',
      'type': 'static_library',
      'variables': {
        'exported_deps': [
          'libminijail',
          'protobuf',
          'system_api',
        ],
        'deps': ['<@(exported_deps)'],
      },
      'all_dependent_settings': {
        'variables': {
          'deps': ['<@(exported_deps)'],
        },
      },
      'sources': [
        'seneschal/service.cc',
      ],
    },
    {
      'target_name': 'seneschal',
      'type': 'executable',
      'dependencies': [
        'libseneschal',
      ],
      'sources': [
        'seneschal/main.cc',
      ],
    },
    {
      'target_name': 'seneschal_client',
      'type': 'executable',
      'variables': {
        'deps': [
          'protobuf',
          'system_api',
        ],
      },
      'sources': [
        'seneschal/client.cc',
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
            'syslog/forwarder_test.cc',
            'syslog/scrubber_test.cc',
          ],
        },
        {
          'target_name': 'service_testing_helper_lib',
          'type': 'static_library',
          'variables': {
            'exported_deps': [
              'libchrome-test-<(libbase_ver)',
            ],
            'deps': ['<@(exported_deps)'],
          },
          'all_dependent_settings': {
            'variables': {
              'deps': ['<@(exported_deps)'],
            },
          },
          'dependencies': [
            'libcicerone',
          ],
          'sources': [
            'cicerone/dbus_message_testing_helper.cc',
            'cicerone/dbus_message_testing_helper.h',
            'cicerone/service_testing_helper.cc',
            'cicerone/service_testing_helper.h',
            'cicerone/tremplin_test_stub.cc',
            'cicerone/tremplin_test_stub.h',
          ],
        },
        {
          'target_name': 'cicerone_test',
          'type': 'executable',
          'dependencies': [
            'libcicerone',
            'service_testing_helper_lib',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'cicerone/container_listener_impl_test.cc',
            'cicerone/tzif_parser_test.cc',
            'cicerone/virtual_machine_test.cc',
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
            'concierge/mac_address_generator_test.cc',
            'concierge/subnet_pool_test.cc',
            'concierge/subnet_test.cc',
            'concierge/termina_vm_test.cc',
          ],
        },
      ],
    }],
  ],
}
