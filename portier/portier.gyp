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
      'target_name': 'libportier',
      'type': 'static_library',
      'variables': {
        'exported_deps': [
          'libshill-net-<(libbase_ver)',
        ],
        'deps': ['<@(exported_deps)'],
      },
      'sources': [
        'ether_socket.cc',
        'group.cc',
        'icmpv6_socket.cc',
        'ipv6_util.cc',
        'll_address.cc',
        'nd_bpf.cc',
        'nd_msg.cc',
        'neighbor_cache.cc',
        'network_socket.cc',
        'proxy_interface.cc',
        'status.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'portier_test',
          'type': 'executable',
          'dependencies': [
            'libportier',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'variables': {
            'exported_deps': [
              'libshill-net-<(libbase_ver)',
            ],
            'deps': ['<@(exported_deps)'],
          },
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'group_manager_test.cc',
            'group_test.cc',
            'ipv6_util_test.cc',
            'll_address_test.cc',
            'nd_msg_test.cc',
            'neighbor_cache_test.cc',
            'status_test.cc',
          ],
        },
      ],
    }],
  ],
}
