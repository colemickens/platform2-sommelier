{
  'variables': {
    'mojo_output_dir': '<(SHARED_INTERMEDIATE_DIR)/include',
  },
  'targets': [
    {
      'target_name': 'midis_mojo_bindings',
      'type': 'none',
      'variables': {
        'mojo_binding_generator': '<(sysroot)/usr/src/libmojo-<(libbase_ver)/mojo/mojom_bindings_generator.py',
        'mojo_template_dir': '<(SHARED_INTERMEDIATE_DIR)/templates',
      },
      'actions': [
        {
          'action_name': 'midis_mojom_templates_dir',
          'inputs': [
            '<(mojo_binding_generator)',
          ],
          'outputs': [
            '<(mojo_template_dir)',
          ],
          'message': 'Creating mojo C++ templates dir',
          'action': [
            'mkdir', '-p', '<(mojo_template_dir)',
          ],
        },
        {
          'action_name': 'midis_mojom_templates',
          'inputs': [
            '<(mojo_binding_generator)',
            '<(mojo_template_dir)',
          ],
          'outputs': [
            '<(mojo_template_dir)/cpp_templates.zip',
          ],
          'message': 'Generating mojo C++ templates',
          'action': [
            'python', '<(mojo_binding_generator)', '--use_bundled_pylibs',
            'precompile', '-o', '<(mojo_template_dir)',
          ],
        },
        {
          'action_name': 'midis_mojom_bindings',
          'inputs': [
            '<(mojo_binding_generator)',
            '<(mojo_template_dir)/cpp_templates.zip',
            'mojo/midis.mojom',
          ],
          'outputs': [
            '<(mojo_output_dir)/mojo/midis.mojom.h',
            '<(mojo_output_dir)/mojo/midis.mojom.cc',
          ],
          'message': 'Generating mojo C++ bindings for midis',
          'action': [
            'python', '<(mojo_binding_generator)',
            '--use_bundled_pylibs', 'generate', 'mojo/midis.mojom',
            '-o', '<(mojo_output_dir)',
            '--bytecode_path', '<(mojo_template_dir)',
            '-g', 'c++',
          ],
        },
      ],
    },
    {
      'target_name': 'midis_common',
      'type': 'static_library',
      'dependencies': [
        'midis_mojo_bindings',
      ],
      'variables': {
        'exported_deps': [
          'libbrillo-<(libbase_ver)',
          'libchrome-<(libbase_ver)',
          'libmojo-<(libbase_ver)',
          'alsa',
        ],
        'deps': ['>@(exported_deps)'],
      },
      'all_dependent_settings': {
        'variables': {
          'deps': [
            '<@(exported_deps)',
          ],
        },
      },
      'sources': [
        # Files included from Chrome //src/media/midi checkout.
        # This directory is placed in platform2 as platform2/media/midi.
        '../media/midi/message_util.cc',
        '../media/midi/midi_message_queue.cc',
        '<(mojo_output_dir)/mojo/midis.mojom.cc',
        'client.cc',
        'client_tracker.cc',
        'device.cc',
        'device_tracker.cc',
        'file_handler.cc',
        'ports.cc',
        'seq_handler.cc',
        'subdevice_client_fd_holder.cc',
      ],
    },
    {
      'target_name': 'midis',
      'type': 'executable',
      'dependencies' : [
        'midis_common',
      ],
      'link_settings': {
        'libraries': [
          '-ldl',
        ],
      },
      'sources': [
        'daemon.cc',
        'main.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'midis_testrunner',
          'type': 'executable',
          'dependencies' : [
            '../common-mk/testrunner.gyp:testrunner',
            'midis_common',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            # TODO(pmalani): Re-enable tests.
            # 'tests/client_test.cc',
            'tests/client_tracker_test.cc',
            # 'tests/device_test.cc',
            'tests/device_tracker_test.cc',
            'tests/seq_handler_test.cc',
            'tests/test_helper.cc',
          ],
        },
      ],
    }],
    # Fuzzer target.
    ['USE_fuzzer == 1', {
      'targets': [
        {
          'target_name': 'midis_seq_handler_fuzzer',
          'type': 'executable',
          'dependencies': [
            'midis_common',
          ],
          'includes': ['../common-mk/common_fuzzer.gypi'],
          'sources': [
            'seq_handler_fuzzer.cc',
          ],
        },
      ],
    }],
  ],
}
