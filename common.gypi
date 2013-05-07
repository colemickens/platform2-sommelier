# This file is included for every single target.
# Use it to define Platform2 wide settings and defaults.

{
  'variables': {
    # Set this to 1 if you'd like to enable -Werror.
    'enable_werror%': 0,

    # Set this to 1 if you'd like to enable debugging flags.
    'debug%': 0,

    # DON'T EDIT BELOW THIS LINE.
    # These arguments shouldn't be changed here.
    'USE_test%': 0,
    'USE_cros_host%': 0,
    'USE_wimax%': 0,

    'external_cflags%': '',
    'external_cxxflags%': '',
    'external_ldflags%': '',

    'deps%': '',
  },
  'conditions': [
    ['USE_cros_host == 1', {
      'variables': {
        'pkg-config': 'pkg-config',
        'sysroot': '',
      },
    }],
  ],
  'target_defaults': {
    'include_dirs': [
      '<(INTERMEDIATE_DIR)/include',
      '<(SHARED_INTERMEDIATE_DIR)/include',
      '..',
    ],
    'cflags': [
      '-Wall',
      '-Wno-psabi',
      '-ggdb3',
      '-fstack-protector-strong',
      '-Wformat=2',
    ],
    'cflags_c': [
      '<(external_cflags)',
    ],
    'cflags_cc': [
      '<(external_cxxflags)',
    ],
    'link_settings': {
      'ldflags': [
        '<(external_ldflags)',
        '-Wl,-z,relro',
        '-Wl,-z,now',
        '-Wl,--as-needed',
      ],
    },
    'conditions': [
      ['enable_werror == 1', {
        'cflags+': [
          '-Werror',
        ],
      }],
      ['USE_cros_host == 1', {
        'defines': [
          'NDEBUG',
        ],
      }],
      ['USE_cros_host == 0', {
        'include_dirs': [
          '<(sysroot)/usr/include',
        ],
        'cflags': [
          '--sysroot=<(sysroot)',
        ],
        'link_settings': {
          'ldflags': [
            '--sysroot=<(sysroot)',
          ],
        },
      }],
      ['deps != ""', {
        'cflags+': [
          '>!@(<(pkg-config) >(deps) --cflags)',
        ],
        'link_settings': {
          'ldflags+': [
            '>!@(<(pkg-config) >(deps) --libs-only-L --libs-only-other)',
          ],
          'libraries+': [
            '>!@(<(pkg-config) >(deps) --libs-only-l)',
          ],
        },
      }],
    ],
    'target_conditions': [
      ['_type == "executable"', {
        'cflags': ['-fPIE'],
        'ldflags': ['-pie'],
      }],
      ['_type == "static_library"', {
        'cflags': ['-fPIE'],
      }],
      ['_type == "shared_library"', {
        'cflags': ['-fPIC'],
      }],
    ],
  },
}
