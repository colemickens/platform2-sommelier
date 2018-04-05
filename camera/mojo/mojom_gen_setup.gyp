{
  'includes': [
    '../build/cros-camera-common.gypi',
  ],
  'targets': [
    {
      'target_name': 'setup_mojom_generator',
      'type': 'none',
      'hard_dependency': 1,
      'includes': [
        'mojom_common.gypi',
      ],
      'actions': [
        {
          'action_name': 'mkdir_mojom_templates_gen_dir',
          'inputs': [],
          'outputs': [
            '<(mojom_templates_gen_dir)',
          ],
          'action': [
            'mkdir', '-p', '<(mojom_templates_gen_dir)',
          ],
          'message': 'Creating mojom templates gen dir <(mojom_templates_gen_dir)',
        },
        {
          'action_name': 'precompile_mojom',
          'inputs': [
            '<(mojom_bindings_generator)',
            '<(mojom_templates_gen_dir)',
          ],
          'outputs': [
            '<(mojom_templates_gen_dir)/cpp_templates.zip',
            '<(mojom_templates_gen_dir)/java_templates.zip',
            '<(mojom_templates_gen_dir)/js_templates.zip',
          ],
          'action': [
            'python', '<(mojom_bindings_generator)',
            '--use_bundled_pylibs', 'precompile', '-o', '<(mojom_templates_gen_dir)',
          ],
          'message': 'Precompiling mojom templates to <(mojom_templates_gen_dir)',
        },
      ],
    },
  ],
}
