{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
        'libchromeos-<(libbase_ver)',
        'libpsyche',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'libdemo',
      'type': 'static_library',
      'variables': {
        'exported_deps': [
          'libprotobinder',
          'protobuf-lite',
        ],
        'proto_in_dir': '.',
        'proto_out_dir': 'include/psyche_demo/proto_bindings',
        'gen_bidl': 1,
      },
      'all_dependent_settings': {
        'variables': {
          'deps': [
            '<@(exported_deps)',
          ],
        },
      },
      'sources': [
        '<(proto_in_dir)/psyche_demo.proto',
      ],
      'includes': ['../../platform2/common-mk/protoc.gypi'],
    },
    {
      'target_name': 'psyche_demo_client',
      'type': 'executable',
      'dependencies': [ 'libdemo', ],
      'sources': [ 'psyche_demo_client.cc', ],
    },
    {
      'target_name': 'psyche_demo_server',
      'type': 'executable',
      'dependencies': [ 'libdemo', ],
      'sources': [ 'psyche_demo_server.cc', ],
    },
  ],
}
