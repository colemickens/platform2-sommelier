{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
        'libchromeos-<(libbase_ver)',
        'libprotobinder',
        'protobuf-lite',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'germ-protos',
      'type': 'static_library',
      'variables': {
        'proto_in_dir': 'idl',
        'proto_out_dir': 'include/germ/proto_bindings',
        'gen_bidl': 1,
      },
      'sources': [
        '<(proto_in_dir)/germ.proto',
      ],
      'includes': ['../common-mk/protoc.gypi'],
    },
    {
      'target_name': 'libgerm',
      'type': 'static_library',
      'dependencies': [
        'germ-protos',
      ],
      'sources': [
        'environment.cc',
        'germ_host.cc',
        'launcher.cc',
      ],
    },
    {
      'target_name': 'germ',
      'type': 'executable',
      'dependencies': [
        'libgerm',
      ],
      'sources': ['germ_main.cc'],
    },
    {
      'target_name': 'germd',
      'type': 'executable',
      'dependencies': [
        'libgerm',
      ],
      'sources': ['germd.cc'],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'germ_unittest',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'dependencies': ['libgerm'],
          'sources': [
            'run_all_tests.cc',
          ],
        },
      ],
    }],
  ],
}
