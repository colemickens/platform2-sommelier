{
  'variables': {
    'pem_dir': '<(SHARED_INTERMEDIATE_DIR)/<(openssl_pem_out_dir)',
    'openssl_pem_in_dir%': '.',
    'openssl': '<!(which openssl)',
  },
  'rules': [
    {
      'rule_name': 'genopenssl_key',
      'extension': 'pem',
      'inputs': [
        '<(openssl)',
      ],
      'outputs': [
        '<(pem_dir)/<(RULE_INPUT_ROOT).pub.pem',
      ],
      'action': [
        '<(openssl)',
        'rsa',
        '-in', '<(openssl_pem_in_dir)/<(RULE_INPUT_ROOT)<(RULE_INPUT_EXT)',
        '-pubout',
        '-out', '<(pem_dir)/<(RULE_INPUT_ROOT).pub.pem',
      ],
      'msvs_cygwin_shell': 0,
      'message': 'Generating public key from <(RULE_INPUT_PATH)',
      'process_outputs_as_sources': 1,
    },
  ],
}
