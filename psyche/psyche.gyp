{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
        'libchromeos-<(libbase_ver)',
      ],
    },
  },

  'targets': [
    # Code shared by libpsyche and psyched.
    {
      'target_name': 'libcommon',
      'type': 'static_library',
      'variables': {
        'exported_deps': [
          'libprotobinder',
          'protobuf-lite',
        ],
        'proto_in_dir': 'common',
        'proto_out_dir': 'include/psyche/proto_bindings',
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
        '<(proto_in_dir)/binder.proto',
        '<(proto_in_dir)/psyche.proto',
        'common/constants.cc',
        'common/util.cc',
      ],
      'includes': ['../../platform2/common-mk/protoc.gypi'],
    },

    # Shared client library.
    {
      'target_name': 'libpsyche',
      'type': 'shared_library',
      'variables': {
        'exported_deps': [],
      },
      'all_dependent_settings': {
        'variables': {
          'deps': [
            '<@(exported_deps)',
          ],
        },
      },
      'sources': [
        'libpsyche/psyche_connection.cc',
      ],
      'dependencies': [
        'libcommon',
      ],
    },

    # psyched.
    {
      'target_name': 'libpsyched',
      'type': 'static_library',
      'variables': {
        'exported_deps': [],
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
        'psyched/registrar.cc',
        'psyched/service.cc',
      ],
      'dependencies': [
        'libcommon',
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
        # Tests.
        {
          'target_name': 'psyched_test',
          'type': 'executable',
          'includes': [ '../common-mk/common_test.gypi' ],
          'dependencies': [ 'libpsyched' ],
          'sources': [
            'common/testrunner.cc',
            # TODO(derat): All of this code is impossible to test right now
            # because it calls protobinder::BinderToInterface. Write a bunch of
            # tests once libprotobinder is stubbable: http://brbug.com/650
          ],
        },
      ],
    }],
  ],
}
