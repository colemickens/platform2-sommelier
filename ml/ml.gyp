{
  'targets': [
    {
      'target_name': 'ml_common',
      'type': 'static_library',
      'includes': [ 'mojom_generator.gypi' ],
      'include_dirs': [
        '<(sysroot)/usr/include/tensorflow',
        '<(sysroot)/usr/include/tensorflow/third_party/flatbuffers',
      ],
      'link_settings': {
        'libraries': [ '-ltensorflow_lite' ],
      },
      'variables': {
        'exported_deps': [
          'libbrillo-<(libbase_ver)',
          'libchrome-<(libbase_ver)',
          'libmojo-<(libbase_ver)',
        ],
        'deps': ['<@(exported_deps)'],
      },
      'all_dependent_settings': {
        'variables': {
          'deps': [
            '<@(exported_deps)',
          ],
        },
      },
      'sources': [
        'daemon.cc',
        'graph_executor_impl.cc',
        'machine_learning_service_impl.cc',
        'mojom/graph_executor.mojom',
        'mojom/machine_learning_service.mojom',
        'mojom/model.mojom',
        'mojom/tensor.mojom',
        'tensor_view.cc',
      ],
    },
    {
      'target_name': 'ml_service',
      'type': 'executable',
      'dependencies': ['ml_common'],
      'sources': ['main.cc'],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'ml_service_test',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'include_dirs': [
            '<(sysroot)/usr/include/tensorflow',
            '<(sysroot)/usr/include/tensorflow/third_party/flatbuffers',
          ],
          'dependencies': ['ml_common'],
          'sources': ['graph_executor_impl_test.cc'],
        },
      ],
    }],
  ],
}
