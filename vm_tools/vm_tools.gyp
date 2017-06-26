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
      'target_name': 'vm-rpcs',
      'type': 'static_library',
      'variables': {
        'proto_in_dir': 'proto',
        'proto_out_dir': 'include',
        'gen_grpc': 1,
        'exported_deps': [
          'grpc++',
          'protobuf',
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
        '<(proto_in_dir)/common.proto',
        '<(proto_in_dir)/guest.proto',
        '<(proto_in_dir)/host.proto',
      ],
      'includes': [
        '../common-mk/protoc.gypi',
      ],
    },
    {
      'target_name': 'libmaitred',
      'type': 'static_library',
      'dependencies': [
        'vm-rpcs',
      ],
      'sources': [
        'maitred/service_impl.cc',
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
            'maitred/service_impl_unittest.cc',
          ],
        },
      ],
    }],
  ],
}
