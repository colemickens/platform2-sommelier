{
  'target_defaults': {
    'dependencies': [
      '../../platform2/libchromeos/libchromeos-<(libbase_ver).gyp:libchromeos-<(libbase_ver)',
    ],
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
        'openssl',
      ],
    },
    'link_settings': {
      'libraries': [
        '-lgflags',
      ],
    },
    'defines': [
      'IPSEC_STARTER="/usr/libexec/ipsec/starter"',
      'IPSEC_WHACK="/usr/libexec/ipsec/whack"',
      'IPSEC_UPDOWN="/usr/libexec/l2tpipsec_vpn/pluto_updown"',
      'L2TPD="/usr/sbin/xl2tpd"',
      'PKCS11_LIB="<(libdir)/libchaps.so"'
    ],
  },
  'targets': [
    {
      'target_name': 'libl2tpipsec_vpn',
      'type': 'static_library',
      'sources': [
        'ipsec_manager.cc',
        'l2tp_manager.cc',
        'daemon.cc',
        'service_manager.cc',
      ],
    },
    {
      'target_name': 'l2tpipsec_vpn',
      'type': 'executable',
      'dependencies': ['libl2tpipsec_vpn'],
      'sources': [
        'l2tpipsec_vpn.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'daemon_test',
          'type': 'executable',
          'dependencies': ['libl2tpipsec_vpn'],
          'includes': ['../../platform2/common-mk/common_test.gypi'],
          'sources': [
            'daemon_test.cc',
          ]
        },
        {
          'target_name': 'ipsec_manager_test',
          'type': 'executable',
          'dependencies': ['libl2tpipsec_vpn'],
          'includes': ['../../platform2/common-mk/common_test.gypi'],
          'sources': [
            'ipsec_manager_test.cc',
          ]
        },
        {
          'target_name': 'l2tp_manager_test',
          'type': 'executable',
          'dependencies': ['libl2tpipsec_vpn'],
          'includes': ['../../platform2/common-mk/common_test.gypi'],
          'sources': [
            'l2tp_manager_test.cc',
          ]
        },
        {
          'target_name': 'service_manager_test',
          'type': 'executable',
          'dependencies': ['libl2tpipsec_vpn'],
          'includes': ['../../platform2/common-mk/common_test.gypi'],
          'sources': [
            'service_manager_test.cc',
          ]
        },
      ],
    }],
  ],
}
