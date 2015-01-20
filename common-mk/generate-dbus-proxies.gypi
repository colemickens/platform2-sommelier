{
  'variables': {
    'dbus_service_config%': '',
    'generator': '<!(which generate-chromeos-dbus-bindings)',
  },
  'inputs': [
    '<(dbus_service_config)',
    '<(generator)',
    '>@(_sources)',
  ],
  'outputs': [
    '<(SHARED_INTERMEDIATE_DIR)/<(proxy_output_file)',
  ],
  'action': [
    '<(generator)',
    '>@(_sources)',
    '--service-config=<(dbus_service_config)',
    '--proxy=>(_outputs)'
  ],
  'hard_dependency': 1,
}
