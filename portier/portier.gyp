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
        'ipv6_util.cc',
        'll_address.cc',
        'nd_msg.cc',
        'network_socket.cc',
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
            'ipv6_util_unittest.cc',
            'll_address_unittest.cc',
            'nd_msg_unittest.cc',
            'status_unittest.cc',
          ],
        },
      ],
    }],
  ],
}
