{
  'target_defaults': {
    'variables': {
      'deps': [
        'dbus-1',
        'openssl',
        'libbuffet-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
        'libchromeos-<(libbase_ver)',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'privetd_common',
      'type': 'static_library',
      'sources': [
        'constants.cc',
        'cloud_delegate.cc',
        'device_delegate.cc',
        'privet_handler.cc',
        'security_delegate.cc',
        'wifi_bootstrap_manager.cc',
      ],
      'actions': [
        {
          'action_name': 'generate-buffet-proxies',
          'variables': {
            # Workaround for issues with ../ in generate-chromeos-dbus-bindings.
            'depth_abs': '<!(realpath <(DEPTH))',
          },
          'inputs': [
            '<(depth_abs)/buffet/dbus_bindings/org.chromium.Buffet.Command.xml',
            '<(depth_abs)/buffet/dbus_bindings/org.chromium.Buffet.Manager.xml',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/include/buffet/dbus-proxies.h',
          ],
          'action': [
            '<!(which generate-chromeos-dbus-bindings)',
            '>@(_inputs)',
            '--proxy=>(_outputs)'
          ],
          'hard_dependency': 1,
        },
      ],
      'includes': ['../common-mk/deps.gypi'],
    },
    {
      'target_name': 'privetd',
      'type': 'executable',
      'dependencies': [
        'privetd_common',
      ],
      'variables': {
        'exported_deps': [
          'libwebserv-<(libbase_ver)',
        ],
        'deps': ['<@(exported_deps)'],
      },
      'sources': [
        'main.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'privetd_testrunner',
          'type': 'executable',
          'dependencies': [
            'privetd_common',
          ],
          'variables': {
            'deps': [
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'privetd_testrunner.cc',
            'privet_handler_unittest.cc',
            'security_delegate_unittest.cc',
          ],
        },
      ],
    }],
  ],
}
