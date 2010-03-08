# Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import itertools


Help('''\
Type: 'scons' to build and 'scons -c' to clean\
''')

# Create a base environment including things that are likely to be common
# to all of the objects in this directory. We pull in overrides from the
# environment to enable cross-compile.
base_env = Environment()
for key in Split('CC CXX AR RANLIB LD NM'):
  value = os.environ.get(key)
  if value != None:
    base_env[key] = value
for key in Split('CFLAGS CCFLAGS CPPPATH LIBPATH'):
  value = os.environ.get(key)
  if value != None:
    base_env[key] = Split(value)

base_env['CPPFLAGS'] = ['-D__STDC_FORMAT_MACROS']

# Fix issue with scons not passing some vars through the environment.
for key in Split('PKG_CONFIG_LIBDIR PKG_CONFIG_PATH SYSROOT'):
  if os.environ.has_key(key):
    base_env['ENV'][key] = os.environ[key]

# Build example program
base_env.Append(LIBS=Split('Xss'))
base_env.Append(CPPPATH=['..'])
xidle = base_env.Library('xidle', ['xidle.cc'])
xidle_example = base_env.Program('xidle-example', ['xidle-example.cc', xidle])
Default(xidle_example)

# Build unit tests
test_env = base_env.Clone()
test_env.Append(LIBS=['gtest'])
test_env.Program('xidle_unittest', ['testrunner.cc', 'xidle_unittest.cc', xidle])
tests = []
