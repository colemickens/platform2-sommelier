{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)'
      ],
    },
    'include_dirs': [
      '../libbrillo',
    ],
    'defines': [
      'USE_DBUS=<(USE_dbus)',
      'USE_RTTI_FOR_TYPE_TAGS',
    ],
  },
  'targets': [
    {
      'target_name': 'libbrillo-<(libbase_ver)',
      'type': 'none',
      'dependencies': [
        'libbrillo-blockdeviceutils-<(libbase_ver)',
        'libbrillo-core-<(libbase_ver)',
        'libbrillo-cryptohome-<(libbase_ver)',
        'libbrillo-http-<(libbase_ver)',
        'libbrillo-minijail-<(libbase_ver)',
        'libbrillo-streams-<(libbase_ver)',
        'libinstallattributes-<(libbase_ver)',
        'libpolicy-<(libbase_ver)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../libbrillo',
        ],
      },
      'includes': ['../common-mk/deps.gypi'],
    },
    {
      'target_name': 'libbrillo-core-<(libbase_ver)',
      'type': 'shared_library',
      'variables': {
        'exported_deps': [
        ],
        'conditions': [
          ['USE_dbus == 1', {
            'exported_deps': [
              'dbus-1',
            ],
          }],
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
      'libraries': ['-lmodp_b64'],
      'sources': [
        'brillo/asynchronous_signal_handler.cc',
        'brillo/backoff_entry.cc',
        'brillo/daemons/daemon.cc',
        'brillo/data_encoding.cc',
        'brillo/errors/error.cc',
        'brillo/errors/error_codes.cc',
        'brillo/file_utils.cc',
        'brillo/flag_helper.cc',
        'brillo/key_value_store.cc',
        'brillo/message_loops/base_message_loop.cc',
        'brillo/message_loops/message_loop.cc',
        'brillo/message_loops/message_loop_utils.cc',
        'brillo/mime_utils.cc',
        'brillo/osrelease_reader.cc',
        'brillo/process.cc',
        'brillo/process_reaper.cc',
        'brillo/process_information.cc',
        'brillo/secure_blob.cc',
        'brillo/strings/string_utils.cc',
        'brillo/syslog_logging.cc',
        'brillo/type_name_undecorate.cc',
        'brillo/url_utils.cc',
        'brillo/userdb_utils.cc',
        'brillo/value_conversion.cc',
      ],
      'conditions': [
        ['USE_dbus == 1', {
          'sources': [
            'brillo/any.cc',
            'brillo/daemons/dbus_daemon.cc',
            'brillo/dbus/async_event_sequencer.cc',
            'brillo/dbus/data_serialization.cc',
            'brillo/dbus/dbus_connection.cc',
            'brillo/dbus/dbus_method_invoker.cc',
            'brillo/dbus/dbus_method_response.cc',
            'brillo/dbus/dbus_object.cc',
            'brillo/dbus/dbus_service_watcher.cc',
            'brillo/dbus/dbus_signal.cc',
            'brillo/dbus/exported_object_manager.cc',
            'brillo/dbus/exported_property_set.cc',
            'brillo/dbus/utils.cc',
          ],
        }],
      ],
    },
    {
      'target_name': 'libbrillo-blockdeviceutils-<(libbase_ver)',
      'type': 'shared_library',
      'dependencies': ['libbrillo-core-<(libbase_ver)'],
      'sources': [
        'brillo/blkdev_utils/loop_device.cc',
      ],
      'conditions': [
        ['USE_device_mapper == 1', {
          'variables': { 'deps': ['devmapper'] },
          'sources': [
            'brillo/blkdev_utils/device_mapper.cc',
            'brillo/blkdev_utils/device_mapper_task.cc',
          ]
        }],
      ],
    },
    {
      'target_name': 'libbrillo-http-<(libbase_ver)',
      'type': 'shared_library',
      'dependencies': [
        'libbrillo-core-<(libbase_ver)',
        'libbrillo-streams-<(libbase_ver)',
      ],
      'variables': {
        'exported_deps': [
          'libcurl',
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
        'brillo/http/curl_api.cc',
        'brillo/http/http_connection_curl.cc',
        'brillo/http/http_form_data.cc',
        'brillo/http/http_request.cc',
        'brillo/http/http_transport.cc',
        'brillo/http/http_transport_curl.cc',
        'brillo/http/http_utils.cc',
      ],
      'conditions': [
        ['USE_dbus == 1', {
          'sources': [
            'brillo/http/http_proxy.cc',
          ],
        }],
      ],
    },
    {
      'target_name': 'libbrillo-streams-<(libbase_ver)',
      'type': 'shared_library',
      'dependencies': [
        'libbrillo-core-<(libbase_ver)',
      ],
      'variables': {
        'exported_deps': [
          'openssl',
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
        'brillo/streams/file_stream.cc',
        'brillo/streams/input_stream_set.cc',
        'brillo/streams/memory_containers.cc',
        'brillo/streams/memory_stream.cc',
        'brillo/streams/openssl_stream_bio.cc',
        'brillo/streams/stream.cc',
        'brillo/streams/stream_errors.cc',
        'brillo/streams/stream_utils.cc',
        'brillo/streams/tls_stream.cc',
      ],
    },
    {
      'target_name': 'libbrillo-test-<(libbase_ver)',
      'type': 'static_library',
      'standalone_static_library': 1,
      'dependencies': [
        'libbrillo-http-<(libbase_ver)',
      ],
      'sources': [
        'brillo/blkdev_utils/loop_device_fake.cc',
        'brillo/http/http_connection_fake.cc',
        'brillo/http/http_transport_fake.cc',
        'brillo/message_loops/fake_message_loop.cc',
        'brillo/streams/fake_stream.cc',
        'brillo/unittest_utils.cc',
      ],
      'includes': ['../common-mk/deps.gypi'],
      'conditions': [
        ['USE_device_mapper == 1', {
          'sources': [
            'brillo/blkdev_utils/device_mapper_fake.cc',
          ]
        }],
      ],
    },
    {
      'target_name': 'libbrillo-cryptohome-<(libbase_ver)',
      'type': 'shared_library',
      'variables': {
        'exported_deps': [
          'openssl',
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
        'brillo/cryptohome.cc',
      ],
    },
    {
      'target_name': 'libbrillo-minijail-<(libbase_ver)',
      'type': 'shared_library',
      'variables': {
        'exported_deps': [
          'libminijail',
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
      'cflags': [
        '-fvisibility=default',
      ],
      'sources': [
        'brillo/minijail/minijail.cc',
      ],
    },
    {
      'target_name': 'libinstallattributes-<(libbase_ver)',
      'type': 'shared_library',
      'dependencies': [
        'libinstallattributes-includes',
        '../common-mk/external_dependencies.gyp:install_attributes-proto',
      ],
      'variables': {
        'exported_deps': [
          'protobuf-lite',
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
        'install_attributes/libinstallattributes.cc',
      ],
    },
    {
      'target_name': 'libpolicy-<(libbase_ver)',
      'type': 'shared_library',
      'dependencies': [
        'libinstallattributes-<(libbase_ver)',
        'libpolicy-includes',
        '../common-mk/external_dependencies.gyp:policy-protos',
      ],
      'variables': {
        'exported_deps': [
          'openssl',
          'protobuf-lite',
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
      'ldflags': [
        '-Wl,--version-script,<(platform2_root)/libbrillo/libpolicy.ver',
      ],
      'sources': [
        'policy/device_policy.cc',
        'policy/device_policy_impl.cc',
        'policy/policy_util.cc',
        'policy/resilient_policy_util.cc',
        'policy/libpolicy.cc',
      ],
    },
    {
      'target_name': 'libbrillo-glib-<(libbase_ver)',
      'type': 'shared_library',
      'dependencies': [
        'libbrillo-<(libbase_ver)',
      ],
      'variables': {
        'exported_deps': [
          'glib-2.0',
          'gobject-2.0',
        ],
        'conditions': [
          ['USE_dbus == 1', {
            'exported_deps': [
              'dbus-1',
              'dbus-glib-1',
            ],
          }],
        ],
        'deps': ['<@(exported_deps)'],
      },
      'cflags': [
        # glib uses the deprecated "register" attribute in some header files.
        '-Wno-deprecated-register',
      ],
      'all_dependent_settings': {
        'variables': {
          'deps': [
            '<@(exported_deps)',
          ],
        },
      },
      'includes': ['../common-mk/deps.gypi'],
      'conditions': [
        ['USE_dbus == 1', {
          'sources': [
            'brillo/glib/abstract_dbus_service.cc',
            'brillo/glib/dbus.cc',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'libbrillo-<(libbase_ver)_tests',
          'type': 'executable',
          'dependencies': [
            'libbrillo-<(libbase_ver)',
            'libbrillo-glib-<(libbase_ver)',
            'libbrillo-test-<(libbase_ver)',
          ],
          'variables': {
            'deps': [
              'libchrome-test-<(libbase_ver)',
            ],
            'proto_in_dir': 'brillo/dbus',
            'proto_out_dir': 'include/brillo/dbus',
          },
          'includes': [
            '../common-mk/common_test.gypi',
            '../common-mk/protoc.gypi',
          ],
          'cflags': [
            '-Wno-format-zero-length',
          ],
          'conditions': [
            ['debug == 1', {
              'cflags': [
                '-fprofile-arcs',
                '-ftest-coverage',
                '-fno-inline',
              ],
              'libraries': [
                '-lgcov',
              ],
            }],
          ],
          'sources': [
            'brillo/asynchronous_signal_handler_test.cc',
            'brillo/backoff_entry_test.cc',
            'brillo/blkdev_utils/loop_device_test.cc',
            'brillo/data_encoding_test.cc',
            'brillo/enum_flags_test.cc',
            'brillo/errors/error_codes_test.cc',
            'brillo/errors/error_test.cc',
            'brillo/file_utils_test.cc',
            'brillo/flag_helper_test.cc',
            'brillo/glib/object_test.cc',
            'brillo/http/http_connection_curl_test.cc',
            'brillo/http/http_form_data_test.cc',
            'brillo/http/http_request_test.cc',
            'brillo/http/http_transport_curl_test.cc',
            'brillo/http/http_utils_test.cc',
            'brillo/key_value_store_test.cc',
            'brillo/map_utils_test.cc',
            'brillo/message_loops/base_message_loop_test.cc',
            'brillo/message_loops/fake_message_loop_test.cc',
            'brillo/message_loops/message_loop_test.cc',
            'brillo/mime_utils_test.cc',
            'brillo/osrelease_reader_test.cc',
            'brillo/process_reaper_test.cc',
            'brillo/process_test.cc',
            'brillo/secure_blob_test.cc',
            'brillo/streams/fake_stream_test.cc',
            'brillo/streams/file_stream_test.cc',
            'brillo/streams/input_stream_set_test.cc',
            'brillo/streams/memory_containers_test.cc',
            'brillo/streams/memory_stream_test.cc',
            'brillo/streams/openssl_stream_bio_test.cc',
            'brillo/streams/stream_test.cc',
            'brillo/streams/stream_utils_test.cc',
            'brillo/strings/string_utils_test.cc',
            'brillo/unittest_utils.cc',
            'brillo/url_utils_test.cc',
            'brillo/value_conversion_test.cc',
            'testrunner.cc',
          ],
          'conditions': [
            ['USE_dbus == 1', {
              'sources': [
                'brillo/any_test.cc',
                'brillo/any_internal_impl_test.cc',
                'brillo/dbus/async_event_sequencer_test.cc',
                'brillo/dbus/data_serialization_test.cc',
                'brillo/dbus/dbus_method_invoker_test.cc',
                'brillo/dbus/dbus_object_test.cc',
                'brillo/dbus/dbus_param_reader_test.cc',
                'brillo/dbus/dbus_param_writer_test.cc',
                'brillo/dbus/dbus_signal_handler_test.cc',
                'brillo/dbus/exported_object_manager_test.cc',
                'brillo/dbus/exported_property_set_test.cc',
                'brillo/http/http_proxy_test.cc',
                'brillo/type_name_undecorate_test.cc',
                'brillo/variant_dictionary_test.cc',
                '<(proto_in_dir)/test.proto',
              ],
            }],
            ['USE_device_mapper == 1', {
              'sources': [
                'brillo/blkdev_utils/device_mapper_test.cc',
              ]
            }],
          ],
        },
        {
          'target_name': 'libinstallattributes-<(libbase_ver)_tests',
          'type': 'executable',
          'dependencies': [
            '../common-mk/external_dependencies.gyp:install_attributes-proto',
            'libinstallattributes-<(libbase_ver)',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'install_attributes/tests/libinstallattributes_test.cc',
          ]
        },
        {
          'target_name': 'libpolicy-<(libbase_ver)_tests',
          'type': 'executable',
          'dependencies': [
            '../common-mk/external_dependencies.gyp:install_attributes-proto',
            '../common-mk/external_dependencies.gyp:policy-protos',
            'libinstallattributes-<(libbase_ver)',
            'libpolicy-<(libbase_ver)',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'install_attributes/mock_install_attributes_reader.cc',
            'policy/tests/device_policy_impl_test.cc',
            'policy/tests/libpolicy_test.cc',
            'policy/tests/policy_util_test.cc',
            'policy/tests/resilient_policy_util_test.cc',
          ]
        },
      ],
    }],
  ],
}
