# Caution!: GYP to GN migration is happening. If you update this file, please
# update common-mk/mojom_bindings_generator.gni too accordingly.

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
    'mojom_bindings_generator_wrapper': '<(platform2_root)/common-mk/mojom_bindings_generator_wrapper.py',
    'mojo_templates_dir': '<(SHARED_INTERMEDIATE_DIR)/templates',
    'mojo_extra_args%': [],
    'mojo_root%': '.',
    'exported_deps': [
      'libchrome-<(libbase_ver)',
      'libmojo-<(libbase_ver)',
    ],
    'deps': ['<@(exported_deps)'],
  },
  'all_dependent_settings': {
    'variables': {
      'deps': ['<@(exported_deps)'],
    },
  },

  # mojom generates header file, so dependency between static libs is also
  # needed.
  'hard_dependency': 1,

  # This gypi generates header files under <(SHARED_INTERMEDIATE_DIR)/include.
  'direct_dependent_settings': {
    'include_dirs': ['<(SHARED_INTERMEDIATE_DIR)/include'],
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
      'variables': {
        'mojo_path': '<!(["realpath", "--relative-to=<(mojo_root)", "<(RULE_INPUT_PATH)"])',
        # Workaround. gyp looks to have an issue to expand variables in action.
        'mojo_include_path': '<(mojo_root)',
        'mojo_depth': '<(mojo_root)',
      },
      'outputs': [
        '<(mojo_output_dir)/<(mojo_path)-internal.h',
        '<(mojo_output_dir)/<(mojo_path)-shared.cc',
        '<(mojo_output_dir)/<(mojo_path)-shared.h',
        '<(mojo_output_dir)/<(mojo_path).cc',
        '<(mojo_output_dir)/<(mojo_path).h',
      ],
      'message': 'Generating mojo C++ bindings from >(mojo_path)',
      'action': [
        'python', '<(mojom_bindings_generator_wrapper)',
        '<(mojom_bindings_generator)', '--use_bundled_pylibs',
        'generate', '<(RULE_INPUT_PATH)',
        '--output_dir', '<(mojo_output_dir)',
        '--bytecode_path', '<(mojo_templates_dir)',
        '-I', '>(mojo_include_path)',
        '-d', '>(mojo_depth)',
        '--generators', 'c++',
        '<@(mojo_extra_args)',
      ],
      'process_outputs_as_sources': 1,
    },
  ],
}
