# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

# Borrowed from updater
# Protobuffer compilation
def ProtocolBufferEmitter(target, source, env):
  """ Inputs:
          target: list of targets to compile to
          source: list of sources to compile
          env: the scons environment in which we are compiling
      Outputs:
          target: the list of targets we'll emit
          source: the list of sources we'll compile"""
  output = str(source[0])
  output = output[0:output.rfind('.proto')]
  target = [
    output + '.pb.cc',
    output + '.pb.h',
  ]
  return target, source

def ProtocolBufferGenerator(source, target, env, for_signature):
  """ Inputs:
          source: list of sources to process
          target: list of targets to generate
          env: scons environment in which we are working
          for_signature: unused
      Outputs: a list of commands to execute to generate the targets from
               the sources."""
  commands = [
    '/usr/bin/protoc '
    ' --proto_path . ${SOURCES} --cpp_out .']
  return commands

proto_builder = Builder(generator = ProtocolBufferGenerator,
                        emitter = ProtocolBufferEmitter,
                        single_source = 1,
                        suffix = '.pb.cc')

env = Environment()

# define generic dbus server/client bindings builders
dbus_server_builder = Builder(action = 'dbus-binding-tool --mode=glib-server --prefix=`basename $SOURCE .xml` $SOURCE > $TARGET')
dbus_client_builder = Builder(action = 'dbus-binding-tool --mode=glib-client --prefix=`basename $SOURCE .xml` $SOURCE > $TARGET')
def GlibMarshalEmitter(target, source, env):
  """ Inputs:
          target: unused
          source: list containing the source .list file
          env: the scons environment in which we are compiling
      Outputs:
          target: the list of targets we'll emit
          source: the list of sources we'll process"""
  output = str(source[0])
  output = output[0:output.rfind('.list')]
  target = [
    output + '.glibmarshal.c',
    output + '.glibmarshal.h'
  ]
  return target, source

def GlibMarshalGenerator(source, target, env, for_signature):
  """ Inputs:
          source: list of sources to process
          target: list of targets to generate
          env: scons environment in which we are working
          for_signature: unused
      Outputs: a list of commands to execute to generate the targets from
               the sources."""
  commands = []
  for target_file in target:
    if str(target_file).endswith('.glibmarshal.h'):
      mode_flag = '--header '
    else:
      mode_flag = '--body '
    cmd = '/usr/bin/glib-genmarshal %s --prefix=cryptohome ' \
          '%s > %s' % (mode_flag, str(source[0]), str(target_file))
    commands.append(cmd)
  return commands

glib_marshal_builder = Builder(generator = GlibMarshalGenerator,
                               emitter = GlibMarshalEmitter,
                               single_source = 1,
                               suffix = 'glibmarshal.c')
env.Append(BUILDERS = { 'DbusServerBindings' : dbus_server_builder,
                        'DbusClientBindings' : dbus_client_builder,
                        'GlibMarshal' : glib_marshal_builder})
# setup sources
commonlib_sources = env.Split("""crypto.cc
                                 cryptohome_event_source.cc
                                 install_attributes.pb.cc
                                 install_attributes.cc
                                 interface.cc
                                 lockbox.cc
                                 marshal.glibmarshal.c
                                 mount.cc
                                 mount_task.cc
                                 pkcs11_init.cc
                                 platform.cc
                                 secure_blob.cc
                                 service.cc
                                 tpm.cc
                                 tpm_init.cc
                                 tpm_status.pb.cc
                                 user_session.cc
                                 username_passkey.cc
                                 user_oldest_activity_timestamp_cache.cc
                                 vault_keyset.cc
                                 vault_keyset.pb.cc
                              """)
server_sources = env.Split("""cryptohomed.cc""")
client_sources = env.Split("""cryptohome.cc""")
path_sources = env.Split("""cryptohome-path.cc""")
test_sources = env.Split("""cryptohome_event_source_unittest.cc
                            crypto_unittest.cc
                            service_unittest.cc
                            install_attributes_unittest.cc
                            lockbox_unittest.cc
                            mount_task_unittest.cc
                            mount_unittest.cc
                            secure_blob_unittest.cc
                            username_passkey_unittest.cc
                            user_oldest_activity_timestamp_cache_unittest.cc
                            vault_keyset_unittest.cc
                            user_session_unittest.cc
                            sha_test_vectors.cc
                            make_tests.cc
                            cryptohome_testrunner.cc
                         """)



env.Append(
    CPPPATH=['.', '..'],
    CPPFLAGS=['-pie', '-fstack-protector-all', '-fPIC',
              '-fno-exceptions', '-O2', '-Wall', '-Werror'],
    # WARNING! Dear person who thinks these library dependencies should be in
    # the alphabetical order. No, you are wrong. Do not attempt to change the
    # order without copious testing. In particular, the 'chromeos' dependency
    # should appear before 'base' or bad things happen.
    LIBS = ['rt', 'crypto', 'ecryptfs', 'event', 'keyutils',
            'chaps', 'protobuf', 'pthread', 'scrypt', 'tspi', 'metrics'],
)
for key in Split('CC CXX AR RANLIB LD NM CFLAGS CCFLAGS'):
  value = os.environ.get(key)
  if value != None:
    env[key] = value

# Fix issue with scons not passing some vars through the environment.
for key in Split('PKG_CONFIG_LIBDIR PKG_CONFIG_PATH SYSROOT'):
  if os.environ.has_key(key):
    env['ENV'][key] = os.environ[key]

pkgconfig = os.environ.get('PKG_CONFIG', 'pkg-config')

env['BUILDERS']['ProtocolBuffer'] = proto_builder

env.Append(LIBPATH = ['.'])
env.ParseConfig(pkgconfig + ' --cflags --libs ' \
                'dbus-1 dbus-glib-1 glib-2.0 gdk-2.0 gtk+-2.0 libchrome' +
                ' libchromeos nss')
env.ProtocolBuffer('vault_keyset.pb.cc', 'vault_keyset.proto')
env.ProtocolBuffer('tpm_status.pb.cc', 'tpm_status.proto')
env.ProtocolBuffer('install_attributes.pb.cc', 'install_attributes.proto')
env.GlibMarshal('marshal.glibmarshal.c', 'marshal.list')

env_commonlib = env.Clone()
env_commonlib.SharedLibrary('cryptohome', commonlib_sources)

env_server = env.Clone()
env_server.Append(LIBS=['cryptohome'])
env_server.DbusServerBindings('bindings/server.h', 'cryptohome.xml')
env_server.Program('cryptohomed', server_sources)

env_client = env.Clone()
env_client.Append(LIBS=['cryptohome'])
env_client.DbusClientBindings('bindings/client.h', 'cryptohome.xml')
env_client.Program('cryptohome', client_sources)

env_path = env.Clone()
env_path.Program('cryptohome-path', path_sources)

# TODO(wad) we'll probably want a separate runner for client roundtrip tests
env_tests = env.Clone()
env_tests.Append(LIBS=['gtest', 'gmock', 'policy'])
# If the lib is linked, it will use the system search path.
env_tests.Program('cryptohome_testrunner', test_sources + commonlib_sources)
