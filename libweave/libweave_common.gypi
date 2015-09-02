{
  'target_defaults': {
    'configurations': {
      'Release': {
        'defines': [
          'NDEBUG',
        ],
        'cflags': [
          '-Os',
        ],
      },
      'Debug': {
        'defines': [
          '_DEBUG',
        ],
        'cflags': [
          '-Og',
        ],
      },
    },
    'include_dirs': [
      '.',
      '..',
      'include',
      'external',
      'third_party/include',
      'third_party/modp_b64/modp_b64',
    ],
    'cflags!': ['-fPIE'],
    'cflags': [
      '-fPIC',
      '-fvisibility=hidden',
      '-Wl,--exclude-libs,ALL',
      '--std=c++11',
      '-Wno-format-nonliteral',
      '-Wno-char-subscripts',
      #'-Wno-deprecated-register',
    ],
    'libraries': [
      '-L../../third_party/lib',
    ],
  },
}
