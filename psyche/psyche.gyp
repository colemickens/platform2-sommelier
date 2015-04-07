# The psyche package.
{
  'includes': ['common.gypi'],
  'targets': [
    {
      'target_name': 'libsomaproto',
      'type': 'static_library',
      'variables': {
        # TODO(derat): Use installed protos: http://brbug.com/684
        'proto_in_dir': '../soma/idl',
        'proto_out_dir': 'include/psyche/proto_bindings',
        'gen_bidl': 1,
      },
      'sources': [
        '<(proto_in_dir)/soma_container_spec.proto',
        '<(proto_in_dir)/soma.proto',
      ],
      'includes': ['../common-mk/protoc.gypi'],
    },
    {
      'target_name': 'libpsyched',
      'type': 'static_library',
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
      'sources': [
        'psyched/client.cc',
        'psyched/container.cc',
        'psyched/registrar.cc',
        'psyched/service.cc',
        'psyched/soma_connection.cc',
      ],
      'dependencies': [
        'libcommon',
        'libsomaproto',
      ],
    },
    {
      'target_name': 'psyched',
      'type': 'executable',
      'sources': [ 'psyched/main.cc' ],
      'dependencies': [
        'libpsyched',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'psyched_test',
          'type': 'executable',
          'includes': [ '../common-mk/common_test.gypi' ],
          'dependencies': [
            'libcommontest',
            'libpsyched',
          ],
          'sources': [
            'common/testrunner.cc',
            'psyched/client_stub.cc',
            'psyched/client_unittest.cc',
            'psyched/container_stub.cc',
            'psyched/container_unittest.cc',
            'psyched/registrar_unittest.cc',
            'psyched/service_stub.cc',
            'psyched/service_unittest.cc',
            'psyched/stub_factory.cc',
          ],
        },
      ],
    }],
  ],
}
