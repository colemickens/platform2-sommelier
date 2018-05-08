# Executable targets that belong to the main cryptohome package.
{
  'includes': ['cryptohome-libs.gypi'],
  'variables': {
    'USE_cert_provision%': 0,
  },
  'targets': [
    # An executable that communicates with bootlockbox daemon.
    {
      'target_name': 'bootlockboxtool',
      'type': 'executable',
      'dependencies': [
        'cryptohome-proto',
      ],
      'variables': {
        'deps': [
          'libbootlockbox-client',
          'libchrome-<(libbase_ver)',
          'libbrillo-<(libbase_ver)',
          'protobuf',
        ],
      },
      'sources': [
        'bootlockbox/boot_lockbox_client.cc',
        'bootlockbox/boot_lockbox_tool.cc',
      ],
    },
    {
      'target_name': 'bootlockboxd',
      'type': 'executable',
      'dependencies': [
        'libcrostpm',
        'bootlockbox-adaptors',
        'cryptohome-proto',
      ],
      'link_settings': {
        'libraries': [
          '-lscrypt',
          '-lchaps',
          '-lkeyutils',
        ],
      },
      'variables': {
        'deps': [
          'libchrome-<(libbase_ver)',
          'libbrillo-<(libbase_ver)',
          'protobuf',
          'openssl',
          'libmetrics-<(libbase_ver)',
          'libecryptfs',
          'vboot_host',
        ],
      },
      'sources': [
        'bootlockbox/boot_lockbox_dbus_adaptor.cc',
        'bootlockbox/boot_lockbox_service.cc',
        'bootlockbox/boot_lockboxd.cc',
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
          'vboot_host',
        ],
      },
      'sources': [
        'cryptohome.cc',
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
          'vboot_host',
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
        ],
      },
      'variables': {
        'deps': [
          'libbrillo-<(libbase_ver)',
          'libchrome-<(libbase_ver)',
          'libecryptfs',
          'libmetrics-<(libbase_ver)',
          'openssl',
          'vboot_host',
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
      'target_name': 'mount_encrypted_lib',
      'type': 'static_library',
      'sources': [
        'mount_encrypted/encryption_key.cc',
        'mount_encrypted/tpm.cc',
        'mount_helpers.cc',
      ],
      'variables': {
        'deps': [
          'glib-2.0',
          'libbrillo-<(libbase_ver)',
          'libchrome-<(libbase_ver)',
          'openssl',
          'vboot_host',
        ],
      },
      'defines': [
        'CHROMEOS_ENVIRONMENT=1',
      ],
      'conditions': [
        ['USE_tpm2 == 1', {
          'defines': [
            # This selects TPM2 code in vboot_host headers.
            'TPM2_MODE=1',
          ],
          'sources': [
            'mount_encrypted/tpm2.cc',
          ],
        }],
        ['USE_tpm2 == 0', {
          'sources': [
            'mount_encrypted/tpm1.cc',
          ],
        }],
      ],
    },
    {
      'target_name': 'mount-encrypted',
      'type': 'executable',
      'dependencies': [
        'libcrostpm',
        'mount_encrypted_lib',
      ],
      'variables': {
        'deps': [
          'glib-2.0',
          'libbrillo-<(libbase_ver)',
          'libchrome-<(libbase_ver)',
          'openssl',
          'vboot_host',
        ],
      },
      'sources': [
        'mount_encrypted.cc',
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
            ],
          },
          'variables': {
            'deps': [
              'vboot_host',
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
            'proto_in_dir': './cert',
            'proto_out_dir': 'include/cert',
            'exported_deps': [
              'protobuf',
            ],
            'deps': ['<@(exported_deps)'],
          },
          'all_dependent_settings': {
            'variables': {
              'deps': ['<@(exported_deps)'],
            },
          },
          'sources': [
            'cert/cert_provision.proto',
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
            'cert/cert_provision.cc',
            'cert/cert_provision_cryptohome.cc',
            'cert/cert_provision_keystore.cc',
            'cert/cert_provision_pca.cc',
            'cert/cert_provision_util.cc',
          ],
        },
        {
          'target_name': 'cert_provision_client',
          'type': 'executable',
          'dependencies': [
            'cert_provision',
          ],
          'sources': [
            'cert/cert_provision_client.cc',
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
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'link_settings': {
            'libraries': [
              '-lchaps',
              '-lkeyutils',
              '-lpolicy-<(libbase_ver)',
              '-lpthread',
              '-lscrypt',
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
              'vboot_host',
            ],
          },
          'sources': [
            'arc_disk_quota_unittest.cc',
            'attestation_unittest.cc',
            'boot_attributes_unittest.cc',
            'bootlockbox/boot_lockbox_dbus_adaptor.cc',
            'bootlockbox/boot_lockbox_service.cc',
            'bootlockbox/boot_lockbox_service_unittest.cc',
            'bootlockbox/boot_lockbox_unittest.cc',
            'challenge_credentials/challenge_credentials_test_utils.cc',
            'crypto_unittest.cc',
            'cryptohome_event_source_unittest.cc',
            'cryptolib_unittest.cc',
            'dircrypto_data_migrator/migration_helper_unittest.cc',
            'fake_le_credential_backend.cc',
            'firmware_management_parameters_unittest.cc',
            'homedirs_unittest.cc',
            'install_attributes_unittest.cc',
            'le_credential_manager_unittest.cc',
            'lockbox_unittest.cc',
            'make_tests.cc',
            'mock_chaps_client_factory.cc',
            'mock_firmware_management_parameters.cc',
            'mock_homedirs.cc',
            'mock_install_attributes.cc',
            'mock_key_challenge_service.cc',
            'mock_keystore.cc',
            'mock_lockbox.cc',
            'mock_mount.cc',
            'mock_pkcs11_init.cc',
            'mock_platform.cc',
            'mock_service.cc',
            'mock_signature_sealing_backend.cc',
            'mock_tpm.cc',
            'mock_tpm_init.cc',
            'mock_user_oldest_activity_timestamp_cache.cc',
            'mock_user_session.cc',
            'mock_vault_keyset.cc',
            'mount_stack_unittest.cc',
            'mount_task_unittest.cc',
            'mount_unittest.cc',
            'obfuscated_username_unittest.cc',
            'persistent_lookup_table_unittest.cc',
            'pkcs11_keystore_unittest.cc',
            'platform_unittest.cc',
            'service_unittest.cc',
            'sign_in_hash_tree_unittest.cc',
            'signature_sealing_backend_test_utils.cc',
            'stateful_recovery_unittest.cc',
            'tpm_init_unittest.cc',
            'tpm_persistent_state_unittest.cc',
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
                'bootlockbox/tpm2_nvspace_utility_unittest.cc',
                'tpm2_test.cc',
              ],
            }],
            ['USE_cert_provision == 1', {
              'dependencies': [
                'cert_provision-static',
              ],
              'sources': [
                'cert/cert_provision_keystore_unittest.cc',
                'cert/cert_provision_unittest.cc',
              ],
            }],
          ],
        },
        {
          'target_name': 'mount_encrypted_unittests',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'dependencies': [
            'libcrostpm',
            'mount_encrypted_lib',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'link_settings': {
            'libraries': [
            ],
          },
          'variables': {
            'deps': [
              'glib-2.0',
              'libbrillo-<(libbase_ver)',
              'libbrillo-test-<(libbase_ver)',
              'libchrome-<(libbase_ver)',
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'sources': [
            'mount_encrypted/encryption_key_unittest.cc',
            'mount_encrypted/tlcl_stub.cc',
          ],
          'conditions': [
            ['USE_tpm2 == 1', {
              'defines': [
                'TPM2_MODE=1',
              ],
            }],
          ],
        },
      ],
    }],
    ['USE_fuzzer == 1', {
      'targets': [
        {
          'target_name': 'cryptohome_cryptolib_rsa_oaep_decrypt_fuzzer',
          'type': 'executable',
          'dependencies': [
            'libcrosplatform',
          ],
          'variables': {
            'deps': [
              'libbrillo-<(libbase_ver)',
              'libchrome-<(libbase_ver)',
              'openssl',
            ],
          },
          'includes': ['../common-mk/common_fuzzer.gypi'],
          'sources': [
            'fuzzers/cryptolib_rsa_oaep_decrypt_fuzzer.cc',
          ],
        },
      ],
    }],
  ],
}
