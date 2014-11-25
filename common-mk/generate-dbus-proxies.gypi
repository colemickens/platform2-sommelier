{
  'variables': {
    'dbus_service_config%': '',
  },
  'inputs': [
    '<(dbus_service_config)',
    '>@(_sources)',
  ],
  'outputs': [
    '<(SHARED_INTERMEDIATE_DIR)/<(proxy_output_file)',
  ],
  'action': [
    '<!(which generate-chromeos-dbus-bindings)',
    '>@(_sources)',
    '--service-config=<(dbus_service_config)',
    '--proxy=>(_outputs)'
  ],
  'hard_dependency': 1,
}
