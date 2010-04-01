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
for key in Split('CC CXX AR RANLIB LD NM'):
  value = os.environ.get(key)
  if value is not None:
    base_env[key] = value
for key in Split('CFLAGS CPPFLAGS CCFLAGS CPPPATH LIBPATH'):
  value = os.environ.get(key, '')
  base_env[key] = Split(value)

base_env['CFLAGS'].extend(Split('-Wall -Wextra -Werror -Wuninitialized'))
base_env['CPPFLAGS'].append('-D__STDC_FORMAT_MACROS')

# Fix issue with scons not passing some vars through the environment.
for key in Split('PKG_CONFIG_LIBDIR PKG_CONFIG_PATH SYSROOT'):
  if key in os.environ:
    base_env['ENV'][key] = os.environ[key]
base_env.Append(CPPPATH=['..'])

gdk_env = base_env.Clone()
gdk_env.ParseConfig('pkg-config --cflags --libs gdk-2.0')

# Build example xidle program
xidle_env = gdk_env.Clone()
xidle_env.Append(LIBS=Split('base rt X11 Xext'))
xidle = xidle_env.Library('xidle', ['xidle.cc'])
xidle_example = xidle_env.Program('xidle-example', ['xidle-example.cc', xidle])
Default(xidle_example)

# Build backlight tool
backlight = gdk_env.Library('backlight', ['backlight.cc'])
backlight_env = base_env.Clone()
backlight_env.Append(LIBS=Split('base rt gflags'))
backlight_tool = backlight_env.Program('backlight-tool',
                                       ['backlight-tool.cc', backlight])
Default(backlight_tool)

# Build power daemon
idle_dimmer = base_env.Library('idle_dimmer', ['idle_dimmer.cc'])
powerd_env = xidle_env.Clone()
powerd_env.Append(LIBS=Split('gflags'))
powerd = powerd_env.Program('powerd',
                            ['powerd.cc', xidle, backlight, idle_dimmer])
Default(powerd)

# Build unit tests
tests = []
testrunner = base_env.Library('testrunner', ['testrunner.cc'])
xidle_unittest_env = xidle_env.Clone()
xidle_unittest_env.Append(LIBS=['gtest', 'Xtst'])
deps = [testrunner, 'xidle_unittest.cc', xidle]
tests.append(xidle_unittest_env.Program('xidle_unittest', deps))
idle_dimmer_unittest_env = gdk_env.Clone()
idle_dimmer_unittest_env.Append(LIBS=Split('base rt gtest gmock'))
deps = [testrunner, 'idle_dimmer_unittest.cc', idle_dimmer]
tests.append(idle_dimmer_unittest_env.Program('idle_dimmer_unittest', deps))
AlwaysBuild(base_env.Alias('tests', tests))

