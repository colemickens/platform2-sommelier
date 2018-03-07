# Executable targets that belong to the cryptohome-dev-utils package. This gets
# included only in dev and test images of Chromium OS.
{
  'includes': ['cryptohome-libs.gypi'],
  'targets': [
    {
      'target_name': 'cryptohome-tpm-live-test',
      'type': 'executable',
      'dependencies': [
        'libcrostpm',
      ],
      'variables': {
        'deps': [
          'libbrillo-<(libbase_ver)',
          'libchrome-<(libbase_ver)',
          'libmetrics-<(libbase_ver)',
          'openssl',
          'protobuf',
        ],
      },
      'sources': [
        'cryptohome-tpm-live-test.cc',
        'tpm_live_test.cc',
      ],
    },
  ],
}
