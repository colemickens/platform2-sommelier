{
  'variables': {
    'cc_dir': '<(SHARED_INTERMEDIATE_DIR)/<(proto_out_dir)',
    'proto_in_dir%': '.',
    'protoc': '<!(which protoc)',
  },
  'rules': [
    {
      'rule_name': 'genproto',
      'extension': 'proto',
      'inputs': [
        '<(protoc)',
      ],
      'outputs': [
        '<(cc_dir)/<(RULE_INPUT_ROOT).pb.cc',
        '<(cc_dir)/<(RULE_INPUT_ROOT).pb.h',
      ],
      'action': [
        '<(protoc)',
        '--proto_path','<(proto_in_dir)',
        '<(proto_in_dir)/<(RULE_INPUT_ROOT)<(RULE_INPUT_EXT)',
        '--cpp_out', '<(cc_dir)',
      ],
      'msvs_cygwin_shell': 0,
      'message': 'Generating C++ code from <(RULE_INPUT_PATH)',
      'process_outputs_as_sources': 1,
    },
  ],
  # This target exports a hard dependency because it generates header
  # files.
  'hard_dependency': 1,
}
