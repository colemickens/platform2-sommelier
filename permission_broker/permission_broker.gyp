{
  'target_defaults': {
    'variables': {
      'deps': [
        'dbus-1',
        'libchrome-<(libbase_ver)',
        'libchromeos-<(libbase_ver)',
        'libudev',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'permission_broker_adaptors',
      'type': 'none',
      'variables': {
        'dbus_adaptors_out_dir': 'include/permission_broker/dbus_adaptors',
      },
      'sources': [
        'dbus_bindings/org.chromium.PermissionBroker.xml',
      ],
      'includes': ['../common-mk/generate-dbus-adaptors.gypi'],
    },
    {
      'target_name': 'libpermission_broker',
      'type': 'static_library',
      'dependencies': ['permission_broker_adaptors'],
      'sources': [
        'allow_group_tty_device_rule.cc',
        'allow_hidraw_device_rule.cc',
        'allow_tty_device_rule.cc',
        'allow_usb_device_rule.cc',
        'deny_claimed_hidraw_device_rule.cc',
        'deny_claimed_usb_device_rule.cc',
        'deny_group_tty_device_rule.cc',
        'deny_uninitialized_device_rule.cc',
        'deny_unsafe_hidraw_device_rule.cc',
        'deny_usb_device_class_rule.cc',
        'deny_usb_vendor_id_rule.cc',
        'hidraw_subsystem_udev_rule.cc',
        'permission_broker.cc',
        'rule.cc',
        'rule_engine.cc',
        'tty_subsystem_udev_rule.cc',
        'udev_rule.cc',
        'udev_scopers.cc',
        'usb_subsystem_udev_rule.cc',
      ],
      'actions': [
        {
          'action_name': 'generate-firewalld-proxies',
          'variables': {
            'proxy_output_file': 'include/firewalld/dbus-proxies.h'
          },
          'sources': [
            '../firewalld/dbus_bindings/org.chromium.Firewalld.xml',
          ],
          'includes': ['../common-mk/generate-dbus-proxies.gypi'],
        },
      ],
    },
    {
      'target_name': 'permission_broker',
      'type': 'executable',
      'dependencies': ['libpermission_broker'],
      'sources': ['permission_broker_main.cc'],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'rule_engine_unittest',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'dependencies': ['libpermission_broker'],
          'sources': [
            'allow_tty_device_rule_unittest.cc',
            'allow_usb_device_rule_unittest.cc',
            'deny_claimed_hidraw_device_rule_unittest.cc',
            'deny_claimed_usb_device_rule_unittest.cc',
            'deny_unsafe_hidraw_device_rule_unittest.cc',
            'deny_usb_device_class_rule_unittest.cc',
            'deny_usb_vendor_id_rule_unittest.cc',
            'group_tty_device_rule_unittest.cc',
            'rule_engine_unittest.cc',
            'run_all_tests.cc',
          ],
        },
      ],
    }],
  ],
}
