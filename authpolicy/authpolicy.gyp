{
  'target_defaults': {
    'variables': {
      'deps': [
        'dbus-1',
        'dbus-c++-1',
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
      ],
    },
  },
  'targets': [

    # Main programs.
    {
      'target_name': 'authpolicyd',
      'type': 'executable',
      'variables': {
        'dbus_adaptors_out_dir': 'include/authpolicy',
      },
      'sources': [
        'authpolicy.cc',
        'dbus_bindings/org.chromium.authpolicy.Domain.xml',
      ],
      'includes': ['../common-mk/generate-dbus-adaptors.gypi'],
    }
  ]
}
