# This include file defines library targets and other auxillary definitions that
# are used for the resulting executable targets.
{
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
        '<(proto_in_dir)/fake_le_credential_metadata.proto',
        '<(proto_in_dir)/hash_tree_leaf_data.proto',
        '<(proto_in_dir)/install_attributes.proto',
        '<(proto_in_dir)/signature_sealed_data.proto',
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
        'dbus_glib_header_stem': 'cryptohome',
      },
      'sources': [
        'dbus_bindings/org.chromium.CryptohomeInterface.xml',
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
        'dbus_glib_header_stem': 'cryptohome',
      },
      'sources': [
        'dbus_bindings/org.chromium.CryptohomeInterface.xml',
      ],
      'includes': ['../common-mk/dbus_glib.gypi'],
    },

    # Common objects.
    {
      'target_name': 'libcrosplatform',
      'type': 'static_library',
      'link_settings': {
        'libraries': [
          '-lkeyutils',
          '-lsecure_erase_file',
        ],
      },
      'variables': {
        'deps': [
          'libbrillo-<(libbase_ver)',
          'libchrome-<(libbase_ver)',
          'libecryptfs',
          'libmetrics-<(libbase_ver)',
        ],
      },
      'sources': [
        'cryptohome_metrics.cc',
        'cryptolib.cc',
        'dircrypto_util.cc',
        'platform.cc',
      ],
    },
    {
      'target_name': 'libcrostpm',
      'type': 'static_library',
      'dependencies': [
        'cryptohome-proto',
        'libcrosplatform',
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
        'bootlockbox/boot_lockbox.cc',
        'crc32.c',
        'crc8.c',
        'crypto.cc',
        'firmware_management_parameters.cc',
        'install_attributes.cc',
        'lockbox.cc',
        'pkcs11_init.cc',
        'pkcs11_keystore.cc',
        'tpm.cc',
        'tpm_init.cc',
        'tpm_persistent_state.cc',
      ],
      'conditions': [
        ['USE_tpm2 == 1', {
          'sources': [
            'signature_sealing_backend_tpm2_impl.cc',
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
        'bootlockbox/boot_attributes.cc',
        'chaps_client_factory.cc',
        'crypto.cc',
        'cryptohome_event_source.cc',
        'dbus_transition.cc',
        'dircrypto_data_migrator/atomic_flag.cc',
        'dircrypto_data_migrator/migration_helper.cc',
        'homedirs.cc',
        'interface.cc',
        'le_credential_manager.cc',
        'lockbox-cache-tpm.cc',
        'lockbox-cache.cc',
        'migration_type.h',
        'mount.cc',
        'mount_factory.cc',
        'mount_stack.cc',
        'mount_task.cc',
        'persistent_lookup_table.cc',
        'service.cc',
        'service_monolithic.cc',
        'sign_in_hash_tree.cc',
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
    {
      'target_name': 'bootlockbox-adaptors',
      'type': 'none',
      'variables': {
        'dbus_adaptors_out_dir': 'include/dbus_adaptors',
        'dbus_service_config': 'dbus_adaptors/dbus-service-config.json',
      },
      'sources': [
        'dbus_adaptors/org.chromium.BootLockboxInterface.xml',
      ],
      'includes': ['../common-mk/generate-dbus-adaptors.gypi'],
    },
  ],
}
