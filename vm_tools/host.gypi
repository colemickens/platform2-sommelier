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
      'deps': ['system_api'],
      'sources': [
        'concierge/service.cc',
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
      ],
    }],
  ],
}
