{
  'target_defaults': {
    'includes': ['../common-mk/common.gypi'],
    'variables': {
      'deps': [
        'protobuf',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'media_perception_protos',
      'type': 'static_library',
      'variables': {
        'proto_in_dir': 'proto/',
        'proto_out_dir': 'include/media_perception',
      },
      'sources': [
        '<(proto_in_dir)/device_management.proto',
        '<(proto_in_dir)/pipeline.proto',
      ],
      'includes': ['../common-mk/protoc.gypi'],
    },
    {
      'target_name': 'media_perception_mojo_bindings',
      'type': 'static_library',
      'sources': [
        'mojom/color_space.mojom',
        'mojom/constants.mojom',
        'mojom/device.mojom',
        'mojom/device_factory.mojom',
        'mojom/device_management.mojom',
        'mojom/geometry.mojom',
        'mojom/image_capture.mojom',
        'mojom/mailbox.mojom',
        'mojom/mailbox_holder.mojom',
        'mojom/media_perception.mojom',
        'mojom/media_perception_service.mojom',
        'mojom/pipeline.mojom',
        'mojom/producer.mojom',
        'mojom/receiver.mojom',
        'mojom/scoped_access_permission.mojom',
        'mojom/shared_memory.mojom',
        'mojom/sync_token.mojom',
        'mojom/time.mojom',
        'mojom/values.mojom',
        'mojom/video_capture_types.mojom',
        'mojom/virtual_device.mojom',
      ],
      'includes': ['../common-mk/mojom_bindings_generator.gypi'],
    },
    {
      'target_name': 'media_perception_service_lib',
      'type': 'static_library',
      'variables': {
        'exported_deps': [
          'libchrome-<(libbase_ver)',
          'libbrillo-<(libbase_ver)',
          'libmojo-<(libbase_ver)',
        ],
        'deps': ['<@(exported_deps)'],
      },
      'dependencies': [
        'media_perception_mojo_bindings',
        'media_perception_protos',
      ],
      'all_dependent_settings': {
        'variables': {
          'deps': [
            '<@(exported_deps)',
          ],
        },
      },
      'sources': [
        'cras_client_impl.cc',
        'cros_dbus_service.cc',
        'media_perception_controller_impl.cc',
        'media_perception_impl.cc',
        'media_perception_service_impl.cc',
        'mojo_connector.cc',
        'producer_impl.cc',
        'proto_mojom_conversion.cc',
        'receiver_impl.cc',
        'shared_memory_provider.cc',
        'video_capture_service_client_impl.cc',
      ],
      'direct_dependent_settings': {
        'cflags': [
          '<!@(<(pkg-config) --cflags libcras)',
          '<!@(<(pkg-config) --cflags dbus-1)',
        ],
      },
      'link_settings': {
        'ldflags': [
          '<!@(<(pkg-config) --libs-only-L --libs-only-other libcras)',
          '<!@(<(pkg-config) --libs-only-L --libs-only-other dbus-1)',
        ],
        'libraries': [
          '<!@(<(pkg-config) --libs-only-l libcras)',
          '<!@(<(pkg-config) --libs-only-l dbus-1)',
        ],
      },
    },
    {
      'target_name': 'rtanalytics_main',
      'type': 'executable',
      'dependencies': [
        'media_perception_service_lib',
      ],
      'sources': ['main.cc'],
      'link_settings': {
        'ldflags': [
          '-L .',
        ],
        'libraries': [
          '-lrtanalytics -ldl',
        ],
      },
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'media_perception_service_test',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'dependencies': ['media_perception_service_lib'],
          'sources': [
            'fake_video_capture_service_client.cc',
            'media_perception_impl_test.cc',
            'proto_mojom_conversion_test.cc',
            'testrunner.cc',
          ],
        },
      ],
    }],
  ],
}
