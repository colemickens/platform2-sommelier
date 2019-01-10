{
  'targets': [
    {
      'target_name': 'midis_mojo_bindings',
      'type': 'static_library',
      'sources': ['mojo/midis.mojom'],
      'includes': ['../common-mk/mojom_bindings_generator.gypi'],
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
        'client.cc',
        'client_tracker.cc',
        'device.cc',
        'device_tracker.cc',
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
            'tests/client_test.cc',
            'tests/client_tracker_test.cc',
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
