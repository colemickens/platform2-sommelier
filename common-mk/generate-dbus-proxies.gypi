{
  'variables': {
    'dbus_service_config%': '',
    'proxy_output_file%': '',
    'mock_output_file%': '',
    'generator': '<!(which generate-chromeos-dbus-bindings)',
  },
  'inputs': [
    '<(dbus_service_config)',
    '<(generator)',
    '>@(_sources)',
  ],
  'action': [
    '<(generator)',
    '>@(_sources)',
    '--service-config=<(dbus_service_config)',
  ],
  'conditions': [
    ['proxy_output_file != ""', {
      'outputs+': [
        '<(SHARED_INTERMEDIATE_DIR)/<(proxy_output_file)',
      ],
      'action+': [
        '--proxy=<(SHARED_INTERMEDIATE_DIR)/<(proxy_output_file)',
      ],
    }],
    ['mock_output_file != ""', {
      'outputs+': [
        '<(SHARED_INTERMEDIATE_DIR)/<(mock_output_file)',
      ],
      'action+': [
        '--mock=<(SHARED_INTERMEDIATE_DIR)/<(mock_output_file)',
      ],
    }],
  ],
  'hard_dependency': 1,
}
