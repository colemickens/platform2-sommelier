{
  'actions': [
    {
      'action_name': 'write-deps',
      'inputs': [
      ],
      'outputs': [
        '<(SHARED_INTERMEDIATE_DIR)/>(_target_name).txt',
      ],
      'action': ['sh', '-c', 'echo >@(deps) > >@(_outputs)'],
    },
  ]
}
