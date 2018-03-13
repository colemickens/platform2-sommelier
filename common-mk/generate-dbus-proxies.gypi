{
  'variables': {
    'dbus_service_config%': '',
    'proxy_output_file%': '',
    'mock_output_file%': '',
    'proxy_path_in_mocks%': '',
    'generator': '<!(which generate-chromeos-dbus-bindings)',
  },
  'inputs': [
    '<(dbus_service_config)',
    '<(generator)',
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
    ['proxy_path_in_mocks != ""', {
      'action+': [
        '--proxy-path-in-mocks=<(proxy_path_in_mocks)',
      ],
    }],
  ],
  'message': 'Generating DBus proxies C++ headers from >@(_sources)',
  'process_outputs_as_sources': 1,
  'hard_dependency': 1,
}
