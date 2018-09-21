{
  'target_defaults': {
    'variables': {
      'deps': [
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
        # system_api depends on protobuf (or protobuf-lite). It must
        # appear before protobuf or the linker flags won't be in the right
        # order.
        'system_api',
        'protobuf-lite',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'oobe_config_restore_adaptors',
      'type': 'none',
      'variables': {
        'dbus_adaptors_out_dir': 'include/dbus_adaptors',
        'dbus_service_config': 'dbus_bindings/dbus-service-config.json',
        'new_fd_bindings': 1,
      },
      'sources': [
        'dbus_bindings/org.chromium.OobeConfigRestore.xml',
      ],
      'includes': ['../common-mk/generate-dbus-adaptors.gypi'],
    },
   # oobe_config library.
    {
      'target_name': 'liboobeconfig',
      'type': 'static_library',
      'dependencies': [
        'oobe_config_proto',
      ],
      'variables': {
        'gen_src_in_dir': '<(SHARED_INTERMEDIATE_DIR)/include/bindings',
        'deps': [
          'dbus-1',
        ],
      },
      'sources': [
        'load_oobe_config_usb.cc',
        'oobe_config.cc',
        'usb_common.cc',
        'utils.cc',
      ],
      'link_settings': {
        'libraries': [
          '-lpolicy-<(libbase_ver)',
        ],
      },
    },
    {
      'target_name': 'finish_oobe_auto_config',
      'type': 'executable',
      'sources': [
        'finish_oobe_auto_config.cc',
        'usb_common.cc',
        'utils.cc',
      ],
    },
    {
      'target_name': 'oobe_config_save',
      'type': 'executable',
      'dependencies': [
        'liboobeconfig',
      ],
      'sources': [
        'oobe_config_save_main.cc',
      ],
    },
    {
      'target_name': 'oobe_config_restore',
      'type': 'executable',
      'dependencies': [
        'liboobeconfig',
        'oobe_config_restore_adaptors',
      ],
      'sources': [
        'oobe_config_restore_main.cc',
        'oobe_config_restore_service.cc',
      ],
    },
    {
      'target_name': 'oobe_config_proto',
      'type': 'static_library',
      'variables': {
        'proto_in_dir': '.',
        'proto_out_dir': 'include/oobe_config',
      },
      'sources': [
        '<(proto_in_dir)/rollback_data.proto',
      ],
      'includes': [
        '../common-mk/protoc.gypi',
      ],
    },

  ],
  # Unit tests.
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'oobe_config_test',
          'type': 'executable',
          'dependencies': [
            'liboobeconfig',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'variables': {
            'deps': [
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'load_oobe_config_usb_test.cc',
            'oobe_config_test.cc',
          ],
        },
      ],
    }],
  ],
}
