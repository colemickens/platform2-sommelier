{
  'targets': [
    # Library with generated gRPC API definitions.
    {
      'target_name': 'diagnostics_grpc_protos',
      'type': 'static_library',
      'variables': {
        'proto_in_dir': 'grpc',
        'proto_out_dir': 'include',
        'gen_grpc': 1,
        'exported_deps': [
          'grpc++',
          'protobuf',
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
        '<(proto_in_dir)/diagnostics_processor.proto',
        '<(proto_in_dir)/diagnosticsd.proto',
      ],
      'includes': [
        '../common-mk/protoc.gypi',
      ],
    },
    # Library that adopts gRPC core async interface to a libchrome-friendly one.
    {
      'target_name': 'libgrpc_async_adapter',
      'type': 'static_library',
      'variables': {
        'exported_deps': [
          'grpc++',
          'libchrome-<(libbase_ver)',
        ],
        'deps': [
          '<@(exported_deps)',
        ],
      },
      'link_settings': {
        'libraries': [
          '-lgpr',
        ],
      },
      'all_dependent_settings': {
        'variables': {
          'deps': [
            '<@(exported_deps)',
          ],
        },
      },
      'sources': [
        'grpc_async_adapter/async_grpc_client.cc',
        'grpc_async_adapter/async_grpc_server.cc',
        'grpc_async_adapter/grpc_completion_queue_dispatcher.cc',
        'grpc_async_adapter/rpc_state.cc',
      ],
    },
    # Library with generated Mojo API definitions.
    {
      'target_name': 'diagnostics_mojo_bindings',
      'type': 'static_library',
      'direct_dependent_settings': {
        'variables': {
          'deps': [
            'libchrome-<(libbase_ver)',
            'libmojo-<(libbase_ver)',
          ],
        },
      },
      'sources': ['mojo/diagnosticsd.mojom'],
      'includes': ['../common-mk/mojom_bindings_generator.gypi'],
    },
    # Library that provides core functionality for the diagnosticsd daemon.
    {
      'target_name': 'libdiagnosticsd',
      'type': 'static_library',
      'dependencies': [
        'diagnostics_grpc_protos',
        'diagnostics_mojo_bindings',
        'libgrpc_async_adapter',
      ],
      'variables': {
        'exported_deps': [
          'dbus-1',
          'libbrillo-<(libbase_ver)',
          'libchrome-<(libbase_ver)',
          'libmojo-<(libbase_ver)',
          'system_api',
        ],
        'deps': [
          '<@(exported_deps)',
        ],
      },
      'all_dependent_settings': {
        'variables': {
          'deps': [
            '<@(exported_deps)',
          ],
        },
      },
      'sources': [
        'diagnosticsd/diagnosticsd_core.cc',
        'diagnosticsd/diagnosticsd_core_delegate_impl.cc',
        'diagnosticsd/diagnosticsd_daemon.cc',
        'diagnosticsd/diagnosticsd_dbus_service.cc',
        'diagnosticsd/diagnosticsd_grpc_service.cc',
        'diagnosticsd/diagnosticsd_mojo_service.cc',
        'diagnosticsd/mojo_utils.cc',
      ],
    },
    # Library that provides the DPSL (diagnostics processor support library)
    # interface.
    {
      'target_name': 'libdpsl',
      'type': 'static_library',
      'dependencies': [
        'diagnostics_grpc_protos',
        'libgrpc_async_adapter',
      ],
      'variables': {
        'exported_deps': [
          'libbrillo-<(libbase_ver)',
          'libchrome-<(libbase_ver)',
        ],
        'deps': [
          '<@(exported_deps)',
        ],
      },
      'link_settings': {
        'variables': {
          'deps': [
            '<@(exported_deps)',
          ],
        },
      },
      'sources': [
        'dpsl/internal/dpsl_global_context_impl.cc',
        'dpsl/internal/dpsl_requester_impl.cc',
        'dpsl/internal/dpsl_rpc_server_impl.cc',
        'dpsl/internal/dpsl_thread_context_impl.cc',
      ],
    },
    # Library that provides core functionality for the telemetry tool.
    {
      'target_name': 'libtelem',
      'type': 'static_library',
      'dependencies': [
        'diagnostics_grpc_protos',
        'libgrpc_async_adapter',
      ],
      'variables': {
        'exported_deps': [
          'libbrillo-<(libbase_ver)',
          'libchrome-<(libbase_ver)',
        ],
        'deps': [
          '<@(exported_deps)',
        ],
      },
      'all_dependent_settings': {
        'variables': {
          'deps': [
            '<@(exported_deps)',
          ],
        },
      },
      'sources': [
        'telem/async_grpc_client_adapter_impl.cc',
        'telem/telem_cache.cc',
        'telem/telem_connection.cc',
      ],
    },
    # The diagnosticsd daemon executable.
    {
      'target_name': 'diagnosticsd',
      'type': 'executable',
      'dependencies': [
        'libdiagnosticsd',
      ],
      'variables': {
        'deps': [
          'libbrillo-<(libbase_ver)',
        ],
      },
      'sources': [
        'diagnosticsd/main.cc',
      ],
    },
    # The diagnostics_processor daemon executable.
    {
      'target_name': 'diagnostics_processor',
      'type': 'executable',
      'dependencies': [
        'libgrpc_async_adapter',
      ],
      'variables': {
        'deps': [
          'libbrillo-<(libbase_ver)',
        ],
      },
      'sources': [
        'diagnostics_processor/main.cc',
      ],
    },
    # Executables for DPSL demos.
    {
      'target_name': 'diagnostics_dpsl_demo_single_thread',
      'type': 'executable',
      'dependencies': [
        'libdpsl',
      ],
      'sources': [
        'dpsl/demo_single_thread/main.cc',
      ],
    },
    # The telemetry tool executable.
    {
      'target_name': 'telem',
      'type': 'executable',
      'dependencies': [
        'libtelem',
      ],
      'variables': {
        'deps': [
          'libbrillo-<(libbase_ver)',
          'libchrome-<(libbase_ver)',
        ],
      },
      'sources': [
        'telem/main.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        # Libraries for unit tests.
        {
          'target_name': 'libgrpc_async_adapter_test_rpcs',
          'type': 'static_library',
          'variables': {
            'proto_in_dir': 'grpc_async_adapter',
            'proto_out_dir': 'include',
            'gen_grpc': 1,
            'exported_deps': [
              'grpc++',
              'protobuf',
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
            'grpc_async_adapter/test_rpcs.proto',
          ],
          'includes': [
            '../common-mk/protoc.gypi',
          ],
        },
        # Unit tests.
        {
          'target_name': 'libgrpc_async_adapter_test',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'dependencies': [
            '../common-mk/testrunner.gyp:testrunner',
            'libgrpc_async_adapter',
            'libgrpc_async_adapter_test_rpcs',
          ],
          'sources': [
            'grpc_async_adapter/async_grpc_client_server_test.cc',
            'grpc_async_adapter/async_grpc_server_test.cc',
            'grpc_async_adapter/grpc_completion_queue_dispatcher_test.cc',
          ],
          'variables': {
            'deps': [
              'libchrome-<(libbase_ver)',
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'link_settings': {
            'libraries': [
              '-lgpr',
            ],
          },
        },
        {
          'target_name': 'diagnosticsd_test',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'dependencies': [
            '../common-mk/testrunner.gyp:testrunner',
            'libdiagnosticsd',
          ],
          'sources': [
            'diagnosticsd/diagnosticsd_core_test.cc',
            'diagnosticsd/diagnosticsd_dbus_service_test.cc',
            'diagnosticsd/diagnosticsd_grpc_service_test.cc',
            'diagnosticsd/diagnosticsd_mojo_service_test.cc',
            'diagnosticsd/fake_browser.cc',
            'diagnosticsd/fake_diagnostics_processor.cc',
            'diagnosticsd/file_test_utils.cc',
            'diagnosticsd/mojo_test_utils.cc',
            'diagnosticsd/mojo_utils_test.cc',
          ],
          'variables': {
            'deps': [
              'libchrome-<(libbase_ver)',
              'libchrome-test-<(libbase_ver)',
            ],
          },
        },
        {
          'target_name': 'libtelem_test',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'dependencies': [
            '../common-mk/testrunner.gyp:testrunner',
            'libtelem',
          ],
          'sources': [
            'telem/telem_cache_test.cc',
            'telem/telem_connection_test.cc',
          ],
          'variables': {
            'deps': [
              'libchrome-<(libbase_ver)',
              'libchrome-test-<(libbase_ver)',
            ],
          },
        },
      ],
    }],
  ],
}
