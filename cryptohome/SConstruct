# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

env = Environment()

# define generic dbus server/client bindings builders
dbus_server_builder = Builder(action = 'dbus-binding-tool --mode=glib-server --prefix=`basename $SOURCE .xml` $SOURCE > $TARGET')
dbus_client_builder = Builder(action = 'dbus-binding-tool --mode=glib-client --prefix=`basename $SOURCE .xml` $SOURCE > $TARGET')
env.Append(BUILDERS = { 'DbusServerBindings' : dbus_server_builder,
                        'DbusClientBindings' : dbus_client_builder})
# setup sources
commonlib_sources = env.Split("""crypto.cc
                                 interface.cc
                                 mount.cc
                                 platform.cc
                                 secure_blob.cc
                                 service.cc
                                 username_passkey.cc
                                 vault_keyset.cc
                              """)
server_sources = env.Split("""cryptohomed.cc""")
client_sources = env.Split("""cryptohome.cc""")
test_sources = env.Split("""crypto_unittest.cc
                            service_unittest.cc
                            mount_unittest.cc
                            secure_blob_unittest.cc
                            username_passkey_unittest.cc
                            vault_keyset_unittest.cc
                            cryptohome_testrunner.cc
                         """)



env.Append(
    CPPPATH=['.', '..'],
    CPPFLAGS=['-pie', '-fstack-protector-all',
              '-fno-exceptions', '-O2', '-Wall', '-Werror'],
    LIBS = ['base', 'chromeos', 'rt', 'crypto', 'ecryptfs', 'keyutils'],
)
for key in Split('CC CXX AR RANLIB LD NM CFLAGS CCFLAGS'):
  value = os.environ.get(key)
  if value != None:
    env[key] = value

# Fix issue with scons not passing some vars through the environment.
for key in Split('PKG_CONFIG_LIBDIR PKG_CONFIG_PATH SYSROOT'):
  if os.environ.has_key(key):
    env['ENV'][key] = os.environ[key]

env.Append(LIBPATH = ['.'])
env.ParseConfig('pkg-config --cflags --libs gobject-2.0 dbus-1 dbus-glib-1')

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

# TODO(wad) we'll probably want a separate runner for client roundtrip tests
env_tests = env.Clone()
env_tests.Append(LIBS=['cryptohome', 'gtest', 'gmock'])
env_tests.Program('cryptohome_testrunner', test_sources)
