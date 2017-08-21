{
  'target_defaults': {
    'variables': {
      'deps': [
        'dbus-c++-1',
        'glib-2.0',
        'gthread-2.0',
        'gobject-2.0',
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
        'libmtp',
        'libudev',
        # system_api depends on protobuf (or protobuf-lite). It must appear
        # before protobuf here or the linker flags won't be in the right
        # order.
        'system_api',
        'protobuf-lite',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'mtpd-adaptors',
      'type': 'none',
      'variables': {
        'xml2cpp_type': 'adaptor',
        'xml2cpp_in_dir': '.',
        'xml2cpp_out_dir': 'include/mtpd/dbus_adaptors',
      },
      'sources': [
        '<(xml2cpp_in_dir)/dbus_bindings/org.chromium.Mtpd.xml',
      ],
      'includes': ['../../platform2/common-mk/xml2cpp.gypi'],
    },
    {
      'target_name': 'libmtpd',
      'type': 'static_library',
      'dependencies': [
        'mtpd-adaptors',
      ],
      'sources': [
        'assert_matching_file_types.cc',
        'daemon.cc',
        'device_manager.cc',
        'file_entry.cc',
        'mtpd_server_impl.cc',
        'storage_info.cc',
        'string_helpers.cc',
      ],
    },
    {
      'target_name': 'mtpd',
      'type': 'executable',
      'dependencies': [
        'libmtpd',
        'mtpd-adaptors',
      ],
      'sources': [
        'main.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'mtpd_testrunner',
          'type': 'executable',
          'includes': ['../../platform2/common-mk/common_test.gypi'],
          'dependencies': [
            'libmtpd',
            'mtpd-adaptors',
            '../../platform2/common-mk/testrunner.gyp:testrunner',
          ],
          'sources': [
            'device_manager_unittest.cc',
          ],
        },
      ],
    }],
  ],
}
