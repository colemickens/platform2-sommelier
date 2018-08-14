{
  'targets': [
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
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
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
