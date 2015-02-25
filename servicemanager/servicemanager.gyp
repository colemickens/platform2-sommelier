{
  'targets': [
    {
      'target_name': 'libsimplebinder',
      'type': 'shared_library',
      'sources': [
        'simplebinder.c',
      ],
    },
    {
      'target_name': 'servicemanager',
      'type': 'executable',
      'dependencies': [
        'libsimplebinder',
      ],
      'sources': ['service_manager.c'],
    },
    {
      'target_name': 'service',
      'type': 'executable',
      'dependencies': [
        'libsimplebinder',
      ],
      'sources': ['service.c'],
    },
  ],
}
