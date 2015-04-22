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
      'target_name': 'soma-protos',
      'type': 'static_library',
      'variables': {
        # TODO(jorgelo): Use installed protos (brbug.com/684).
        'proto_in_dir': '../soma/idl',
        'proto_out_dir': 'include/germ/proto_bindings',
        'gen_bidl': 1,
      },
      'sources': [
        '<(proto_in_dir)/soma_container_spec.proto',
      ],
      'includes': ['../common-mk/protoc.gypi'],
    },
    {
      'target_name': 'libgerm',
      'type': 'static_library',
      'dependencies': [
        'germ-protos',
        'soma-protos',
      ],
      'sources': [
        'environment.cc',
        'germ_client.cc',
        'germ_host.cc',
        'germ_init.cc',
        'germ_zygote.cc',
        'launcher.cc',
        'process_reaper.cc',
        'switches.cc',
      ],
      'variables': {
        'exported_deps': [
          'libsoma',
        ],
      },
      'all_dependent_settings': {
        'variables': {
          'deps': [
            '<@(exported_deps)',
          ],
        },
      },
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
      'sources': [
        'germd.cc',
      ],
      'variables': {
        'deps': [
          'libpsyche',
        ],
      },
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
            'germ_zygote_unittest.cc',
            'launcher_unittest.cc',
            'process_reaper_unittest.cc',
            'test_util.cc',
            'run_all_tests.cc',
          ],
        },
      ],
    }],
  ],
}
