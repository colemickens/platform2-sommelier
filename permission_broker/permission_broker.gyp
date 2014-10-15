{
  'target_defaults': {
    'variables': {
      'deps': [
        'dbus-1',
        'dbus-glib-1',
        'glib-2.0',
        'gobject-2.0',
        'libchrome-<(libbase_ver)',
        'libchromeos-<(libbase_ver)',
        'libudev',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'libpermission_broker',
      'type': 'static_library',
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
        'tty_subsystem_udev_rule.cc',
        'udev_rule.cc',
        'udev_scopers.cc',
        'usb_subsystem_udev_rule.cc',
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
          'target_name': 'permission_broker_unittest',
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
            'permission_broker_unittest.cc',
            'run_all_tests.cc',
          ],
        },
      ],
    }],
  ],
}
