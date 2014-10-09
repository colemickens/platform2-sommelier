{
  'variables': {
    'h_dir': '<(SHARED_INTERMEDIATE_DIR)/<(dbus_glib_out_dir)',
    'dbus_glib_type%': 'client',  # options are 'client' or 'server'.
    'dbus-binding-tool': '<!(which dbus-binding-tool)',
  },
  'rules': [
    {
      'rule_name': 'gendbus_glib',
      'extension': 'xml',
      'inputs': [
        '<(dbus-binding-tool)',
      ],
      'outputs': [
        '<(h_dir)/<(RULE_INPUT_ROOT).dbus<(dbus_glib_type).h',
      ],
      'action': [
        '<(dbus-binding-tool)',
        '<(RULE_INPUT_PATH)',
        '--mode=glib-<(dbus_glib_type)',
        '--prefix=<(dbus_glib_prefix)',
        '--output=<(h_dir)/<(RULE_INPUT_ROOT).dbus<(dbus_glib_type).h',
      ],
      'msvs_cygwin_shell': 0,
      'message':
        'Generating <(dbus_glib_type)-side C++ header from <(RULE_INPUT_PATH)',
      'process_outputs_as_sources': 1,
    },
  ],
  # This target exports a hard dependency because it generates header
  # files.
  'hard_dependency': 1,
}

