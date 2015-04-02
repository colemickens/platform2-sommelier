# Code shared by the libpsyche and psyche packages.
{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
        'libchromeos-<(libbase_ver)',
      ],
    },
    'include_dirs': [
      'lib',
    ],
  },
  'targets': [
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
  ],
}
