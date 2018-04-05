{
  'type': 'none',
  'actions': [
    {
      'action_name': 'copy',
      'inputs': [
        '<(src)',
      ],
      'outputs': [
        '<(dst)',
      ],
      'action': [
        'cp', '<(src)', '<(dst)',
      ],
      'message': 'Copying static library <(src) to <(dst)',
    },
  ],
}
