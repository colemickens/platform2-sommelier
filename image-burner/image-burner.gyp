# TODO: Rename these files to pass this check.
# gyplint: disable=GypLintSourceFileNames

{
  'target_defaults': {
    'link_settings': {
      'libraries': [
        '-lrootdev',
      ],
    },
    'variables': {
      'deps': [
        'dbus-1',
        'dbus-glib-1',
        'gobject-2.0',
        'libbrillo-<(libbase_ver)',
        'libbrillo-glib-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'image-burner-dbus-server',
      'type': 'none',
      'variables': {
        'dbus_glib_type': 'server',
        'dbus_glib_out_dir': 'include/bindings',
        'dbus_glib_prefix': 'image_burner',
      },
      'sources': [
        'image_burner.xml',
      ],
      'includes': ['../common-mk/dbus_glib.gypi'],
    },
    {
      'target_name': 'libimage-burner',
      'type': 'static_library',
      'dependencies': [
        'image-burner-dbus-server',
      ],
      'cflags': [
        # The generated dbus headers use "register".
        '-Wno-deprecated-register',
      ],
      'sources': [
        'image_burn_service.cc',
        'image_burner.cc',
        'image_burner_impl.cc',
        'image_burner_utils.cc',
      ],
    },
    {
      'target_name': 'image_burner',
      'type': 'executable',
      'dependencies': [
        'libimage-burner',
      ],
      'sources': [
        'image_burner_main.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'unittest_runner',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'dependencies': [
            'libimage-burner',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'sources': [
            'image_burner_impl_unittest.cc',
            'image_burner_utils_unittest.cc',
          ],
        },
      ],
    }],
  ],
}
