# TODO: Rename these files to pass this check.
# gyplint: disable=GypLintSourceFileNames

# TODO: Fix the visibility on these shared libs.
# gyplint: disable=GypLintVisibilityFlags

{
  'target_defaults': {
    'variables': {
      'deps': [
        'dbus-c++-1',
        'gthread-2.0',
        'gobject-2.0',
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
        'libmetrics-<(libbase_ver)',
        'libminijail',
      ],
      # cromo uses try/catch to interact with dbus-c++.
      'enable_exceptions': 1,
    },
    'link_settings': {
      'libraries': [
        '-ldl',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'cromo-adaptors',
      'type': 'none',
      'variables': {
        'xml2cpp_type': 'adaptor',
        'xml2cpp_in_dir': 'local-xml',
        'xml2cpp_out_dir': 'include/cromo/dbus_adaptors',
      },
      'sources': [
        '<(xml2cpp_in_dir)/org.freedesktop.DBus.Properties.xml',
      ],
      'includes': ['../common-mk/xml2cpp.gypi'],
    },
    {
      'target_name': 'libcromo',
      'type': 'static_library',
      'standalone_static_library': 1, # tells GYP to not make this a 'thin' library
      'dependencies': [
        '../common-mk/external_dependencies.gyp:modemmanager-dbus-adaptors',
        'cromo-adaptors',
      ],
      'sources': [
        'cromo_server.cc',
        'hooktable.cc',
        'modem_handler.cc',
        'sms_cache.cc',
        'sms_message.cc',
        'syslog_helper.cc',
        'utilities.cc',
      ],
    },
    {
      'target_name': 'cromo',
      'type': 'executable',
      # cromo needs to export symbols, as specified in cromo.ver, to its
      # plugins. gyp currently does not link a static library with
      # --whole-archive, which causes some unused symbols in libcromo.a get
      # removed. As a workaround, the 'cromo' target explicitly takes the
      # dependencies and sources from the 'libcromo' target, instead of
      # depending on 'libcromo'.
      'dependencies': [
        '../common-mk/external_dependencies.gyp:modemmanager-dbus-adaptors',
        'cromo-adaptors',
      ],
      'defines': [
        'PLUGINDIR="<(libdir)/cromo/plugins"',
      ],
      'cflags': [
        '-fvisibility=default',
      ],
      'ldflags': [
        '-Wl,--dynamic-list-cpp-typeinfo,--dynamic-list=<(platform2_root)/cromo/cromo.ver',
      ],
      'libraries': [
        '-lpthread',
      ],
      'sources': [
        'carrier.cc',
        'cromo_server.cc',
        'hooktable.cc',
        'main.cc',
        'modem_handler.cc',
        'plugin_manager.cc',
        'sandbox.cc',
        'sms_cache.cc',
        'sms_message.cc',
        'syslog_helper.cc',
        'utilities.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'dummy_modem',
          'type': 'shared_library',
          'dependencies': ['../common-mk/external_dependencies.gyp:modemmanager-dbus-adaptors'],
          'sources': [
            'dummy_modem.cc',
            'dummy_modem_handler.cc',
          ],
        },
        {
          'target_name': 'cromo_server_unittest',
          'type': 'executable',
          'dependencies': ['libcromo'],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'carrier.cc',
            'cromo_server_unittest.cc',
          ],
        },
        {
          'target_name': 'utilities_unittest',
          'type': 'executable',
          'dependencies': ['libcromo'],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'utilities_unittest.cc',
          ],
        },
        {
          'target_name': 'sms_message_unittest',
          'type': 'executable',
          'dependencies': ['libcromo'],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'sms_message_unittest.cc',
          ],
        },
        {
          'target_name': 'sms_cache_unittest',
          'type': 'executable',
          'dependencies': ['libcromo'],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'sms_cache_unittest.cc',
          ],
        },
      ],
    }],
  ],
}
