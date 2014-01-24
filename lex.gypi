{
  'variables': {
    'cc_dir': '<(SHARED_INTERMEDIATE_DIR)/lex/<(lexer_out_dir)',
    'lexer_in_dir%': '.',
    'lex': '<!(which flex)',
  },
  'rules': [
    {
      'rule_name': 'lex',
      'extension': 'l',
      'inputs': [
        '<(lex)',
      ],
      'outputs': [
        '<(cc_dir)/<(RULE_INPUT_ROOT).c',
      ],
      'action': [
        '<(lex)',
        '-o', '<(cc_dir)/<(RULE_INPUT_ROOT).c',
        '<(lexer_in_dir)/<(RULE_INPUT_ROOT)<(RULE_INPUT_EXT)',
      ],
      'msvs_cygwin_shell': 0,
      'message': 'Generating lexer from <(RULE_INPUT_PATH)',
      'process_outputs_as_sources': 1,
    },
  ],
}
