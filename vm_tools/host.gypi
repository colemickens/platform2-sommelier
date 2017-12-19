{
  'target_defaults': {
    'variables': {
      'deps': [
        'libbrillo-<(libbase_ver)',
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
      'target_name': 'vm_launcher',
      'type': 'executable',
      'dependencies': ['vm-rpcs'],
      'sources': [
        'launcher/crosvm.cc',
        'launcher/mac_address.cc',
        'launcher/nfs_export.cc',
        'launcher/pooled_resource.cc',
        'launcher/subnet.cc',
        'launcher/vm_launcher.cc',
        'launcher/vsock_cid.cc',
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
      'target_name': 'libconcierge',
      'type': 'static_library',
      'dependencies': ['vm-rpcs'],
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
        'concierge/mac_address_generator.cc',
        'concierge/service.cc',
        'concierge/subnet_pool.cc',
        'concierge/virtual_machine.cc',
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
