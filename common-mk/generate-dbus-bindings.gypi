{
  'variables': {
    'h_dir': '<(SHARED_INTERMEDIATE_DIR)/<(generate_dbus_bindings_out_dir)',
    'generate_dbus_bindings_in_dir%': '.',
    'generate_dbus_bindings': '<!(which generate-chromeos-dbus-bindings)',
  },
  'rules': [
    {
      'rule_name': 'generate_dbus_bindings',
      'extension': 'xml',
      'inputs': [
        '<(generate_dbus_bindings)',
      ],
      'outputs': [
        '<(h_dir)/<(RULE_INPUT_ROOT).h',
      ],
      'action': [
        '<(generate_dbus_bindings)',
        '--input=<(RULE_INPUT_PATH)',
        '--<(generate_dbus_bindings_type)=<(h_dir)/<(RULE_INPUT_ROOT).h',
      ],
      'msvs_cygwin_shell': 0,
      'message': 'Generating C++ header from <(RULE_INPUT_PATH)',
      'process_outputs_as_sources': 1,
    },
  ],
  # This target exports a hard dependency because it generates header
  # files.
  'hard_dependency': 1,
}
