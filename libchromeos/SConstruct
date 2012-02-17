# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import SCons.Util

ROOT = os.environ.get('ROOT', '/')
PKG_CONFIG = os.environ.get('PKG_CONFIG', 'pkg-config')
BASE_VER = os.environ['BASE_VER']
libchrome = 'chrome-%s' % BASE_VER
libchromeos = 'chromeos-%s' % BASE_VER

# Set up an env object that every target below can be based on.
def common_env():
  env = Environment(
      CPPPATH = [ '.', './chromeos/policy', ],
      CCFLAGS = [ '-g', ],
  )

  for key in Split('CC CXX AR RANLIB LD NM CFLAGS CXXFLAGS CCFLAGS LIBPATH'):
    value = os.environ.get(key)
    if value != None:
      env[key] = Split(value)
  env['CCFLAGS'] += ['-fPIC', '-fno-exceptions']

  if os.environ.has_key('CPPFLAGS'):
    env['CCFLAGS'] += SCons.Util.CLVar(os.environ['CPPFLAGS'])
  if os.environ.has_key('LDFLAGS'):
    env['LINKFLAGS'] += SCons.Util.CLVar(os.environ['LDFLAGS'])

  # Fix issue with scons not passing some vars through the environment.
  for key in Split('PKG_CONFIG_LIBDIR PKG_CONFIG_PATH SYSROOT'):
    if os.environ.has_key(key):
      env['ENV'][key] = os.environ[key]

  return env


common_pc_libs = 'lib%s' % libchrome
base_name = 'chromeos'
base_libs = [
  {
    'name' : 'core',
    'sources' : """
                chromeos/dbus/abstract_dbus_service.cc
                chromeos/dbus/dbus.cc
                chromeos/dbus/error_constants.cc
                chromeos/process.cc
                chromeos/secure_blob.cc
                chromeos/syslog_logging.cc
                chromeos/utility.cc
                """,
    'libs' : '',
    'pc_libs' : 'dbus-1 glib-2.0 dbus-glib-1 dbus-c++-1 ' + common_pc_libs,
  },
  {
    'name' : 'cryptohome',
    'sources' : """
                chromeos/cryptohome.cc
                """,
    'libs' : '',
    'pc_libs' : 'openssl ' + common_pc_libs,
  },
]

env = common_env()
env.Append(
    LIBS = ['event'],
    CPPPATH = ['../third_party/chrome/files'],
  )
env_test = env.Clone()

all_base_libs = []
all_pc_libs = ''
all_libs = []
all_scons_libs = []

# Build all the shared libraries.
for lib in base_libs:
  pc_libs = lib['pc_libs'].replace('${bslot}', BASE_VER)
  all_pc_libs += ' ' + pc_libs

  libs = Split(lib['libs'].replace('${bslot}', BASE_VER))
  all_libs += libs

  name = '%s-%s-%s' % (base_name, lib['name'], BASE_VER)
  all_base_libs += [name]
  corename = '%s-core-%s' % (base_name, BASE_VER)
  # Automatically link the sub-libs against the main core lib.
  # This is to keep from having to explicitly mention it in the
  # table above (i.e. lazy).
  if name != corename:
    libs += [corename]

  e = env.Clone()
  e.Append(
    LIBS = Split(libs),
    LIBPATH = ['.'],
    LINKFLAGS = ['-Wl,--as-needed', '-Wl,-z,defs',
                 '-Wl,-soname,lib%s.so' % name],
  )
  if pc_libs:
    e.ParseConfig(PKG_CONFIG + ' --cflags --libs %s' % pc_libs)
  all_scons_libs += [ e.SharedLibrary(name, Split(lib['sources'])) ]


# Build the random text files (pkg-config and linker script).

def lib_list(libs):
  return ' '.join(['-l' + l for l in libs])

subst_dict = {
  '@BSLOT@' : BASE_VER,
  '@PRIVATE_PC@' : all_pc_libs,
  '@BASE_LIBS@' : lib_list(all_base_libs),
  '@LIBS@' : lib_list(all_libs),
}
env = Environment(tools = ['textfile'], SUBST_DICT = subst_dict)

