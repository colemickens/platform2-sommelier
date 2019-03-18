# Caution!: GYP to GN migration is happening. If you update this file, please
# update common-mk/proto_library.gni too accordingly.
{
  'variables': {
    'cc_dir': '<(SHARED_INTERMEDIATE_DIR)/<(proto_out_dir)',
    'go_dir': '<(SHARED_INTERMEDIATE_DIR)/<(proto_out_dir)',
    'python_dir': '<(SHARED_INTERMEDIATE_DIR)/<(proto_out_dir)/py',
    'proto_in_dir%': '.',
    'protoc': '<!(which protoc)',
    'grpc_cpp_plugin': '<!(which grpc_cpp_plugin)',
    'gen_go%': 0,
    'gen_grpc%': 0,
    'gen_grpc_gmock%': 0,
    'gen_go_grpc%': 0,
    'gen_python%': 0,
  },
  'rules': [
    {
      'rule_name': 'genproto',
      'extension': 'proto',
      'inputs': [
        '<(protoc)',
      ],
      'conditions': [
        ['gen_go==1', {
          'variables': {
            'out_args': ['--go_out', '<(go_dir)'],
          },
          'outputs': [
            '<(go_dir)/<(RULE_INPUT_ROOT).pb.go',
          ],
        }],
        ['gen_python==1', {
          'variables': {
            'out_args': ['--python_out', '<(python_dir)'],
          },
          'outputs': [
            '<(python_dir)/<(RULE_INPUT_ROOT)_pb.py',
          ],
        }],
        ['gen_grpc==1', {
          'variables': {
            'out_args': [
              '--plugin=protoc-gen-grpc=<(grpc_cpp_plugin)',
              '--cpp_out=<(cc_dir)',
            ],
          },
          'outputs': [
            '<(cc_dir)/<(RULE_INPUT_ROOT).grpc.pb.cc',
            '<(cc_dir)/<(RULE_INPUT_ROOT).grpc.pb.h',
            '<(cc_dir)/<(RULE_INPUT_ROOT).pb.cc',
            '<(cc_dir)/<(RULE_INPUT_ROOT).pb.h',
          ],
          'conditions': [
            ['gen_grpc_gmock==0', {
              'variables': {
                'out_args': ['--grpc_out=<(cc_dir)',],
              },
            }],
            ['gen_grpc_gmock==1', {
              'variables': {
                'out_args': ['--grpc_out=generate_mock_code=true:<(cc_dir)',],
              },
              'outputs': ['<(cc_dir)/<(RULE_INPUT_ROOT)_mock.grpc.pb.h',],
            }],
          ],
        }],
        ['gen_go_grpc==1', {
          'variables': {
            'out_args': [
              '--go_out=plugins=grpc:<(go_dir)',
            ],
          },
          'outputs': [
            '<(go_dir)/<(RULE_INPUT_ROOT).pb.go',
          ],
        }],
        ['gen_grpc==0 and gen_go_grpc==0 and gen_go==0 and gen_python==0', {
          'outputs': [
            '<(cc_dir)/<(RULE_INPUT_ROOT).pb.cc',
            '<(cc_dir)/<(RULE_INPUT_ROOT).pb.h',
          ],
          'variables': {
            'out_args': ['--cpp_out', '<(cc_dir)'],
          },
        }],
      ],
      'action': [
        '<(protoc)',
        '--proto_path','<(proto_in_dir)',
        '--proto_path','<(sysroot)/usr/share/proto',
        '<(proto_in_dir)/<(RULE_INPUT_ROOT)<(RULE_INPUT_EXT)',
        '>@(out_args)',
      ],
      'msvs_cygwin_shell': 0,
      'message': 'Generating C++ code from <(RULE_INPUT_PATH)',
      'process_outputs_as_sources': 1,
    },
  ],
  # This target exports a hard dependency because it generates header
  # files.
  'hard_dependency': 1,
  'direct_dependent_settings': {
    'include_dirs': [
      '<(SHARED_INTERMEDIATE_DIR)',
    ],
  },
  # Protobuf 3.7.0 introduced generated code that is sometimes unreachable.
  'cflags_cc': [ "-Wno-unreachable-code" ],
}
