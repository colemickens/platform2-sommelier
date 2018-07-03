# Place this .gypi in the 'includes' of a library or executable target to
# process any *.mojom sources into generated .cc and .h files.
{
  'variables': {
    'deps': [
      'libchrome-<(libbase_ver)',
      'libmojo-<(libbase_ver)',
    ],
    'mojo_binding_generator': '<(sysroot)/usr/src/libmojo-<(libbase_ver)/mojo/mojom_bindings_generator.py',
    'mojo_output_dir': '<(SHARED_INTERMEDIATE_DIR)/include',
    'mojo_template_dir': '<(SHARED_INTERMEDIATE_DIR)/templates',
  },
  'actions': [
    {
      'action_name': 'create_templates_dir',
      'inputs': [
      ],
      'outputs': [
        '<(mojo_template_dir)',
      ],
      'message': 'Creating mojo C++ templates dir <(mojo_template_dir)',
      'action': [
        'mkdir', '-p', '<(mojo_template_dir)',
      ],
    },
    {
      'action_name': 'generate_mojom_templates',
      'inputs': [
        '<(mojo_binding_generator)',
        '<(mojo_template_dir)',
      ],
      'outputs': [
        '<(mojo_template_dir)/cpp_templates.zip',
      ],
      'message': 'Generating mojo templates in <(mojo_template_dir)',
      'action': [
        'python', '<(mojo_binding_generator)', '--use_bundled_pylibs',
        'precompile', '-o', '<(mojo_template_dir)',
      ],
    },
  ],
  'rules': [
    {
      'rule_name': 'generate_mojom_bindings',
      'extension': 'mojom',
      'inputs': [
        '<(mojo_binding_generator)',
        '<(mojo_template_dir)/cpp_templates.zip',
      ],
      'outputs': [
        '<(mojo_output_dir)/<(RULE_INPUT_PATH)-internal.h',
        '<(mojo_output_dir)/<(RULE_INPUT_PATH).cc',
        '<(mojo_output_dir)/<(RULE_INPUT_PATH).h',
      ],
      'message': 'Generating mojo C++ bindings for <(RULE_INPUT_PATH)',
      'action': [
        'python', '<(mojo_binding_generator)', '--use_bundled_pylibs',
        'generate', '<(RULE_INPUT_PATH)',
        '--bytecode_path', '<(mojo_template_dir)',
        '-I', '.',
        '--output_dir', '<(mojo_output_dir)',
        '--generators', 'c++',
      ],
      'process_outputs_as_sources': 1,
    },
  ],
}
