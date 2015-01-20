{
  'variables': {
    'h_dir': '<(SHARED_INTERMEDIATE_DIR)/<(dbus_adaptors_out_dir)',
    'dbus_service_config%': '',
    'generator': '<!(which generate-chromeos-dbus-bindings)',
  },
  'rules': [
    {
      'rule_name': 'generate_dbus_adaptors',
      'extension': 'xml',
      'inputs': [
        '<(dbus_service_config)',
        '<(generator)',
        '<(RULE_INPUT_PATH)',
      ],
      'outputs': [
        '<(h_dir)/<(RULE_INPUT_ROOT).h',
      ],
      'action': [
        '<(generator)',
        '<(RULE_INPUT_PATH)',
        '--service-config=<(dbus_service_config)',
        '--adaptor=<(h_dir)/<(RULE_INPUT_ROOT).h',
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
