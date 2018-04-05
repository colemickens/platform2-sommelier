{
  'type': 'static_library',
  'standalone_static_library': 1,
  'cflags!': [
    '-fPIE',
  ],
  'cflags': [
    '-fPIC',
  ],
}
