{
  'targets': [
    {
      'target_name': 'ml_service',
      'type': 'executable',
      'includes': [ 'mojom_generator.gypi' ],
      'variables': {
        'deps': [
          'libbrillo-<(libbase_ver)',
          'libchrome-<(libbase_ver)',
          'libmojo-<(libbase_ver)',
        ],
      },
      'sources': [
        'daemon.cc',
        'machine_learning_service_impl.cc',
        'main.cc',
        'mojom/graph_executor.mojom',
        'mojom/machine_learning_service.mojom',
        'mojom/model.mojom',
        'mojom/tensor.mojom',
      ],
    },
  ],
}
