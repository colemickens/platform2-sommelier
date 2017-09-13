{
  'variables': {
    'USE_cert_provision%': 0,
  },
  'target_defaults': {
    'defines': [
      'USE_TPM2=<(USE_tpm2)',
    ],
  },
  'targets': [
    # Protobufs.
    {
      'target_name': 'cryptohome-proto-external',
      'type': 'none',
      'variables': {
        'shared_proto_path': '<(sysroot)/usr/include/chromeos/dbus/cryptohome',
        'proto_out_dir': '.',
      },
      'actions': [
        {
          'action_name': 'proto-mung',
          'inputs': [
            '<(shared_proto_path)/key.proto',
            '<(shared_proto_path)/rpc.proto',
            '<(shared_proto_path)/signed_secret.proto',
          ],
          'outputs': [
            '<(proto_out_dir)/key.proto',
            '<(proto_out_dir)/rpc.proto',
            '<(proto_out_dir)/signed_secret.proto',
          ],
          'action': [
            'sh', '-c',
            'cp <@(_inputs) <(proto_out_dir)/ && sed -i s:LITE_RUNTIME:CODE_SIZE:g <@(_outputs)',
          ],
        },
      ],
    },
    {
      'target_name': 'cryptohome-proto',
      'type': 'static_library',
      # libcryptohome-proto.a is used by a shared_libary
      # object, so we need to build it with '-fPIC' instead of '-fPIE'.
      'cflags!': ['-fPIE'],
      'cflags': ['-fPIC'],
      'variables': {
        'proto_in_dir': '.',
        'proto_out_dir': 'include',
      },
      'dependencies': [
        'cryptohome-proto-external',
      ],
      'sources': [
        '<(proto_in_dir)/attestation.proto',
        '<(proto_in_dir)/boot_lockbox_key.proto',
        '<(proto_in_dir)/install_attributes.proto',
        '<(proto_in_dir)/tpm_status.proto',
        '<(proto_in_dir)/vault_keyset.proto',
        'key.proto',
        'rpc.proto',
        'signed_secret.proto',
      ],
      'includes': [
        '../common-mk/protoc.gypi',
      ],
    },

    # D-Bus glib bindings.
    {
      'target_name': 'cryptohome-dbus-client',
      'type': 'none',
      'variables': {
        'dbus_glib_type': 'client',
        'dbus_glib_out_dir': 'include/bindings',
        'dbus_glib_prefix': 'cryptohome',
      },
      'sources': [
        'cryptohome.xml',
      ],
      'includes': ['../common-mk/dbus_glib.gypi'],
    },
    {
      'target_name': 'cryptohome-dbus-server',
      'type': 'none',
      'variables': {
        'dbus_glib_type': 'server',
        'dbus_glib_out_dir': 'include/bindings',
        'dbus_glib_prefix': 'cryptohome',
      },
      'sources': [
        'cryptohome.xml',
      ],
      'includes': ['../common-mk/dbus_glib.gypi'],
    },

    # Common objects.
    {
      'target_name': 'libcrostpm',
      'type': 'static_library',
      'dependencies': [
        'cryptohome-proto',
      ],
      'link_settings': {
        'libraries': [
          '-lsecure_erase_file',
        ],
      },
      'variables': {
        'deps': [
          'libchrome-<(libbase_ver)',
          'libmetrics-<(libbase_ver)',
        ],
      },
      'sources': [
        'attestation.cc',
        'boot_lockbox.cc',
        'crc32.c',
        'crc8.c',
        'crypto.cc',
        'cryptohome_metrics.cc',
        'cryptolib.cc',
        'dircrypto_util.cc',
        'firmware_management_parameters.cc',
        'install_attributes.cc',
        'lockbox.cc',
        'pkcs11_init.cc',
        'pkcs11_keystore.cc',
        'platform.cc',
        'tpm.cc',
        'tpm_init.cc',
      ],
      'conditions': [
        ['USE_tpm2 == 1', {
          'sources': [
            'tpm2_impl.cc',
            'tpm2_metrics.cc',
          ],
          'all_dependent_settings': {
            'libraries': [
              '-ltrunks',
              '-ltpm_manager',
            ],
          },
        }],
        ['USE_tpm2 == 0', {
          'sources': [
            'tpm_impl.cc',
            'tpm_metrics.cc',
          ],
          'all_dependent_settings': {
            'libraries': [
              '-ltspi',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'libcryptohome',
      'type': 'static_library',
      'dependencies': [
        'cryptohome-dbus-server',
        'cryptohome-proto',
      ],
      'variables': {
        'deps': [
          'libchrome-<(libbase_ver)',
        ],
      },
      'cflags': [
        # The generated dbus headers use "register".
        '-Wno-deprecated-register',
      ],
      'sources': [
        'attestation_task.cc',
        'boot_attributes.cc',
        'chaps_client_factory.cc',
        'crypto.cc',
        'cryptohome_event_source.cc',
        'dbus_transition.cc',
        'dircrypto_data_migrator/atomic_flag.cc',
        'dircrypto_data_migrator/migration_helper.cc',
        'homedirs.cc',
        'interface.cc',
        'lockbox-cache-tpm.cc',
        'lockbox-cache.cc',
        'migration_type.h',
        'mount.cc',
        'mount_factory.cc',
        'mount_stack.cc',
        'mount_task.cc',
        'service.cc',
        'service_monolithic.cc',
        'stateful_recovery.cc',
        'user_oldest_activity_timestamp_cache.cc',
        'user_session.cc',
        'username_passkey.cc',
        'vault_keyset.cc',
        'vault_keyset_factory.cc',
      ],
      'conditions': [
        ['USE_tpm2 == 1', {
          'sources': [
            'service_distributed.cc',
          ],
          'all_dependent_settings': {
            'libraries': [
              '-lattestation',
            ],
          },
        }],
      ],
    },

    # Main programs.
    {
      'target_name': 'cryptohome',
      'type': 'executable',
      'dependencies': [
        'cryptohome-dbus-client',
        'cryptohome-proto',
        'cryptohome-proto-external',
        'libcrostpm',
        'libcryptohome',
      ],
      'link_settings': {
        'libraries': [
          '-lchaps',
          '-lkeyutils',
          '-lpolicy-<(libbase_ver)',
          '-lpthread',
          '-lscrypt',
          '-lvboot_host',
        ],
      },
      'variables': {
        'deps': [
          'dbus-1',
          'dbus-glib-1',
          'glib-2.0',
          'libbrillo-<(libbase_ver)',
          'libbrillo-glib-<(libbase_ver)',
          'libchrome-<(libbase_ver)',
          'libecryptfs',
          'libmetrics-<(libbase_ver)',
          'openssl',
          'protobuf',
        ],
      },
      'sources': [
        'cryptohome.cc',
        'tpm_live_test.cc',
      ],
    },
    {
      'target_name': 'cryptohome-path',
      'type': 'executable',
      'dependencies': [
        'libcrostpm',
        'libcryptohome',
      ],
      'variables': {
        'deps': [
          'libbrillo-<(libbase_ver)',
          'libchrome-<(libbase_ver)',
        ],
      },
      'sources': [
        'cryptohome-path.cc',
      ],
    },
    {
      'target_name': 'cryptohomed',
      'type': 'executable',
      'dependencies': [
        'libcrostpm',
        'libcryptohome',
      ],
      'link_settings': {
        'libraries': [
          '-lchaps',
          '-lkeyutils',
          '-lpolicy-<(libbase_ver)',
          '-lpthread',
          '-lscrypt',
          '-lvboot_host',
        ],
      },
      'variables': {
        'deps': [
          'dbus-1',
          'dbus-glib-1',
          'glib-2.0',
          'libbrillo-<(libbase_ver)',
          'libbrillo-glib-<(libbase_ver)',
          'libchrome-<(libbase_ver)',
          'libecryptfs',
          'libmetrics-<(libbase_ver)',
          'openssl',
          # system_api depends on protobuf (or protobuf-lite). It must appear
          # before protobuf here or the linker flags won't be in the right
          # order.
          'system_api',
          'protobuf',
        ],
      },
      'sources': [
        'cryptohomed.cc',
      ],
    },
    {
      'target_name': 'lockbox-cache',
      'type': 'executable',
      'dependencies': [
        'cryptohome-proto',
        'libcrostpm',
      ],
      'link_settings': {
        'libraries': [
          '-lkeyutils',
          '-lsecure_erase_file',
          '-lvboot_host',
        ],
      },
      'variables': {
        'deps': [
          'libbrillo-<(libbase_ver)',
          'libchrome-<(libbase_ver)',
          'libecryptfs',
          'libmetrics-<(libbase_ver)',
          'openssl',
        ],
      },
      'sources': [
        'crc32.c',
        'dircrypto_util.cc',
        'lockbox-cache-main.cc',
        'lockbox-cache-tpm.cc',
        'lockbox-cache.cc',
        'lockbox.cc',
        'platform.cc',
      ],
    },
    {
      'target_name': 'mount-encrypted',
      'type': 'executable',
      'link_settings': {
        'libraries': [
          '-lvboot_host',
        ],
      },
      'variables': {
        'deps': [
          'glib-2.0',
          'openssl',
        ],
      },
      'sources': [
        'mount-encrypted.c',
        'mount-helpers.c',
      ],
    },
    {
      'target_name': 'tpm-manager',
      'type': 'executable',
      'dependencies': [
        'libcrostpm',
      ],
      'variables': {
        'deps': [
          'glib-2.0',
          'libbrillo-<(libbase_ver)',
          'libbrillo-glib-<(libbase_ver)',
          'libchrome-<(libbase_ver)',
          'libecryptfs',
          'libmetrics-<(libbase_ver)',
          'openssl',
          'protobuf',
        ],
      },
      'sources': [
        'tpm_manager.cc',
      ],
      'conditions': [
        ['USE_tpm2 == 1', {
          'sources': [
            'tpm_manager_v2.cc',
          ],
          'link_settings': {
            'libraries': [
              '-ltrunks',
              '-ltpm_manager',
              '-lattestation',
            ],
          },
        }],
        ['USE_tpm2 == 0', {
          'sources': [
            'tpm_manager_v1.cc',
          ],
          'link_settings': {
            'libraries': [
              '-lchaps',
              '-lscrypt',
              '-lvboot_host',
            ],
          },
        }],
      ],
    },
  ],
  'conditions': [
    ['USE_cert_provision == 1', {
      'targets': [
        {
          'target_name': 'cert_provision-proto',
          'type': 'static_library',
          # libcert_provision-proto.a is used by a shared_libary
          # object, so we need to build it with '-fPIC' instead of '-fPIE'.
          'cflags!': ['-fPIE'],
          'cflags': ['-fPIC'],
          'variables': {
            'proto_in_dir': '.',
            'proto_out_dir': 'include',
          },
          'sources': [
            'cert_provision.proto',
          ],
          'includes': [
            '../common-mk/protoc.gypi',
          ],
        },
        {
          'target_name': 'cert_provision',
          'type': 'shared_library',
          'dependencies': [
            'cert_provision-static',
          ],
        },
        {
          'target_name': 'cert_provision-static',
          'type': 'static_library',
          # libcert_provision-static.a is used by a shared_libary
          # object, so we need to build it with '-fPIC' instead of '-fPIE'.
          'cflags!': ['-fPIE'],
          'cflags': ['-fPIC'],
          'dependencies': [
            'cryptohome-dbus-client',
            'cryptohome-proto',
            'cert_provision-proto',
          ],
          'link_settings': {
            'libraries': [
              '-lchaps',
              '-lpthread',
            ],
          },
          'variables': {
            'exported_deps': [
              'dbus-glib-1',
              'libbrillo-<(libbase_ver)',
              'libbrillo-glib-<(libbase_ver)',
              'libchrome-<(libbase_ver)',
              'openssl',
              'protobuf',
            ],
            'deps': ['<@(exported_deps)'],
          },
          'direct_dependent_settings': {
            'variables': {
              'deps': ['<@(exported_deps)'],
            },
          },
          'sources': [
            'cert_provision.cc',
            'cert_provision_cryptohome.cc',
            'cert_provision_keystore.cc',
            'cert_provision_pca.cc',
            'cert_provision_util.cc',
          ],
        },
        {
          'target_name': 'cert_provision_client',
          'type': 'executable',
          'dependencies': [
            'cert_provision',
          ],
          'sources': [
            'cert_provision_client.cc',
          ],
          'variables': {
            'deps': [
              'libbrillo-<(libbase_ver)',
              'libchrome-<(libbase_ver)',
            ],
          },
        },
      ],
    }],
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'cryptohome_testrunner',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'dependencies': [
            'libcrostpm',
            'libcryptohome',
          ],
          'link_settings': {
            'libraries': [
              '-lchaps',
              '-lkeyutils',
              '-lpolicy-<(libbase_ver)',
              '-lpthread',
              '-lscrypt',
              '-lvboot_host',
            ],
          },
          'variables': {
            'deps': [
              'dbus-1',
              'dbus-glib-1',
              'glib-2.0',
              'libbrillo-<(libbase_ver)',
              'libbrillo-glib-<(libbase_ver)',
              'libbrillo-test-<(libbase_ver)',
              'libchrome-<(libbase_ver)',
              'libchrome-test-<(libbase_ver)',
              'libecryptfs',
              'libmetrics-<(libbase_ver)',
              'openssl',
              'protobuf',
            ],
          },
          'sources': [
            'attestation_unittest.cc',
            'boot_attributes_unittest.cc',
            'boot_lockbox_unittest.cc',
            'crypto_unittest.cc',
            'cryptohome_event_source_unittest.cc',
            'cryptohome_testrunner.cc',
            'dircrypto_data_migrator/migration_helper_unittest.cc',
            'firmware_management_parameters_unittest.cc',
            'homedirs_unittest.cc',
            'install_attributes_unittest.cc',
            'lockbox_unittest.cc',
            'make_tests.cc',
            'mock_chaps_client_factory.cc',
            'mock_firmware_management_parameters.cc',
            'mock_homedirs.cc',
            'mock_install_attributes.cc',
            'mock_keystore.cc',
            'mock_lockbox.cc',
            'mock_mount.cc',
            'mock_pkcs11_init.cc',
            'mock_platform.cc',
            'mock_service.cc',
            'mock_tpm.cc',
            'mock_tpm_init.cc',
            'mock_user_oldest_activity_timestamp_cache.cc',
            'mock_user_session.cc',
            'mock_vault_keyset.cc',
            'mount_stack_unittest.cc',
            'mount_task_unittest.cc',
            'mount_unittest.cc',
            'pkcs11_keystore_unittest.cc',
            'platform_unittest.cc',
            'service_unittest.cc',
            'stateful_recovery_unittest.cc',
            'tpm_init_unittest.cc',
            'user_oldest_activity_timestamp_cache_unittest.cc',
            'user_session_unittest.cc',
            'username_passkey_unittest.cc',
            'vault_keyset_unittest.cc',
          ],
          'conditions': [
            ['USE_tpm2 == 1', {
              'libraries': [
                '-ltrunks_test',
                '-ltpm_manager_test',
              ],
              'sources': [
                'tpm2_test.cc',
              ],
            }],
            ['USE_cert_provision == 1', {
              'dependencies': [
                'cert_provision-static',
              ],
              'sources': [
                'cert_provision_keystore_unittest.cc',
                'cert_provision_unittest.cc',
              ],
            }],
          ],
        },
      ],
    }],
  ],
}
