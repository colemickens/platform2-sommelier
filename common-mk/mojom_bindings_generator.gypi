# GYP template to generate static library for the given mojom files.
# How to use:
# {
#   'target_name': 'foo_mojo_bindings',
#   'type': 'static_library',
#   'sources': [
#     'mojo/foo.mojom',
#     'mojo/foo_sub.mojom',
#   ],
#   'includes': ['$(PATH_TO_THIS_DIR)/mojom_bindings_generator.gypi'],
# }
#
# Then this generates static library for the mojom files and the header files
# under <(SHARED_INTERMEDIATE_DIR)/include. E.g.
# <(SHARED_INTERMEDIATE_DIR/include/mojo/foo.mojom.h etc., where "mojo"
# directory comes from 'sources' relative path.
{
  'variables': {
    'mojo_output_dir': '<(SHARED_INTERMEDIATE_DIR)/include',
    'mojom_bindings_generator': '<(sysroot)/usr/src/libmojo-<(libbase_ver)/mojo/mojom_bindings_generator.py',
    'mojo_templates_dir': '<(SHARED_INTERMEDIATE_DIR)/templates',
    'mojo_extra_args%': [],
    'deps': [
      'libchrome-<(libbase_ver)',
      'libmojo-<(libbase_ver)',
    ],
  },
  'actions': [
    {
      'action_name': 'mojo_templates_dir',
      'inputs': [],
      'outputs': ['<(mojo_templates_dir)'],
      'message': 'Creating mojo C++ templates dir',
      'action': ['mkdir', '-p', '<(mojo_templates_dir)'],
    },
    {
      'action_name': 'mojo_templates',
      'inputs': [
        '<(mojom_bindings_generator)',
        '<(mojo_templates_dir)',
      ],
      'outputs': ['<(mojo_templates_dir)/cpp_templates.zip'],
      'message': 'Generating mojo C++ templates',
      'action': [
        'python', '<(mojom_bindings_generator)', '--use_bundled_pylibs',
        'precompile', '-o', '<(mojo_templates_dir)',
      ],
    },
  ],
  'rules': [
    {
      'rule_name': 'mojom_bindings_gen',
      'extension': 'mojom',
      'inputs': [
        '<(mojom_bindings_generator)',
        '<(mojo_templates_dir)/cpp_templates.zip',
      ],
      'outputs': [
        '<(mojo_output_dir)/<(RULE_INPUT_PATH)-internal.h',
        '<(mojo_output_dir)/<(RULE_INPUT_PATH).cc',
        '<(mojo_output_dir)/<(RULE_INPUT_PATH).h',
      ],
      'message': 'Generating mojo C++ bindings from <(RULE_INPUT_PATH)',
      'action': [
        'python', '<(mojom_bindings_generator)', '--use_bundled_pylibs',
        'generate', '<(RULE_INPUT_PATH)',
        '--output_dir', '<(mojo_output_dir)',
        '--bytecode_path', '<(mojo_templates_dir)',
        '-I', '.',
        '--generators', 'c++',
        '<@(mojo_extra_args)',
      ],
      'process_outputs_as_sources': 1,
    },
  ],
}
