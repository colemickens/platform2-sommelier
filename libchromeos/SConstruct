# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

SOURCES=['chromeos/cryptohome.cc',
         'chromeos/dbus/abstract_dbus_service.cc',
         'chromeos/dbus/dbus.cc',
         'chromeos/dbus/error_constants.cc',
         'chromeos/process.cc',
         'chromeos/string.cc',
         'chromeos/syslog_logging.cc',
         'chromeos/utility.cc']

env = Environment(
    CPPPATH=[ '.', '../third_party/chrome/files' ],
    CCFLAGS=[ '-g' ],
)
for key in Split('CC CXX AR RANLIB LD NM CFLAGS CCFLAGS'):
  value = os.environ.get(key)
  if value != None:
    env[key] = Split(value)
env['CCFLAGS'] += ['-fPIC', '-fno-exceptions']

# Fix issue with scons not passing some vars through the environment.
for key in Split('PKG_CONFIG_LIBDIR PKG_CONFIG_PATH SYSROOT'):
  if os.environ.has_key(key):
    env['ENV'][key] = os.environ[key]

# glib and dbug environment
env.ParseConfig(
    os.environ['PKG_CONFIG'] + ' --cflags --libs dbus-1 glib-2.0 dbus-glib-1')
env.StaticLibrary('chromeos', SOURCES)

# Unit test
if ARGUMENTS.get('debug', 0):
  env.Append(
    CCFLAGS = ['-fprofile-arcs', '-ftest-coverage', '-fno-inline'],
    LIBS = ['gcov'],
  )

env_test = env.Clone()

env_test.Append(
    LIBS = ['gtest', 'base', 'rt'],
    LIBPATH = ['.', '../third_party/chrome'],
  )
for key in Split('CC CXX AR RANLIB LD NM CFLAGS CCFLAGS'):
  value = os.environ.get(key)
  if value:
    env_test[key] = Split(value)

# Use libchromeos instead of passing in LIBS in order to always
# get the version we just built, not what was previously installed.
unittest_sources =['chromeos/glib/object_unittest.cc',
                   'chromeos/process_test.cc',
                   'libchromeos.a']
unittest_main = ['testrunner.cc']
unittest_cmd = env_test.Program('unittests',
                           unittest_sources + unittest_main)

Clean(unittest_cmd, Glob('*.gcda') + Glob('*.gcno') + Glob('*.gcov') +
                    Split('html app.info'))

# --------------------------------------------------
# Prepare and build the policy serving library.
PROTO_PATH = '%susr/include/proto' % os.environ["ROOT"]
PROTO_FILES = ['%s/chrome_device_policy.proto' % PROTO_PATH,
               '%s/device_management_backend.proto' % PROTO_PATH]
PROTO_SOURCES=['chromeos/policy/bindings/chrome_device_policy.pb.cc',
               'chromeos/policy/bindings/device_management_backend.pb.cc'];

POLICY_SOURCES=PROTO_SOURCES + \
    ['chromeos/policy/device_policy.cc',
     'chromeos/policy/device_policy_impl.cc',
     'chromeos/policy/libpolicy.cc'];

# Reset the environment.
env = Environment(
    CPPPATH=[ '.' ],
    CCFLAGS=[ '-g' ],
    LIBS = ['base', 'protobuf-lite'],
    LIBPATH = ['.', '../third_party/chrome'],
)
for key in Split('CC CXX AR RANLIB LD NM CFLAGS CCFLAGS CPPPATH LIBPATH'):
  value = os.environ.get(key)
  if value != None:
    env[key] = Split(value)
env['CCFLAGS'] += ['-fPIC', '-fno-exceptions']

# Build the protobuf definitions.
env.Command(PROTO_SOURCES,
            None,
            ('mkdir -p chromeos/policy/bindings && ' +
             '/usr/bin/protoc --proto_path=%s ' +
             '--cpp_out=chromeos/policy/bindings %s') % (
             PROTO_PATH, ' '.join(PROTO_FILES)));

# Fix issue with scons not passing some vars through the environment.
for key in Split('PKG_CONFIG_LIBDIR PKG_CONFIG_PATH SYSROOT'):
  if os.environ.has_key(key):
    env['ENV'][key] = os.environ[key]

env.StaticLibrary('policy', POLICY_SOURCES)
env.ParseConfig(os.environ['PKG_CONFIG'] + ' --cflags --libs glib-2.0 openssl')
env.SharedLibrary('policy', POLICY_SOURCES)

# Prepare the test case as well
env_test = env.Clone()

env_test.Append(
    LIBS = ['gtest', 'base', 'rt', 'pthread'],
    LIBPATH = ['.'],
  )
for key in Split('CC CXX AR RANLIB LD NM CFLAGS CCFLAGS'):
  value = os.environ.get(key)
  if value:
    env_test[key] = Split(value)


# Use libpolicy instead of passing in LIBS in order to always
# get the version we just built, not what was previously installed.
unittest_sources=['chromeos/policy/tests/libpolicy_unittest.cc',
                  'libpolicy.a']
env_test.ParseConfig(
    os.environ['PKG_CONFIG'] + ' --cflags --libs glib-2.0 openssl')
env_test.Program('libpolicy_unittest', unittest_sources)
