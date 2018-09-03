# Caution!: GYP to GN migration is happening. If you update this file, please
# update common-mk/xml2cpp.gni too accordingly.
{
  'variables': {
    'h_dir': '<(SHARED_INTERMEDIATE_DIR)/<(xml2cpp_out_dir)',
    'xml2cpp_extension%': 'xml',
    'xml2cpp_in_dir%': '.',
    'dbusxx-xml2cpp': '<!(which dbusxx-xml2cpp)',
  },
  'rules': [
    {
      'rule_name': 'genxml2cpp',
      'extension': '<(xml2cpp_extension)',
      'inputs': [
        '<(dbusxx-xml2cpp)',
      ],
      'outputs': [
        '<(h_dir)/<(RULE_INPUT_ROOT).h',
      ],
      'action': [
        '<(dbusxx-xml2cpp)',
        '<(RULE_INPUT_PATH)',
        '--<(xml2cpp_type)=<(h_dir)/<(RULE_INPUT_ROOT).h',
        '--sync',
        '--async',
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
