# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

Help("""\
Type: 'scons' to build and 'scons -c' to clean\
""")

# Create a base environment including things that are likely to be common
# to all of the objects in this directory. We pull in overrides from the
# environment to enable cross-compile.
base_env = Environment()
for key in Split('CC CXX AR RANLIB LD NM PKG_CONFIG'):
  value = os.environ.get(key)
  if value is not None:
    base_env[key] = value
for key in Split('CFLAGS CPPFLAGS CXXFLAGS CCFLAGS CPPPATH LIBPATH'):
  value = os.environ.get(key, '')
  base_env[key] = Split(value)

extra_flags = '-fno-strict-aliasing -Wall -Wextra -Werror -Wuninitialized'
extra_cppflags = '-D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS'
base_env['CFLAGS'].extend(Split(extra_flags))
base_env['CXXFLAGS'].extend(Split(extra_flags))
base_env['CPPFLAGS'].extend(Split(extra_cppflags))

# Fix issue with scons not passing some vars through the environment.
for key in Split('SYSROOT'):
  if key in os.environ:
    base_env['ENV'][key] = os.environ[key]
base_env.Append(CPPPATH=['..'])

shill_env = base_env.Clone()
shill_env.ParseConfig(
  os.environ['PKG_CONFIG'] + ' --cflags --libs dbus-1 dbus-glib-1'
)
shill_env.Append(LIBS=['base', 'glog'])

shill_sources = Split('''\
  shill_logging.cc shill_daemon.cc shill_config.cc shill_event.cc
  resource.cc manager.cc service.cc device.cc
  dbus_control.cc
''')
shill_lib = shill_env.Library('shill_lib', shill_sources)
shill = shill_env.Program('shill', ['shill_main.cc', shill_lib])
Default(shill)

# Build unit tests
tests = []
testrunner = base_env.Library('testrunner', ['testrunner.cc'])
shill_unittest_env = shill_env.Clone()
shill_unittest_env.Append(LIBS=['gtest', 'gmock'])
deps = [testrunner, 'shill_unittest.cc']
deps.append(shill_lib)
tests.append(shill_unittest_env.Program('shill_unittest', deps))
AlwaysBuild(base_env.Alias('tests', tests))
