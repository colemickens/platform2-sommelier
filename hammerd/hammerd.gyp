# TODO: Rename these files to pass this check.
# gyplint: disable=GypLintSourceFileNames

{
  'target_defaults': {
    'variables': {
      'deps': [
        'fmap',
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
        'libmetrics-<(libbase_ver)',
        'libpcrecpp',
        'openssl',
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
      'target_name': 'libhammerd',
      'type': 'static_library',
      'cflags!': ['-fPIE'],
      'cflags': ['-fPIC'],
      'sources': [
        # TODO(crbug.com/649672): Upgrade to OpenSSL 1.1 support curve25519.
        'curve25519.c',
        'dbus_wrapper.cc',
        'fmap_utils.cc',
        'hammer_updater.cc',
        'pair_utils.cc',
        'process_lock.cc',
        'update_fw.cc',
        'usb_utils.cc',
      ],
    },
    {
      'target_name': 'hammerd',
      'type': 'executable',
      'dependencies': ['libhammerd'],
      'sources': [
        'main.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_hammerd_api == 1', {
      'targets': [
        {
          'target_name': 'libhammerd-api',
          'type': 'shared_library',
          'dependencies': ['libhammerd'],
          'sources': [
            'hammerd_api.cc',
          ],
        },
      ],
    }],
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'unittest_runner',
          'type': 'executable',
          'dependencies': [
            'libhammerd',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'curve25519_unittest.cc',
            'hammer_updater_unittest.cc',
            'pair_utils_unittest.cc',
            'update_fw_unittest.cc',
          ],
        },
      ],
    }],
  ],
}
