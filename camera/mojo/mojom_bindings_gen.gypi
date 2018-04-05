{
  'hard_dependency': 1,
  'includes': [
    'mojom_common.gypi',
  ],
  'dependencies': [
    '<(src_root_path)/mojo/mojom_gen_setup.gyp:setup_mojom_generator',
  ],
  'rules': [
    {
      'rule_name': 'generate_mojom_bindings',
      'extension': 'mojom',
      'inputs': [
        '<(mojom_bindings_generator)',
      ],
      'outputs': [
        '<(RULE_INPUT_PATH)-internal.h',
        '<(RULE_INPUT_PATH).cc',
        '<(RULE_INPUT_PATH).h',
      ],
      'action': [
        'python', '<(mojom_bindings_generator)', '--use_bundled_pylibs',
        'generate', '<(RULE_INPUT_PATH)',

        # Set platform/arc-camera as the root for include path.
        '-I', '<(src_root_path)',

        '--bytecode_path', '<(mojom_templates_gen_dir)',
        '-g', 'c++',
      ],
      'message': 'Generating C++ mojom templates from <(RULE_INPUT_PATH)',
      'process_outputs_as_sources': 1,
    },
  ],
  'include_dirs': [
    '<(src_root_path)/mojo',
  ],
}
