{
  'targets': [
    # Library with generated gRPC API definitions.
    {
      'target_name': 'diagnostics-grpc-protos',
      'type': 'static_library',
      'variables': {
        'proto_in_dir': 'grpc',
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
        '<(proto_in_dir)/diagnostics_processor.proto',
        '<(proto_in_dir)/diagnosticsd.proto',
      ],
      'includes': [
        '../common-mk/protoc.gypi',
      ],
    },
    # The diagnosticsd daemon executable.
    {
      'target_name': 'libgrpc_async_adapter',
      'type': 'static_library',
      'variables': {
        'exported_deps': [
          'grpc++',
          'libchrome-<(libbase_ver)',
        ],
        'deps': [
          '<@(exported_deps)',
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
        'grpc_async_adapter/grpc_completion_queue_dispatcher.cc',
      ],
    },
    {
      'target_name': 'diagnosticsd',
      'type': 'executable',
      'dependencies': [
        'libgrpc_async_adapter',
      ],
      'includes': ['mojom_generator.gypi'],
      'dependencies': [
        'diagnostics-grpc-protos',
      ],
      'variables': {
        'deps': [
          'libbrillo-<(libbase_ver)',
        ],
      },
      'sources': [
        'diagnosticsd/main.cc',
        'mojo/diagnosticsd.mojom',
      ],
    },
    {
      'target_name': 'diagnostics_processor',
      'type': 'executable',
      'dependencies': [
        'libgrpc_async_adapter',
      ],
      'variables': {
        'deps': [
          'libbrillo-<(libbase_ver)',
        ],
      },
      'sources': [
        'diagnostics_processor/main.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        # Unit tests.
        {
          'target_name': 'libgrpc_async_adapter_test',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'dependencies': [
            '../common-mk/testrunner.gyp:testrunner',
            'libgrpc_async_adapter',
          ],
          'sources': [
            'grpc_async_adapter/grpc_completion_queue_dispatcher_test.cc',
          ],
          'variables': {
            'deps': [
              'libchrome-<(libbase_ver)',
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'link_settings': {
            'libraries': [
              '-lgpr',
            ],
          },
        },
        {
          'target_name': 'diagnosticsd_test',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'dependencies': [
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'variables': {
            'deps': [
              'libchrome-<(libbase_ver)',
              'libchrome-test-<(libbase_ver)',
            ],
          },
        },
      ],
    }],
  ],
}