env.Substfile(source = 'libchromeos.pc.in',
              target = 'lib%s.pc' % libchromeos)
env.Depends('lib%s.so' % libchromeos, all_scons_libs)
env.Substfile('lib%s.so' % libchromeos,
              [Value('GROUP ( AS_NEEDED ( @BASE_LIBS@ ) )')])

# Unit test
if ARGUMENTS.get('debug', 0):
  env_test.Append(
      CCFLAGS = ['-fprofile-arcs', '-ftest-coverage', '-fno-inline'],
      LIBS = ['gcov'],
    )

env_test.Append(
    LIBS = [libchromeos, 'gtest', 'rt'],
    LIBPATH = ['.', '../third_party/chrome'],
    LINKFLAGS = ['-Wl,-rpath,\'$$ORIGIN\':.'],
  )
env_test.ParseConfig(PKG_CONFIG + ' --cflags --libs dbus-1 glib-2.0 ' +
                     'dbus-glib-1 dbus-c++-1 openssl lib%s' % libchrome)

unittest_sources =['chromeos/glib/object_unittest.cc',
                   'chromeos/process_test.cc',
                   'chromeos/secure_blob_unittest.cc',
                   'chromeos/utility_test.cc']
unittest_main = ['testrunner.cc']
unittest_cmd = env_test.Program('unittests',
                           unittest_sources + unittest_main)

Clean(unittest_cmd, Glob('*.gcda') + Glob('*.gcno') + Glob('*.gcov') +
                    Split('html app.info'))

# --------------------------------------------------
# Prepare and build the policy serving library.
libpolicy = 'policy-%s' % BASE_VER
PROTO_PATH = '%susr/include/proto' % ROOT
PROTO_FILES = ['%s/chrome_device_policy.proto' % PROTO_PATH,
               '%s/device_management_backend.proto' % PROTO_PATH]
PROTO_SOURCES=['chromeos/policy/bindings/chrome_device_policy.pb.cc',
               'chromeos/policy/bindings/device_management_backend.pb.cc'];
PROTO_HEADERS = [x.replace('.cc', '.h') for x in PROTO_SOURCES]

POLICY_SOURCES=PROTO_SOURCES + \
    ['chromeos/policy/device_policy.cc',
     'chromeos/policy/device_policy_impl.cc',
     'chromeos/policy/libpolicy.cc'];

env = common_env()
env.Append(
    LIBS = [libchromeos, 'protobuf-lite', 'pthread', 'rt'],
    LIBPATH = ['.', '../third_party/chrome'],
    LINKFLAGS = ['-Wl,--as-needed', '-Wl,-z,defs',
                 '-Wl,-soname,lib%s.so' % libpolicy,
                 '-Wl,--version-script,libpolicy.ver'],
  )

# Build the protobuf definitions.
env.Command(PROTO_SOURCES + PROTO_HEADERS,
            None,
            ('mkdir -p chromeos/policy/bindings && ' +
             '/usr/bin/protoc --proto_path=%s ' +
             '--cpp_out=chromeos/policy/bindings %s') % (
             PROTO_PATH, ' '.join(PROTO_FILES)));

env.SharedLibrary(libpolicy, POLICY_SOURCES)
env.ParseConfig(PKG_CONFIG + ' --cflags --libs glib-2.0 openssl ' +
                'lib%s' % libchrome)

# Prepare the test case as well
env_test = common_env()
env_test.Append(
    LIBS = [libpolicy, 'gtest', 'rt', 'pthread'],
    LIBPATH = ['.'],
    LINKFLAGS = ['-Wl,-rpath,\'$$ORIGIN\':.'],
  )
env_test.ParseConfig(PKG_CONFIG + ' --cflags --libs lib%s' % libchrome)

# Use libpolicy instead of passing in LIBS in order to always
# get the version we just built, not what was previously installed.
unittest_sources=['chromeos/policy/tests/libpolicy_unittest.cc']
env_test.ParseConfig(PKG_CONFIG + ' --cflags --libs glib-2.0 openssl')
env_test.Program('libpolicy_unittest', unittest_sources)
