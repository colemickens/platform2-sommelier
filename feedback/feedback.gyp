# TODO: Rename these files to pass this check.
# gyplint: disable=GypLintSourceFileNames

{
  'target_defaults': {
    'variables': {
      'deps': [
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'libfeedback_daemon',
      'type': 'static_library',
      'dependencies': ['feedback_proto'],
      'sources': [
        'components/feedback/feedback_report.cc',
        'components/feedback/feedback_uploader.cc',
        'feedback_service.cc',
        'feedback_uploader_http.cc',
        'feedback_util.cc',
      ],
      'variables': {
        'exported_deps': [
          'protobuf-lite',
        ],
        'deps': [
          '<@(exported_deps)',
        ],
      },
      'all_dependent_settings': {
        'variables': {
          'deps+': [
            '<@(exported_deps)',
          ],
        },
      },
      'include_dirs': ['.'],
    },
    {
      'target_name': 'feedback_daemon',
      'type': 'executable',
      'dependencies': ['libfeedback_daemon'],
      'sources': [
        'feedback_daemon.cc',
      ],
    },
    {
      'target_name': 'libfeedback_client',
      'type': 'static_library',
      'dependencies': ['feedback_proto'],
      'sources': [
        'components/feedback/feedback_common.cc',
        'feedback_service_interface.cc',
        'feedback_util.cc',
      ],
      'variables': {
        'exported_deps': [
          'protobuf-lite',
        ],
        'deps': [
          '<@(exported_deps)',
        ],
      },
      'all_dependent_settings': {
        'variables': {
          'deps+': [
            '<@(exported_deps)',
          ],
        },
      },
      'include_dirs': ['.'],
    },
    {
      'target_name': 'feedback_client',
      'type': 'executable',
      'dependencies': ['libfeedback_client', 'libfeedback_daemon'],
      'sources': [
        'feedback_client.cc',
      ],
    },
    {
      'target_name': 'feedback_proto',
      'type': 'static_library',
      'variables': {
        'proto_in_dir': 'components/feedback/proto',
        'proto_out_dir': 'include/components/feedback/proto',
      },
      'sources': [
        '<(proto_in_dir)/annotations.proto',
        '<(proto_in_dir)/chrome.proto',
        '<(proto_in_dir)/common.proto',
        '<(proto_in_dir)/config.proto',
        '<(proto_in_dir)/dom.proto',
        '<(proto_in_dir)/extension.proto',
        '<(proto_in_dir)/math.proto',
        '<(proto_in_dir)/web.proto',
      ],
      'includes': [
        '../common-mk/protoc.gypi',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'feedback_daemon_test',
          'type': 'executable',
          'dependencies': [
            'libfeedback_client',
            'libfeedback_daemon',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'feedback_service_unittest.cc',
          ],
        },
      ],
    }],
  ],
}
